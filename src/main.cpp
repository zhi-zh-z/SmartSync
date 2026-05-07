/**
 * @file main.cpp
 * @brief SmartSync 智环引诊终端 —— 主程序
 * @version 3.0
 *
 * 功能模块：
 *   ① Wi-Fi  ：首次上电启动 AP 配网热点，手机扫码填写凭据后自动保存并重连；
 *               已有凭据时直接连接，断线自动重连（wifi_manager）
 *   ② NFC 刷卡：RC522 非阻塞轮询，内置防抖，刷卡后回调切换导航状态（nfc_reader）
 *   ③ 语音识别：点击「语音咨询」按钮开始录音，点击「停止识别」上传讯飞 ASR（voice_asr）
 *   ④ LVGL UI ：三态切换（待机 / 导航 / 语音咨询）
 *
 * 引脚定义（如需修改，请编辑各模块头文件中的宏）：
 *   RC522  : RST=28  SS=29  SCK=32  MOSI=33  MISO=34  → nfc_reader.h
 *   麦克风 : CLK=38  DATA=39（PDM 单声道）             → voice_asr.h
 *   触摸屏 : 见 pins_config.h
 *   屏幕   : MIPI-DSI（由 lvgl_sw_rotation() 驱动）
 *
 * Wi-Fi 配网说明：
 *   首次上电（或凭据被清除后）：
 *     → 设备启动 AP 热点 "SmartGuide_Config"（密码 12345678）
 *     → 手机连接热点后，浏览器访问 http://192.168.4.1/
 *     → 填写目标 Wi-Fi SSID 和密码，提交后设备自动重启连接
 *   换网络：
 *     → 连接设备 IP 访问 http://<设备IP>/reset 清除凭据，重新配网
 */

#include <Arduino.h>
#include "lvgl.h"
#include "pins_config.h"
#include "lvgl_port_v9.h"
#include "ui/ui.h"
#include "network/wifi_manager.h"
#include "network/nfc_reader.h"
#include "voice/voice_asr.h"

// ======================================================
//  外部 C 函数声明（由厂商驱动提供）
// ======================================================
#ifdef __cplusplus
extern "C" {
#endif
void lvgl_sw_rotation(void);
void my_trace(const char* msg);
#ifdef __cplusplus
}
#endif

void my_trace(const char* msg) {
    Serial.println(msg);
    Serial.flush();
}

// ======================================================
//  NFC 刷卡回调
//  nfc_reader 检测到卡片后调用此函数（已内置防抖）
// ======================================================
static void on_card_read(const String& uid) {
    // ------- 示例：静态匹配（开发阶段使用）-------
    // 实际项目请在此发 HTTP GET 请求查询后端：
    //   http://<后端IP>/api/patient?uid=<uid>
    //   → 解析 JSON 获取 patient_name / direction / destination
    const char* direction   = "向右";
    const char* destination = "心电图室";

    Serial.printf("[NFC 回调] UID=%s → 目的地: %s\n", uid.c_str(), destination);

    if (lvgl_port_lock(-1)) {
        ui_set_nav_info(direction, destination);
        ui_set_state_navigating();
        lvgl_port_unlock();
    }
}

// ======================================================
//  语音识别回调
//  ASR 识别完成后调用此函数
// ======================================================
static void on_asr_result(const String& text) {
    Serial.printf("[ASR 回调] 识别结果: \"%s\"\n", text.c_str());

    if (lvgl_port_lock(-1)) {
        ui_set_asr_result(text.length() > 0
                          ? text.c_str()
                          : "（未识别到内容，请重试）");
        lvgl_port_unlock();
    }
}

// ======================================================
//  setup()
// ======================================================
void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(2000);

    Serial.println("\n\n=======================================");
    Serial.println("SmartSync 智环引诊终端 v3.0 启动...");
    Serial.printf("PSRAM: %d Bytes / 空闲堆: %d Bytes\n",
                  ESP.getPsramSize(), ESP.getFreeHeap());
    Serial.println("=======================================");

    // ── ① 初始化屏幕 + LVGL ──────────────────────────────
    Serial.println("[INIT] ① 初始化屏幕驱动...");
    lvgl_sw_rotation();

    Serial.println("[INIT] ② 绘制 UI...");
    if (lvgl_port_lock(-1)) {
        ui_init();
        lvgl_port_unlock();
    }

    // ── ② Wi-Fi 配网 / 连接 ──────────────────────────────
    // wifi_manager_begin() 内部逻辑：
    //   有凭据 → 尝试 STA 连接 → 成功返回 true
    //   无凭据 / 连接失败 → 启动 AP 配网热点 → 返回 false
    // 无论成功与否，loop() 中都需调用 wifi_manager_loop()
    Serial.println("[INIT] ③ 初始化 Wi-Fi...");
    bool wifi_ok = wifi_manager_begin();
    if (wifi_ok) {
        // 同步 NTP 时间（讯飞 ASR 签名需要准确时间戳）
        configTime(8 * 3600, 0, "pool.ntp.org", "time.aliyun.com");
        Serial.println("[INIT] NTP 时间同步已启动");
    } else {
        // AP 配网模式，Wi-Fi 功能暂不可用
        // 提示信息已在 wifi_manager_begin() 内部打印
    }

    // ── ③ NFC 读卡器 ─────────────────────────────────────
    Serial.println("[INIT] ④ 初始化 NFC 读卡器...");
    nfc_reader_begin(on_card_read);
    // 初始化失败不致命，loop 中继续轮询（接线修正后自动恢复）

    // ── ④ 麦克风 / 语音识别 ──────────────────────────────
    Serial.println("[INIT] ⑤ 初始化麦克风...");
    if (asr_init()) {
        asr_set_result_callback(on_asr_result);
        ui_set_voice_callbacks(asr_start_recording,
                               asr_stop_and_recognize,
                               asr_is_recording);
        Serial.println("[INIT] 麦克风初始化成功");
    } else {
        Serial.println("[INIT] ⚠ 麦克风初始化失败，语音功能不可用");
    }

    Serial.println("[INIT] ★ 所有模块初始化完毕，进入主循环！★");
}

// ======================================================
//  loop()
// ======================================================
void loop() {
    // ① Wi-Fi 维护 / AP 配网 HTTP 服务（必须每帧调用）
    wifi_manager_loop();

    // ② NFC 轮询（内部含防抖，可高频调用）
    nfc_reader_loop();

    // ③ ASR 状态机（录音采集 + 识别请求）
    asr_loop();

    delay(10);
}