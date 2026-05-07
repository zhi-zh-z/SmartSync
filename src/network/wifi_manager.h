#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>

// ============================================================
//  AP 配网热点参数（固定，用户用手机连此热点进行配置）
// ============================================================
#define WIFI_AP_SSID     "SmartGuide_Config"  // 配网热点名称
#define WIFI_AP_PASSWORD "12345678"           // 配网热点密码（至少8位）

// STA 模式连接超时（毫秒）
#define WIFI_CONNECT_TIMEOUT_MS 10000

/**
 * @brief Wi-Fi 管理器初始化
 *
 * 执行逻辑：
 *   1. 尝试从 NVS 读取已保存的 SSID/密码并连接
 *   2. 若连接成功则返回 true，同时后台运行 HTTP WebServer 处理 /reset 请求
 *   3. 若连接失败（无凭据或密码错误），启动 AP 配网热点 + Web 配网页面，
 *      用户手机连接热点后访问 http://192.168.4.1/ 填写 SSID/密码提交，
 *      设备自动保存并重启连接
 *   注意：进入 AP 模式时此函数不会返回，需在 loop() 中调用 wifi_manager_loop()
 *
 * @return true  STA 模式已连接
 * @return false 进入 AP 配网模式（loop 中需持续调用 wifi_manager_loop()）
 */
bool wifi_manager_begin();

/**
 * @brief 必须在 loop() 中调用
 *        - AP 模式：处理 Web 配网 HTTP 请求
 *        - STA 模式：每 5 秒检测断线并自动重连
 */
void wifi_manager_loop();

/**
 * @brief 当前是否处于 STA 已连接状态
 */
bool wifi_manager_is_connected();

/**
 * @brief 返回当前 IP（STA 模式）；未连接时返回空字符串
 */
String wifi_manager_local_ip();

/**
 * @brief 清除 NVS 中保存的 Wi-Fi 凭据，重启后将重新进入配网模式
 */
void wifi_manager_clear_credentials();
