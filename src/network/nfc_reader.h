#pragma once

#include <Arduino.h>

// ============================================================
//  RC522 NFC 读卡器引脚配置
//  根据你的实际接线在此修改
// ============================================================
#define NFC_RST_PIN   28   // RC522 RST → GPIO28
#define NFC_SS_PIN    29   // RC522 SDA(CS) → GPIO29
#define NFC_SCK_PIN   32   // RC522 SCK → GPIO32
#define NFC_MOSI_PIN  33   // RC522 MOSI → GPIO33
#define NFC_MISO_PIN  34   // RC522 MISO → GPIO34

// 防重复读取冷却时间（毫秒）
// 同一张卡在此时间内只触发一次回调，防止连续触发
#define NFC_DEBOUNCE_MS 2000

/**
 * @brief 刷卡结果回调类型
 * @param uid  读取到的 UID 字符串（大写十六进制，如 "A3B2C1D0"）
 */
typedef void (*nfc_uid_callback_t)(const String& uid);

/**
 * @brief 初始化 SPI 总线和 RC522 读卡器
 * @param on_card_read  检测到卡片时调用的回调函数
 * @return true   初始化成功（固件版本正常）
 * @return false  初始化失败（请检查接线）
 */
bool nfc_reader_begin(nfc_uid_callback_t on_card_read);

/**
 * @brief 必须在 loop() 中周期调用，轮询检测卡片
 */
void nfc_reader_loop();

/**
 * @brief 返回 RC522 固件版本字节（0x91 / 0x92 = 正常）
 */
uint8_t nfc_reader_firmware_version();
