#pragma once

#include <Arduino.h>

// ============================================================
//  【用户配置区】在此填写语音识别服务商的 AppID/Key
//  本模块默认使用【讯飞开放平台】语音听写（HTTP方式）
//  注册地址：https://www.xfyun.cn/
//  申请"语音听写（流式版）"服务后可在控制台获得以下三个参数
// ============================================================
#define ASR_APPID    "your_xfyun_appid"      // 替换为你的 AppID
#define ASR_API_KEY  "your_xfyun_api_key"    // 替换为你的 API Key
#define ASR_API_SEC  "your_xfyun_api_secret" // 替换为你的 API Secret

// 若你使用【百度 AI】短语音识别（REST API），请切换以下宏并填写参数
// #define ASR_PROVIDER_BAIDU
// #define BAIDU_APP_ID    "xxx"
// #define BAIDU_API_KEY   "xxx"
// #define BAIDU_SECRET    "xxx"

// PDM/I2S 麦克风引脚（ESP32-P4 Function EV Board 板载麦克风）
// 若你的板子引脚不同，在此修改
#define ASR_MIC_CLK   38   // PDM 时钟引脚（PDMCLK）
#define ASR_MIC_DATA  39   // PDM 数据引脚（PDMDATA）

// 录音采样率（讯飞要求 16000Hz）
#define ASR_SAMPLE_RATE 16000
// 单次录音最大时长（毫秒）
#define ASR_MAX_DURATION_MS 8000
// PCM 缓冲区大小（字节）：8s × 16kHz × 16bit × 单声道 = 256KB
#define ASR_BUF_SIZE (ASR_SAMPLE_RATE * (ASR_MAX_DURATION_MS / 1000) * 2)

/**
 * @brief 初始化 PDM/I2S 麦克风驱动
 * @return true 初始化成功
 */
bool asr_init();

/**
 * @brief 开始录音（非阻塞，后台录音）
 *        录音进行中调用 asr_is_recording() 可查询状态
 */
void asr_start_recording();

/**
 * @brief 停止录音，并立即上传音频到 ASR 服务
 *        结果通过回调 asr_set_result_callback() 异步返回
 */
void asr_stop_and_recognize();

/**
 * @brief 检查当前是否正在录音
 */
bool asr_is_recording();

/**
 * @brief 注册识别结果回调函数
 *        识别完成后，框架会以识别到的文本字符串调用此回调
 *        若识别失败，text 为空字符串 ""
 * @param callback  void callback(const String& text)
 */
typedef void (*asr_result_cb_t)(const String& text);
void asr_set_result_callback(asr_result_cb_t callback);

/**
 * @brief 必须在 loop() 中周期调用，驱动内部状态机
 */
void asr_loop();
