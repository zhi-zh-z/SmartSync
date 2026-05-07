/**
 * @file wifi_manager.cpp
 * @brief Wi-Fi 管理器实现（AP 网页配网 + NVS 凭据持久化 + 断线自动重连）
 *
 * 配网流程：
 *   首次使用（无凭据）：
 *     → 设备启动 AP 热点 "SmartGuide_Config"（密码 12345678）
 *     → 用手机连接热点，浏览器访问 http://192.168.4.1/
 *     → 填写目标 Wi-Fi 的 SSID 和密码，点击"连接"
 *     → 凭据保存到 NVS，设备自动重启，重启后自动连接目标 Wi-Fi
 *
 *   已有凭据：
 *     → 启动时直接读取 NVS，尝试连接
 *     → 连接失败则清除凭据并重新进入 AP 配网模式
 *
 *   重置 Wi-Fi（需要换网络）：
 *     → 调用 wifi_manager_clear_credentials() 后重启
 *     → 或在 STA 已连状态下访问 http://<设备IP>/reset
 */

#include "wifi_manager.h"

// ── 内部静态变量 ──────────────────────────────────────────────
static Preferences  _prefs;
static WebServer    _server(80);
static bool         _ap_mode         = false;
static bool         _sta_connected   = false;
static unsigned long _last_check_ms  = 0;

// ── 凭据存取 ─────────────────────────────────────────────────

static void _save_credentials(const String& ssid, const String& password) {
    _prefs.begin("wifi", false);
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", password);
    _prefs.end();
    Serial.println("[WiFi] ✓ 凭据已保存到 NVS");
}

static bool _load_credentials(String& ssid, String& password) {
    _prefs.begin("wifi", true);
    ssid     = _prefs.getString("ssid", "");
    password = _prefs.getString("pass", "");
    _prefs.end();
    return (ssid.length() > 0 && password.length() > 0);
}

void wifi_manager_clear_credentials() {
    _prefs.begin("wifi", false);
    _prefs.clear();
    _prefs.end();
    Serial.println("[WiFi] 凭据已清除，重启后将进入配网模式");
}

// ── STA 连接 ─────────────────────────────────────────────────

static bool _connect_sta(const String& ssid, const String& password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.printf("[WiFi] 正在连接: %s ", ssid.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("\n[WiFi] ✗ 连接超时");
            return false;
        }
        delay(200);
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] ✓ 连接成功！IP: %s\n",
                  WiFi.localIP().toString().c_str());
    return true;
}

// ── AP 配网 Web 服务器路由 ────────────────────────────────────

