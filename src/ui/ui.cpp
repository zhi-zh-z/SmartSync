#include "ui.h"
#include <stdio.h>

static lv_obj_t *main_screen;
static lv_obj_t *left_panel;
static lv_obj_t *right_panel;
static lv_obj_t *bottom_panel;

// Variables for arrow animation
static lv_obj_t *arrow_line;
static lv_obj_t *arrow_line2;
static lv_obj_t *dest_label;

// Draw a giant tech arrow
static lv_point_precise_t arrow_pts_1[] = {
    {300, 350}, {400, 200}, {500, 350}
};
static lv_point_precise_t arrow_pts_2[] = {
    {350, 350}, {350, 500}, {450, 500}, {450, 350}
};

static void arrow_anim_cb(void * var, int32_t v)
{
    lv_obj_t * obj = (lv_obj_t *)var;
    lv_obj_set_y(obj, v);
}

void ui_init()
{
    // 主屏幕背景
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0xF2F7FA), 0); // Light blueish white
    lv_obj_set_style_pad_all(main_screen, 0, 0);
    lv_obj_set_style_border_width(main_screen, 0, 0);
    
    // 主容器：上下分层
    lv_obj_t * main_col = lv_obj_create(main_screen);
    lv_obj_set_size(main_col, 1280, 800);
    lv_obj_set_flex_flow(main_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(main_col, 10, 0);
    lv_obj_set_style_pad_row(main_col, 10, 0);
    lv_obj_set_style_bg_opa(main_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_col, 0, 0);

    // ==========================================
    // 上半部分：左右分层 (占满剩余空间)
    // ==========================================
    lv_obj_t * top_row = lv_obj_create(main_col);
    lv_obj_set_width(top_row, lv_pct(100));
    lv_obj_set_flex_grow(top_row, 1);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(top_row, 0, 0);
    lv_obj_set_style_pad_column(top_row, 10, 0);
    lv_obj_set_style_bg_opa(top_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_row, 0, 0);

    // ---- 左侧：动态巨型箭头 ----
    left_panel = lv_obj_create(top_row);
    lv_obj_set_width(left_panel, lv_pct(50));
    lv_obj_set_height(left_panel, lv_pct(100));
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x0055A4), 0); // 医疗深蓝
    lv_obj_set_style_radius(left_panel, 20, 0);
    lv_obj_set_style_border_width(left_panel, 0, 0);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 箭头元素 1: 尖端
    arrow_line = lv_line_create(left_panel);
    lv_line_set_points(arrow_line, arrow_pts_1, 3);
    lv_obj_set_style_line_width(arrow_line, 40, 0);
    lv_obj_set_style_line_color(arrow_line, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_rounded(arrow_line, true, 0);
    
    // 箭头元素 2: 尾部
    arrow_line2 = lv_line_create(left_panel);
    lv_line_set_points(arrow_line2, arrow_pts_2, 4);
    lv_obj_set_style_line_width(arrow_line2, 40, 0);
    lv_obj_set_style_line_color(arrow_line2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_rounded(arrow_line2, true, 0);

    // 目的地文本
    dest_label = lv_label_create(left_panel);
    lv_label_set_text(dest_label, "WAITING FOR NFC...");
    lv_obj_set_style_text_font(dest_label, &lv_font_montserrat_32, 0); // Use large English font
    lv_obj_set_style_text_color(dest_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(dest_label, LV_ALIGN_TOP_MID, 0, 40);

    // 呼吸动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arrow_line);
    lv_anim_set_values(&a, -20, 20);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, arrow_anim_cb);
    lv_anim_start(&a);

    lv_anim_set_var(&a, arrow_line2);
    lv_anim_start(&a);

    // ---- 右侧：2D 地图抽象层 ----
    right_panel = lv_obj_create(top_row);
    lv_obj_set_width(right_panel, lv_pct(50));
    lv_obj_set_height(right_panel, lv_pct(100));
    lv_obj_set_style_bg_color(right_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(right_panel, 20, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_shadow_width(right_panel, 10, 0);
    lv_obj_set_style_shadow_color(right_panel, lv_color_hex(0xD0E0E8), 0);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 抽象画几个科室 (方块)
    lv_obj_t * room1 = lv_obj_create(right_panel);
    lv_obj_set_size(room1, 200, 150);
    lv_obj_align(room1, LV_ALIGN_TOP_LEFT, 50, 50);
    lv_obj_set_style_bg_color(room1, lv_color_hex(0xE0F0FF), 0);
    
    lv_obj_t * room2 = lv_obj_create(right_panel);
    lv_obj_set_size(room2, 150, 150);
    lv_obj_align(room2, LV_ALIGN_TOP_RIGHT, -50, 50);
    lv_obj_set_style_bg_color(room2, lv_color_hex(0xE0F0FF), 0);

    lv_obj_t * room3 = lv_obj_create(right_panel);
    lv_obj_set_size(room3, 400, 200);
    lv_obj_align(room3, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_color(room3, lv_color_hex(0xE0F0FF), 0);

    // 路径线 (高亮蓝色)
    static lv_point_precise_t path_pts[] = { {200, 500}, {200, 300}, {400, 300}, {400, 100} };
    lv_obj_t * path_line = lv_line_create(right_panel);
    lv_line_set_points(path_line, path_pts, 4);
    lv_obj_set_style_line_width(path_line, 10, 0);
    lv_obj_set_style_line_color(path_line, lv_color_hex(0x00A4FF), 0);
    lv_obj_set_style_line_rounded(path_line, true, 0);

    // 当前位置标记 (发光红点)
    lv_obj_t * current_pos = lv_obj_create(right_panel);
    lv_obj_set_size(current_pos, 30, 30);
    lv_obj_align(current_pos, LV_ALIGN_BOTTOM_LEFT, 185, -135);
    lv_obj_set_style_radius(current_pos, 15, 0);
    lv_obj_set_style_bg_color(current_pos, lv_color_hex(0xFF3333), 0);
    lv_obj_set_style_shadow_color(current_pos, lv_color_hex(0xFF3333), 0);
    lv_obj_set_style_shadow_width(current_pos, 20, 0);

    // ==========================================
    // 下半部分：三个控制按钮
    // ==========================================
    bottom_panel = lv_obj_create(main_col);
    lv_obj_set_width(bottom_panel, lv_pct(100));
    lv_obj_set_height(bottom_panel, 120);
    lv_obj_set_flex_flow(bottom_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(bottom_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_panel, 0, 0);
    lv_obj_clear_flag(bottom_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 声明带中文的字体 (如果开启了 SIMSUN_16_CJK)
    #if LV_FONT_SIMSUN_16_CJK
        extern const lv_font_t lv_font_simsun_16_cjk;
    #endif

    // 按钮 1: 复位
    lv_obj_t * btn_reset = lv_button_create(bottom_panel);
    lv_obj_set_size(btn_reset, 300, 80);
    lv_obj_set_style_bg_color(btn_reset, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(btn_reset, lv_color_hex(0x0055A4), 0);
    lv_obj_set_style_radius(btn_reset, 40, 0);
    lv_obj_t * lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, LV_SYMBOL_REFRESH " RESET");
    lv_obj_center(lbl_reset);

    // 按钮 2: SOS 紧急求助 (红色)
    lv_obj_t * btn_sos = lv_button_create(bottom_panel);
    lv_obj_set_size(btn_sos, 300, 80);
    lv_obj_set_style_bg_color(btn_sos, lv_color_hex(0xFF3333), 0);
    lv_obj_set_style_text_color(btn_sos, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(btn_sos, 40, 0);
    lv_obj_set_style_shadow_color(btn_sos, lv_color_hex(0xFF3333), 0);
    lv_obj_set_style_shadow_width(btn_sos, 30, 0);
    lv_obj_t * lbl_sos = lv_label_create(btn_sos);
    lv_label_set_text(lbl_sos, LV_SYMBOL_WARNING " SOS HELP");
    lv_obj_center(lbl_sos);

    // 按钮 3: AI语音咨询
    lv_obj_t * btn_ai = lv_button_create(bottom_panel);
    lv_obj_set_size(btn_ai, 300, 80);
    lv_obj_set_style_bg_color(btn_ai, lv_color_hex(0x0055A4), 0);
    lv_obj_set_style_text_color(btn_ai, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(btn_ai, 40, 0);
    lv_obj_t * lbl_ai = lv_label_create(btn_ai);
    lv_label_set_text(lbl_ai, LV_SYMBOL_AUDIO " AI VOICE");
    lv_obj_center(lbl_ai);

    // 挂载
    lv_screen_load(main_screen);
}

void ui_set_nav_info(const char* direction, const char* station_name) {
    if(dest_label) {
        char buf[64];
        snprintf(buf, sizeof(buf), "TO: %s", station_name);
        lv_label_set_text(dest_label, buf);
    }
}
