#pragma once

#include "lvgl.h"

LV_FONT_DECLARE(ui_font_source_han_20);
LV_FONT_DECLARE(ui_font_source_han_32);
LV_FONT_DECLARE(ui_font_source_han_40);

// ── 初始化 ──────────────────────────────────────────────────
// 在此函数内完成所有屏幕和组件的初始化及样式设置。
void ui_init();

// ── 状态切换 ────────────────────────────────────────────────
void ui_set_state_idle(void);
void ui_set_state_navigating(void);
void ui_set_state_voice(void);

// ── 数据更新接口 ─────────────────────────────────────────────
// 更新左侧地图导航路线与右侧导航信息（方向/目的地）
void ui_set_nav_info(const char* direction, const char* destination);

// 更新语音界面对话框中显示的 ASR 识别结果文本
void ui_set_asr_result(const char* text);

// ── 语音功能回调注册 ─────────────────────────────────────────
// 由 main.cpp 在 setup() 中调用，将麦克风控制函数注入到 UI 按钮事件
typedef void     (*voice_start_fn)(void);
typedef void     (*voice_stop_fn)(void);
typedef bool     (*voice_is_rec_fn)(void);
void ui_set_voice_callbacks(voice_start_fn start_cb,
                             voice_stop_fn  stop_cb,
                             voice_is_rec_fn is_rec_cb);
