/**
 * @file voice_asr.cpp
 * @brief 语音识别模块实现（讯飞开放平台 REST API）
 *
 * 工作流程：
 *   1. asr_start_recording() → 驱动 PDM/I2S 麦克风录制 PCM 音频到 PSRAM 缓冲区
 *   2. asr_stop_and_recognize() → 停止录音，将 PCM 数据 Base64 编码后通过 HTTPS POST
 *      发送到讯飞语音听写接口，解析 JSON 返回识别结果文本
 *   3. 识别完成后调用用户注册的回调函数
 *
 * 讯飞 API 参考：
 *   https://www.xfyun.cn/doc/asr/voicedictation/API.html
 *
 * 依赖库（platformio.ini lib_deps）：
 *   - ArduinoJson
 *   - ESP32 自带 WiFiClientSecure / HTTPClient
 *   - ESP32 自带 driver/i2s
 */

#include "voice_asr.h"
#include "wifi_manager.h"
#include <driver/i2s.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>      // HMAC-SHA256 签名
#include <mbedtls/base64.h>  // Base64 编码

// ======================================================
//  内部状态
// ======================================================
static bool             _is_recording   = false;
static int16_t*         _pcm_buf        = nullptr;  // PSRAM 中的 PCM 缓冲区
static size_t           _pcm_samples    = 0;         // 已录制的采样数
static asr_result_cb_t  _result_cb      = nullptr;
static bool             _need_recognize = false;     // 停录后触发识别标志

// I2S 端口号（确保不与显示/音频冲突，此处使用 I2S_NUM_1）
static const i2s_port_t MIC_I2S_PORT = I2S_NUM_1;

// ======================================================
//  工具函数：Base64 编码
// ======================================================
static String base64_encode_buf(const uint8_t* data, size_t len) {
    size_t out_len = 0;
    // 计算输出大小（mbedtls 要求 +1 for null terminator）
    mbedtls_base64_encode(nullptr, 0, &out_len, data, len);
    String result;
    result.reserve(out_len);
    uint8_t* buf = (uint8_t*)malloc(out_len + 1);
    if (!buf) return "";
    mbedtls_base64_encode(buf, out_len + 1, &out_len, data, len);
    buf[out_len] = '\0';
    result = String((char*)buf);
    free(buf);
    return result;
}

// ======================================================
//  工具函数：HMAC-SHA256 签名
// ======================================================
static String hmac_sha256_base64(const String& key, const String& msg) {
    uint8_t digest[32];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, (const uint8_t*)key.c_str(), key.length());
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)msg.c_str(), msg.length());
    mbedtls_md_hmac_finish(&ctx, digest);
    mbedtls_md_free(&ctx);
    return base64_encode_buf(digest, 32);
}

// ======================================================
//  PDM/I2S 麦克风初始化
// ======================================================
bool asr_init() {
    // 先在 PSRAM 中申请录音缓冲区
    _pcm_buf = (int16_t*)ps_malloc(ASR_BUF_SIZE);
    if (!_pcm_buf) {
        Serial.println("[ASR] ✗ PSRAM 不足，无法分配录音缓冲区！");
        return false;
    }
    memset(_pcm_buf, 0, ASR_BUF_SIZE);

    // 配置 I2S（PDM 输入模式）
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate          = ASR_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 4,
        .dma_buf_len          = 512,
        .use_apll             = false,
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = ASR_MIC_CLK,
        .ws_io_num    = I2S_PIN_NO_CHANGE,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = ASR_MIC_DATA,
    };

    esp_err_t ret = i2s_driver_install(MIC_I2S_PORT, &cfg, 0, NULL);
    if (ret != ESP_OK) {
        Serial.printf("[ASR] ✗ I2S 驱动安装失败: %d\n", ret);
        return false;
    }
    i2s_set_pin(MIC_I2S_PORT, &pins);
    i2s_zero_dma_buffer(MIC_I2S_PORT);

    Serial.println("[ASR] ✓ 麦克风初始化成功");
    return true;
}

// ======================================================
//  注册回调
// ======================================================
void asr_set_result_callback(asr_result_cb_t callback) {
    _result_cb = callback;
}

// ======================================================
//  开始录音
// ======================================================
void asr_start_recording() {
    if (_is_recording) return;
    _pcm_samples = 0;
    memset(_pcm_buf, 0, ASR_BUF_SIZE);
    i2s_start(MIC_I2S_PORT);
    _is_recording   = true;
    _need_recognize = false;
    Serial.println("[ASR] 🎙 开始录音...");
}

// ======================================================
//  停止录音，触发识别
// ======================================================
void asr_stop_and_recognize() {
    if (!_is_recording) return;
    i2s_stop(MIC_I2S_PORT);
    _is_recording   = false;
    _need_recognize = true;
    Serial.printf("[ASR] ⏹ 停止录音，已采集 %u 个采样（%.1f 秒）\n",
                  (unsigned)_pcm_samples,
                  (float)_pcm_samples / ASR_SAMPLE_RATE);
}

bool asr_is_recording() {
    return _is_recording;
}

