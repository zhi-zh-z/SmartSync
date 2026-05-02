#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "lvgl.h"
#include "pins_config.h"
#include "lvgl_port_v9.h"
#include "ui/ui.h"

#define NFC_RST_PIN   28
#define NFC_SS_PIN    29
#define NFC_SCK_PIN   32
#define NFC_MOSI_PIN  33
#define NFC_MISO_PIN  34

MFRC522 mfrc522(NFC_SS_PIN, NFC_RST_PIN);
lv_obj_t * main_label;

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

void setup() {
    Serial.begin(115200);
    // 【极其关键】强行把 ESP32 原生的底层 C 语言报错输出到 VS Code 终端！
    Serial.setDebugOutput(true);
    delay(2000); 

    Serial.println("\n\n=======================================");
    Serial.println("SmartSync Embedded OS v1.0 Starting...");
    Serial.printf("PSRAM Size: %d Bytes\n", ESP.getPsramSize());
    Serial.println("=======================================");

    Serial.println("[TRACE] STEP 1: 准备初始化 MIPI DSI 屏幕驱动...");
    delay(100);

    // 调用厂家底层驱动
    lvgl_sw_rotation(); 

    Serial.println("[TRACE] STEP 2: MIPI DSI 初始化成功！");
    delay(100);

    Serial.println("[TRACE] STEP 3: 准备绘制 UI...");
    if (lvgl_port_lock(-1)) {
        ui_init(); // 调用我们自己写的 UI 框架
        lvgl_port_unlock();
    }

    Serial.println("[TRACE] STEP 5: 准备初始化 NFC...");
    SPI.begin(NFC_SCK_PIN, NFC_MISO_PIN, NFC_MOSI_PIN, NFC_SS_PIN);
    mfrc522.PCD_Init();
    
    Serial.println("[TRACE] ★ 所有模块初始化完毕，进入主循环！ ★");
}

void loop() {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        String uid = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            uid += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
            uid += String(mfrc522.uid.uidByte[i], HEX);
        }
        uid.toUpperCase();
        Serial.printf("\n[NFC] 扫码成功! UID: %s\n", uid.c_str());
        if (lvgl_port_lock(-1)) {
            // 这里替换为新的 API
            ui_set_nav_info("←", "内科急诊室");
            lvgl_port_unlock();
        }
        mfrc522.PICC_HaltA();
    }
    delay(20); 
}