// 配网主页（样式稍作美化，中文友好）
static const char PROV_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang='zh-CN'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>SmartGuide WiFi 配置</title>
  <style>
    body{font-family:sans-serif;background:#f0f4ff;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
    .card{background:#fff;border-radius:16px;padding:32px 28px;width:320px;box-shadow:0 4px 24px rgba(0,0,0,.12)}
    h2{color:#1a4f8b;margin-bottom:24px;font-size:1.3em;text-align:center}
    label{display:block;font-size:.85em;color:#555;margin-bottom:4px}
    input{width:100%;box-sizing:border-box;padding:10px 12px;border:1.5px solid #c9e4f5;border-radius:8px;font-size:1em;margin-bottom:16px;outline:none}
    input:focus{border-color:#1a4f8b}
    button{width:100%;padding:12px;background:#1a4f8b;color:#fff;border:none;border-radius:8px;font-size:1em;cursor:pointer}
    button:active{background:#0f3360}
    .tip{font-size:.78em;color:#888;text-align:center;margin-top:16px}
  </style>
</head>
<body>
  <div class='card'>
    <h2>🏥 智能导诊设备<br>Wi-Fi 配置</h2>
    <form action='/connect' method='POST'>
      <label>Wi-Fi 名称 (SSID)</label>
      <input type='text' name='ssid' placeholder='请输入网络名称' required>
      <label>密码</label>
      <input type='password' name='pass' placeholder='请输入密码'>
      <button type='submit'>保存并连接</button>
    </form>
    <p class='tip'>连接成功后设备将自动重启</p>
  </div>
</body>
</html>
)rawhtml";

static void _setup_ap_server() {
    // 主配网页
    _server.on("/", HTTP_GET, []() {
        _server.send_P(200, "text/html; charset=utf-8", PROV_HTML);
    });

    // 提交凭据
    _server.on("/connect", HTTP_POST, []() {
        String ssid = _server.arg("ssid");
        String pass = _server.arg("pass");

        if (ssid.length() == 0) {
            _server.send(400, "text/html; charset=utf-8",
                         "<p style='font-family:sans-serif;color:red'>SSID 不能为空！"
                         "<br><a href='/'>返回</a></p>");
            return;
        }

        _save_credentials(ssid, pass);
        _server.send(200, "text/html; charset=utf-8",
                     "<p style='font-family:sans-serif;color:green;text-align:center'>"
                     "✓ 凭据已保存，设备正在重启……<br>"
                     "<small>请重新连接目标 Wi-Fi 后访问设备 IP</small></p>");
        delay(1500);
        ESP.restart();
    });

    // /reset 路由：清除凭据并重新配网
    _server.on("/reset", HTTP_GET, []() {
        wifi_manager_clear_credentials();
        _server.send(200, "text/html; charset=utf-8",
                     "<p style='font-family:sans-serif;text-align:center'>"
                     "凭据已清除，设备即将重启进入配网模式……</p>");
        delay(1500);
        ESP.restart();
    });

    // STA 模式下也挂载 /reset，方便换网络
    _server.onNotFound([]() {
        _server.send(404, "text/plain", "Not Found");
    });

    _server.begin();
}

static void _start_ap_mode() {
    _ap_mode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

    Serial.println("[WiFi] ★ 进入 AP 配网模式 ★");
    Serial.printf("[WiFi]   热点名: %s\n", WIFI_AP_SSID);
    Serial.printf("[WiFi]   密  码: %s\n", WIFI_AP_PASSWORD);
    Serial.printf("[WiFi]   配网页: http://%s/\n",
                  WiFi.softAPIP().toString().c_str());

    _setup_ap_server();
}

// ── 公共 API ─────────────────────────────────────────────────

bool wifi_manager_begin() {
    String ssid, password;
    if (_load_credentials(ssid, password)) {
        Serial.printf("[WiFi] 读取到已保存凭据，尝试连接 \"%s\"...\n", ssid.c_str());
        if (_connect_sta(ssid, password)) {
            _sta_connected = true;
            _ap_mode       = false;
            // 同时挂载 /reset 供 STA 模式访问
            _setup_ap_server();
            return true;
        }
        // 密码错误或热点消失 → 清除凭据，进入配网
        Serial.println("[WiFi] ⚠ 连接失败，清除旧凭据，进入配网模式...");
        wifi_manager_clear_credentials();
    } else {
        Serial.println("[WiFi] 无已保存凭据，直接进入配网模式...");
    }

    _start_ap_mode();
    return false;  // AP 模式
}

void wifi_manager_loop() {
    // 无论 AP/STA 都需要处理 HTTP 请求
    _server.handleClient();

    if (_ap_mode) return;  // AP 模式下不做重连检查

    // STA 模式：每 5 秒检查连接状态
    unsigned long now = millis();
    if (now - _last_check_ms < 5000) return;
    _last_check_ms = now;

    if (WiFi.status() != WL_CONNECTED) {
        _sta_connected = false;
        Serial.println("[WiFi] ⚠ 连接断开，尝试重连...");
        String ssid, password;
        if (_load_credentials(ssid, password)) {
            if (_connect_sta(ssid, password)) {
                _sta_connected = true;
            }
        }
    }
}

bool wifi_manager_is_connected() {
    return (!_ap_mode) && (WiFi.status() == WL_CONNECTED);
}

String wifi_manager_local_ip() {
    if (!wifi_manager_is_connected()) return "";
    return WiFi.localIP().toString();
}