// ======================================================
//  内部：调用讯飞语音听写 REST API
// ======================================================
static void _do_recognize() {
    if (!wifi_manager_is_connected()) {
        Serial.println("[ASR] ✗ Wi-Fi 未连接，无法识别");
        if (_result_cb) _result_cb("");
        return;
    }
    if (_pcm_samples == 0) {
        Serial.println("[ASR] ✗ 录音内容为空");
        if (_result_cb) _result_cb("");
        return;
    }

    Serial.println("[ASR] 📤 正在上传音频到讯飞 ASR...");

    // ---- 讯飞 REST API 鉴权 ----
    // 请求 URL（语音听写 v2 HTTP 接口）
    // 文档：https://www.xfyun.cn/doc/asr/voicedictation/API.html
    const char* host = "iat-api.xfyun.cn";
    const int   port = 443;
    String path = "/v2/iat";

    // 当前时间戳（需要 NTP 同步，若无则用固定时间戳仅用于测试）
    time_t ts = time(nullptr);
    if (ts < 1000000) ts = 1746460000; // 后备固定值（2026-05-05 UTC）

    // 构造签名字符串：host + date + path
    char date_buf[64];
    struct tm* t = gmtime(&ts);
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", t);

    String sign_origin = "host: ";
    sign_origin += host;
    sign_origin += "\ndate: ";
    sign_origin += date_buf;
    sign_origin += "\nPOST ";
    sign_origin += path;
    sign_origin += " HTTP/1.1";

    String signature = hmac_sha256_base64(ASR_API_SEC, sign_origin);
    String auth_origin = "api_key=\"";
    auth_origin += ASR_API_KEY;
    auth_origin += "\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"";
    auth_origin += signature;
    auth_origin += "\"";
    String authorization = base64_encode_buf((uint8_t*)auth_origin.c_str(), auth_origin.length());

    // ---- 构造 JSON 请求体 ----
    // PCM 数据 Base64 编码
    size_t pcm_bytes = _pcm_samples * sizeof(int16_t);
    String audio_b64 = base64_encode_buf((uint8_t*)_pcm_buf, pcm_bytes);

    // JSON body（讯飞格式）
    JsonDocument doc;
    doc["common"]["app_id"] = ASR_APPID;
    doc["business"]["language"]  = "zh_cn";
    doc["business"]["domain"]    = "iat";
    doc["business"]["accent"]    = "mandarin";
    doc["business"]["format"]    = "audio/L16;rate=16000";
    doc["business"]["encoding"]  = "raw";
    doc["data"]["status"]        = 2; // 2 = 最后一帧（一次性发完）
    doc["data"]["format"]        = "audio/L16;rate=16000";
    doc["data"]["encoding"]      = "raw";
    doc["data"]["audio"]         = audio_b64;

    String body;
    serializeJson(doc, body);

    // ---- HTTPS POST ----
    WiFiClientSecure client;
    client.setInsecure(); // 跳过证书验证（生产环境请使用正确的根证书）

    HTTPClient http;
    String url = "https://";
    url += host;
    url += path;
    url += "?authorization=";
    url += authorization;
    url += "&date=";
    url += date_buf;
    url += "&host=";
    url += host;

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    int code = http.POST(body);
    if (code != 200) {
        Serial.printf("[ASR] ✗ HTTP 错误: %d\n", code);
        if (_result_cb) _result_cb("");
        http.end();
        return;
    }

    // ---- 解析响应 ----
    String resp = http.getString();
    http.end();
    Serial.printf("[ASR] 响应: %s\n", resp.c_str());

    JsonDocument resp_doc;
    DeserializationError err = deserializeJson(resp_doc, resp);
    if (err) {
        Serial.printf("[ASR] ✗ JSON 解析失败: %s\n", err.c_str());
        if (_result_cb) _result_cb("");
        return;
    }

    // 讯飞返回格式：{"code":0,"message":"success","sid":"...","data":{"result":{"ws":[...],...},"status":2}}
    int resp_code = resp_doc["code"].as<int>();
    if (resp_code != 0) {
        Serial.printf("[ASR] ✗ 识别失败，code=%d msg=%s\n",
                      resp_code, resp_doc["message"].as<const char*>());
        if (_result_cb) _result_cb("");
        return;
    }

    // 拼接所有词语
    String result_text = "";
    JsonArray ws = resp_doc["data"]["result"]["ws"].as<JsonArray>();
    for (JsonObject word_obj : ws) {
        JsonArray cw = word_obj["cw"].as<JsonArray>();
        for (JsonObject char_obj : cw) {
            result_text += char_obj["w"].as<const char*>();
        }
    }

    Serial.printf("[ASR] ✓ 识别结果: %s\n", result_text.c_str());
    if (_result_cb) _result_cb(result_text);
}

// ======================================================
//  loop() 驱动函数
// ======================================================
void asr_loop() {
    // 录音中：从 I2S 持续读取 PCM 数据
    if (_is_recording) {
        size_t max_samples = ASR_BUF_SIZE / sizeof(int16_t);
        if (_pcm_samples < max_samples) {
            size_t to_read = min((size_t)512, max_samples - _pcm_samples);
            size_t bytes_read = 0;
            i2s_read(MIC_I2S_PORT,
                     _pcm_buf + _pcm_samples,
                     to_read * sizeof(int16_t),
                     &bytes_read,
                     10 / portTICK_PERIOD_MS);
            _pcm_samples += bytes_read / sizeof(int16_t);
        } else {
            // 缓冲区满，自动停止
            Serial.println("[ASR] ⚠ 录音已达最大时长，自动停止");
            asr_stop_and_recognize();
        }
        return;
    }

    // 需要识别
    if (_need_recognize) {
        _need_recognize = false;
        _do_recognize();
    }
}
