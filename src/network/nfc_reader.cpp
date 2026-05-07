/**
 * @file nfc_reader.cpp
 * @brief RC522 NFC 读卡器驱动封装
 *
 * 特性：
 *   - 基于 SPI 总线的 MFRC522 库
 *   - 内置防抖：同一 UID 在 NFC_DEBOUNCE_MS 内只回调一次
 *   - 非阻塞轮询：通过 nfc_reader_loop() 在主循环中调用
 *   - 回调驱动：刷卡后调用 nfc_uid_callback_t 传递 UID 字符串
 *
 * 依赖：
 *   platformio.ini lib_deps 中需包含:
 *     miguelbalboa/MFRC522@^1.4.11
 */

#include "nfc_reader.h"
#include <SPI.h>
#include <MFRC522.h>

// ── 内部状态 ─────────────────────────────────────────────────
static MFRC522            _mfrc522(NFC_SS_PIN, NFC_RST_PIN);
static nfc_uid_callback_t _uid_callback = nullptr;
static String             _last_uid     = "";
static unsigned long      _last_read_ms = 0;

// ── 公共 API ─────────────────────────────────────────────────

bool nfc_reader_begin(nfc_uid_callback_t on_card_read) {
    _uid_callback = on_card_read;

    // 初始化专用 SPI 总线
    SPI.begin(NFC_SCK_PIN, NFC_MISO_PIN, NFC_MOSI_PIN, NFC_SS_PIN);
    _mfrc522.PCD_Init();
    delay(10);

    uint8_t ver = _mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    bool ok = (ver == 0x91 || ver == 0x92);

    Serial.printf("[NFC] RC522 固件版本: 0x%02X %s\n",
                  ver, ok ? "✓ 正常" : "⚠ 异常，请检查接线");

    if (!ok) {
        // 版本为 0x00 或 0xFF 通常表示 SPI 通信失败
        Serial.println("[NFC] 提示：若固件版本为 0x00/0xFF，请检查：");
        Serial.println("       1. SPI 引脚是否与 nfc_reader.h 中定义一致");
        Serial.println("       2. VCC 是否接 3.3V（不可接 5V）");
        Serial.println("       3. GND 是否可靠连接");
    }

    return ok;
}

void nfc_reader_loop() {
    // 检查是否有新卡靠近
    if (!_mfrc522.PICC_IsNewCardPresent()) return;
    if (!_mfrc522.PICC_ReadCardSerial())   return;

    // 拼接 UID 字符串（大写十六进制）
    String uid = "";
    for (byte i = 0; i < _mfrc522.uid.size; i++) {
        if (_mfrc522.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(_mfrc522.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    // 停止与卡片的通信（为下次检测做准备）
    _mfrc522.PICC_HaltA();
    _mfrc522.PCD_StopCrypto1();

    // 防抖：同一张卡在冷却时间内不重复触发
    unsigned long now = millis();
    if (uid == _last_uid && (now - _last_read_ms) < NFC_DEBOUNCE_MS) {
        return;
    }
    _last_uid     = uid;
    _last_read_ms = now;

    Serial.printf("[NFC] ✓ 读卡成功 | UID: %s\n", uid.c_str());

    // 触发上层回调
    if (_uid_callback) {
        _uid_callback(uid);
    }
}

uint8_t nfc_reader_firmware_version() {
    return _mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
}
