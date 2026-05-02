#pragma once

#include "lvgl.h"

// 在此函数内完成所有屏幕和组件的初始化及样式设置。
void ui_init();

// 开放给主程序的接口，用于更新左侧大箭头和右侧当前位置的文字。
void ui_set_nav_info(const char* direction, const char* station_name);
