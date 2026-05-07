#include "ui.h"
#include <stdio.h>
#include <esp_heap_caps.h>   // heap_caps_malloc / MALLOC_CAP_SPIRAM

/*********************
 * DEFINES
 *********************/
#define COLOR_BG_MAIN           lv_color_hex(0xEBF5FF)
#define COLOR_TEXT_NAVY         lv_color_hex(0x1A4F8B)
#define COLOR_WHITE             lv_color_hex(0xFFFFFF)
#define COLOR_BLACK             lv_color_hex(0x000000)
#define COLOR_RED               lv_color_hex(0xCC0000)

#define COLOR_TILE_WASHROOM     lv_color_hex(0xD9F1FF)
#define COLOR_TILE_REGISTRATION lv_color_hex(0x5B9BD5)
#define COLOR_TILE_EXAM         lv_color_hex(0xA9D1F7)
#define COLOR_TILE_WAITING      lv_color_hex(0xFFFFFF)
#define COLOR_TILE_DEPT         lv_color_hex(0x3D85C6)
#define COLOR_TILE_SOS          lv_color_hex(0x2D64A8)

#define MAP_GRAY                lv_color_hex(0x808080)

#define ARROW_YELLOW            lv_color_hex(0xFFC000)

/**********************
 * STATIC VARIABLES
 **********************/
static lv_obj_t * scr_common;
static lv_obj_t * cont_right_idle;
static lv_obj_t * cont_right_nav;
static lv_obj_t * cont_right_voice;

static lv_obj_t * lbl_status_text;
static lv_obj_t * btn_stop;
static lv_obj_t * map_route_line;
static lv_obj_t * map_route_head;

// footer 中间按钮（三态）
static lv_obj_t * btn_reset_footer;   // 待机态：复位
static lv_obj_t * btn_voice_footer;   // 导航态：语音咨询
static lv_obj_t * btn_stop_footer;    // 语音态：停止识别

// 导航大箭头 canvas 缓冲区（PSRAM 分配，持久存在）
// 分辨率 360×180，RGB565 = 360*180*2 = 129600 字节
#define NAV_ARROW_W 360
#define NAV_ARROW_H 180
static uint8_t * _s_arrow_buf = nullptr;

// 语音界面专用控件
static lv_obj_t * lbl_asr_result;   // 对话框中显示 ASR 文本
static lv_obj_t * lbl_rec_hint;     // 录音状态提示（录音中.../识别中...）
static lv_obj_t * btn_voice_toggle; // 「开始/停止录音」切换按钮
static lv_obj_t * lbl_voice_btn;    // 按钮文字标签

// 麦克风控制回调（由 main.cpp 注入）
static voice_start_fn   _voice_start_cb  = nullptr;
static voice_stop_fn    _voice_stop_cb   = nullptr;
static voice_is_rec_fn  _voice_is_rec_cb = nullptr;

/**********************
 * EVENT HANDLERS
 **********************/
static void ui_event_btn_stop(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_set_state_idle();
    }
}

static void ui_event_btn_sos(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        printf("🚨 紧急求助按钮被点击！\n");
    }
}

static void ui_event_sim_nfc(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_set_state_navigating();
    }
}

// 「语音咨询」入口按钮 —— 切换到语音界面并自动开始录音
static void ui_event_btn_voice(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_set_state_voice();
        // 进入语音界面后自动开始录音
        if (_voice_start_cb) _voice_start_cb();
        if (lbl_rec_hint) lv_label_set_text(lbl_rec_hint, "🎙 录音中……");
        if (lbl_voice_btn) lv_label_set_text(lbl_voice_btn, "停止识别");
        if (lbl_asr_result) lv_label_set_text(lbl_asr_result, "正在聆听，请说话……");
    }
}

// 「停止识别/开始录音」切换按钮（在语音界面中）
static void ui_event_btn_voice_toggle(lv_event_t * e) {
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    bool is_rec = _voice_is_rec_cb ? _voice_is_rec_cb() : false;
    if (is_rec) {
        // 当前正在录音 → 停止并触发识别
        if (_voice_stop_cb) _voice_stop_cb();
        if (lbl_rec_hint) lv_label_set_text(lbl_rec_hint, "⏳ 识别中……");
        if (lbl_voice_btn) lv_label_set_text(lbl_voice_btn, "重新录音");
        if (lbl_asr_result) lv_label_set_text(lbl_asr_result, "正在识别，请稍候……");
    } else {
        // 当前未在录音 → 开始新一轮录音
        if (_voice_start_cb) _voice_start_cb();
        if (lbl_rec_hint) lv_label_set_text(lbl_rec_hint, "🎙 录音中……");
        if (lbl_voice_btn) lv_label_set_text(lbl_voice_btn, "停止识别");
        if (lbl_asr_result) lv_label_set_text(lbl_asr_result, "正在聆听，请说话……");
    }
}

/**********************
 * COMPONENT BUILDERS
 **********************/

static void create_header(lv_obj_t * parent) {
    lv_obj_t * header = lv_obj_create(parent);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_hor(header, 20, 0);
    
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_time = lv_label_create(header);
    lv_label_set_text(lbl_time, "2026年5月4日 21:40");
    lv_obj_set_style_text_color(lbl_time, COLOR_TEXT_NAVY, 0);
    lv_obj_set_style_text_font(lbl_time, &ui_font_source_han_32, 0);

    lv_obj_t * lbl_terminal = lv_label_create(header);
    lv_label_set_text(lbl_terminal, "子站 no.12");
    lv_obj_set_style_text_color(lbl_terminal, COLOR_TEXT_NAVY, 0);
    lv_obj_set_style_text_font(lbl_terminal, &ui_font_source_han_32, 0);
}

static lv_obj_t* create_map_room(lv_obj_t * parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_color_t color, const char* name, const lv_font_t* font, const char* symbol) {
    lv_obj_t * room = lv_obj_create(parent);
    lv_obj_set_pos(room, x, y);
    lv_obj_set_size(room, w, h);
    lv_obj_set_style_bg_color(room, color, 0);
    lv_obj_set_style_border_width(room, 0, 0);
    lv_obj_set_style_radius(room, 20, 0);
    lv_obj_set_style_shadow_width(room, 20, 0);
    lv_obj_set_style_shadow_opa(room, LV_OPA_10, 0);
    lv_obj_set_style_shadow_offset_y(room, 5, 0);
    lv_obj_set_style_pad_all(room, 15, 0);

    if (symbol) {
        lv_obj_t * sym_lbl = lv_label_create(room);
        lv_label_set_text(sym_lbl, symbol);
        lv_obj_set_style_text_font(sym_lbl, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(sym_lbl, (lv_color_brightness(color) > 128) ? COLOR_TEXT_NAVY : COLOR_WHITE, 0);
        lv_obj_align(sym_lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    }

    lv_obj_t * lbl = lv_label_create(room);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, (lv_color_brightness(color) > 128) ? COLOR_TEXT_NAVY : COLOR_WHITE, 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    
    return room;
}

static void create_left_map(lv_obj_t * parent) {
    lv_obj_t * map_cont = lv_obj_create(parent);
    lv_obj_set_size(map_cont, 0, LV_PCT(100));
    lv_obj_set_flex_grow(map_cont, 1);
    lv_obj_set_style_bg_opa(map_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(map_cont, 0, 0);
    lv_obj_set_style_pad_all(map_cont, 0, 0);

    // LV_SYMBOL_TINT=水滴(洗手间) LV_SYMBOL_BELL=呼叫(候诊) LV_SYMBOL_PLUS=医疗(口腔) LV_SYMBOL_EYE_OPEN=影像(B超)
    create_map_room(map_cont, 0, 0, 260, 200, COLOR_TILE_WASHROOM, "洗手间", &ui_font_source_han_20, LV_SYMBOL_TINT);
    create_map_room(map_cont, 440, 0, 160, 200, COLOR_TILE_REGISTRATION, "登记|报告\n收费|打印", &ui_font_source_han_20, LV_SYMBOL_FILE);
    create_map_room(map_cont, 0, 380, 260, 220, COLOR_TILE_EXAM, "总检室", &ui_font_source_han_20, LV_SYMBOL_EYE_OPEN);

    lv_obj_t * waiting = create_map_room(map_cont, 260, 380, 180, 220, COLOR_TILE_WAITING, "候诊区", &ui_font_source_han_20, LV_SYMBOL_BELL);
    lv_obj_set_style_border_width(waiting, 2, 0);
    lv_obj_set_style_border_color(waiting, COLOR_TILE_REGISTRATION, 0);

    create_map_room(map_cont, 440, 380, 160, 105, COLOR_TILE_DEPT, "B2口腔", &ui_font_source_han_20, LV_SYMBOL_PLUS);
    create_map_room(map_cont, 440, 495, 160, 105, COLOR_TILE_DEPT, "B5B超", &ui_font_source_han_20, LV_SYMBOL_EYE_OPEN);

    // 当前位置标记：圆形红点
    lv_obj_t * loc_marker = lv_obj_create(map_cont);
    lv_obj_set_size(loc_marker, 22, 22);
    lv_obj_set_style_radius(loc_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(loc_marker, COLOR_RED, 0);
    lv_obj_set_style_bg_opa(loc_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(loc_marker, 3, 0);
    lv_obj_set_style_border_color(loc_marker, COLOR_WHITE, 0);
    lv_obj_set_style_shadow_width(loc_marker, 12, 0);
    lv_obj_set_style_shadow_color(loc_marker, COLOR_RED, 0);
    lv_obj_set_style_shadow_opa(loc_marker, LV_OPA_50, 0);
    lv_obj_set_style_pad_all(loc_marker, 0, 0);
    lv_obj_set_pos(loc_marker, 265, 385);

    // Route line (hidden by default)
    map_route_line = lv_line_create(map_cont);
    static lv_point_precise_t line_pts[] = { {275, 395}, {275, 240}, {560, 240} };
    lv_line_set_points(map_route_line, line_pts, 3);
    lv_obj_set_style_line_width(map_route_line, 12, 0);
    lv_obj_set_style_line_color(map_route_line, ARROW_YELLOW, 0);
    lv_obj_set_style_line_rounded(map_route_line, true, 0);
    lv_obj_set_style_line_dash_width(map_route_line, 20, 0);
    lv_obj_set_style_line_dash_gap(map_route_line, 10, 0);
    lv_obj_add_flag(map_route_line, LV_OBJ_FLAG_HIDDEN);

    // Route arrow head (hidden by default)
    map_route_head = lv_line_create(map_cont);
    static lv_point_precise_t head_pts[] = { {530, 210}, {570, 240}, {530, 270} };
    lv_line_set_points(map_route_head, head_pts, 3);
    lv_obj_set_style_line_width(map_route_head, 12, 0);
    lv_obj_set_style_line_color(map_route_head, ARROW_YELLOW, 0);
    lv_obj_set_style_line_rounded(map_route_head, true, 0);
    lv_obj_add_flag(map_route_head, LV_OBJ_FLAG_HIDDEN);
}

static void create_right_idle(lv_obj_t * parent) {
    cont_right_idle = lv_obj_create(parent);
    lv_obj_set_size(cont_right_idle, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(cont_right_idle, 0, 0);
    lv_obj_set_style_bg_opa(cont_right_idle, LV_OPA_TRANSP, 0);
    // 待机态右侧白框不显示任何提示文字，等待 NFC 刷卡
}

static void create_right_nav(lv_obj_t * parent) {
    cont_right_nav = lv_obj_create(parent);
    lv_obj_set_size(cont_right_nav, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(cont_right_nav, 0, 0);
    lv_obj_set_style_bg_color(cont_right_nav, COLOR_WHITE, 0);
    lv_obj_set_style_pad_all(cont_right_nav, 0, 0);

    lv_obj_set_layout(cont_right_nav, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_right_nav, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_right_nav, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont_right_nav, 24, 0);

    // ── 大箭头：用 canvas 绘制实心块箭头 ──────────────────────
    // 申请 PSRAM 缓冲区（只申请一次，后续复用）
    if (!_s_arrow_buf) {
        _s_arrow_buf = (uint8_t*)heap_caps_malloc(
            NAV_ARROW_W * NAV_ARROW_H * 2,   // RGB565 = 2 bytes/pixel
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!_s_arrow_buf) {
            // PSRAM 不足时降级到内部堆
            _s_arrow_buf = (uint8_t*)malloc(
                NAV_ARROW_W * NAV_ARROW_H * 2);
        }
    }

    lv_obj_t * arrow_canvas = lv_canvas_create(cont_right_nav);
    lv_obj_set_size(arrow_canvas, NAV_ARROW_W, NAV_ARROW_H);
    lv_canvas_set_buffer(arrow_canvas, _s_arrow_buf,
                         NAV_ARROW_W, NAV_ARROW_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(arrow_canvas, COLOR_WHITE, LV_OPA_COVER);

    // 在 canvas 上绘制：箭头轴 + 三角形箭头头
    // 轴：x=0..259, y=60..119（高60px，垂直居中）
    // 头：三角形 (260,0) (360,90) (260,180)
    lv_layer_t layer;
    lv_canvas_init_layer(arrow_canvas, &layer);

    lv_draw_rect_dsc_t rdsc;
    lv_draw_rect_dsc_init(&rdsc);
    rdsc.bg_color   = ARROW_YELLOW;
    rdsc.bg_opa     = LV_OPA_COVER;
    rdsc.radius     = 0;
    rdsc.border_width = 0;
    lv_area_t shaft = {0, 60, 259, 119};
    lv_draw_rect(&layer, &rdsc, &shaft);

    lv_draw_triangle_dsc_t tdsc;
    lv_draw_triangle_dsc_init(&tdsc);
    // 实际安装的 LVGL 头文件确认：字段是 bg_color / bg_opa
    tdsc.bg_color = ARROW_YELLOW;
    tdsc.bg_opa   = LV_OPA_COVER;
    tdsc.p[0].x = 260; tdsc.p[0].y = 0;
    tdsc.p[1].x = 360; tdsc.p[1].y = 90;
    tdsc.p[2].x = 260; tdsc.p[2].y = 180;
    lv_draw_triangle(&layer, &tdsc);

    lv_canvas_finish_layer(arrow_canvas, &layer);

    // ── 「请向右走」—— 40px 粗体 ────────────────────────────
    lv_obj_t * lbl_dir = lv_label_create(cont_right_nav);
    lv_label_set_text(lbl_dir, "请向右走");
    lv_obj_set_style_text_color(lbl_dir, COLOR_BLACK, 0);
    lv_obj_set_style_text_font(lbl_dir, &ui_font_source_han_40, 0);

    // ── 「目的地：心电图室」—— 20px ─────────────────────────
    lv_obj_t * lbl_dest = lv_label_create(cont_right_nav);
    lv_label_set_text(lbl_dest, "目的地：心电图室");
    lv_obj_set_style_text_color(lbl_dest, COLOR_BLACK, 0);
    lv_obj_set_style_text_font(lbl_dest, &ui_font_source_han_20, 0);
}

static void create_right_voice(lv_obj_t * parent) {
    cont_right_voice = lv_obj_create(parent);
    lv_obj_set_size(cont_right_voice, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(cont_right_voice, 0, 0);
    lv_obj_set_style_bg_opa(cont_right_voice, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(cont_right_voice, 20, 0);
    lv_obj_set_layout(cont_right_voice, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_right_voice, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_right_voice, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont_right_voice, 16, 0);

    // ── 标题行 ──
    lv_obj_t * lbl_title = lv_label_create(cont_right_voice);
    lv_label_set_text(lbl_title, "AI 语音咨询");
    lv_obj_set_style_text_color(lbl_title, COLOR_TEXT_NAVY, 0);
    lv_obj_set_style_text_font(lbl_title, &ui_font_source_han_32, 0);

    // ── 录音状态提示 ──
    lbl_rec_hint = lv_label_create(cont_right_voice);
    lv_label_set_text(lbl_rec_hint, "点击下方按钮开始");
    lv_obj_set_style_text_color(lbl_rec_hint, MAP_GRAY, 0);
    lv_obj_set_style_text_font(lbl_rec_hint, &ui_font_source_han_20, 0);

    // ── 麦克风图标 ──
    lv_obj_t * mic_cont = lv_obj_create(cont_right_voice);
    lv_obj_set_size(mic_cont, 80, 120);
    lv_obj_set_style_bg_opa(mic_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mic_cont, 0, 0);

    lv_obj_t * mic_head = lv_obj_create(mic_cont);
    lv_obj_set_size(mic_head, 50, 80);
    lv_obj_set_align(mic_head, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_color(mic_head, COLOR_TILE_SOS, 0);
    lv_obj_set_style_border_width(mic_head, 0, 0);
    lv_obj_set_style_radius(mic_head, 25, 0);

    lv_obj_t * mic_stand = lv_obj_create(mic_cont);
    lv_obj_set_size(mic_stand, 8, 25);
    lv_obj_set_align(mic_stand, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_color(mic_stand, COLOR_TEXT_NAVY, 0);
    lv_obj_set_style_radius(mic_stand, 0, 0);
    lv_obj_set_style_border_width(mic_stand, 0, 0);

    // ── 「停止识别 / 重新录音」切换按钮 ──
    btn_voice_toggle = lv_btn_create(cont_right_voice);
    lv_obj_set_size(btn_voice_toggle, LV_PCT(85), 56);
    lv_obj_set_style_bg_color(btn_voice_toggle, COLOR_TILE_SOS, 0);
    lv_obj_set_style_radius(btn_voice_toggle, 28, 0);
    lv_obj_set_style_shadow_width(btn_voice_toggle, 16, 0);
    lv_obj_set_style_shadow_opa(btn_voice_toggle, LV_OPA_30, 0);
    lv_obj_add_event_cb(btn_voice_toggle, ui_event_btn_voice_toggle, LV_EVENT_CLICKED, NULL);

    lbl_voice_btn = lv_label_create(btn_voice_toggle);
    lv_label_set_text(lbl_voice_btn, "停止识别");
    lv_obj_set_style_text_color(lbl_voice_btn, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_voice_btn, &ui_font_source_han_20, 0);
    lv_obj_set_align(lbl_voice_btn, LV_ALIGN_CENTER);

    // ── ASR 结果对话框 ──
    lv_obj_t * dialog = lv_obj_create(cont_right_voice);
    lv_obj_set_size(dialog, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(dialog, 100, 0);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0xF0F4FF), 0);
    lv_obj_set_style_radius(dialog, 16, 0);
    lv_obj_set_style_border_color(dialog, lv_color_hex(0xC9E4F5), 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_pad_all(dialog, 14, 0);

    lbl_asr_result = lv_label_create(dialog);
    lv_label_set_text(lbl_asr_result, "识别结果将显示在这里……");
    lv_obj_set_style_text_color(lbl_asr_result, COLOR_TEXT_NAVY, 0);
    lv_obj_set_style_text_font(lbl_asr_result, &ui_font_source_han_20, 0);
    lv_label_set_long_mode(lbl_asr_result, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_asr_result, LV_PCT(100));
}

static lv_obj_t* create_status_row(lv_obj_t * parent, const char* text) {
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * dot = lv_obj_create(row);
    lv_obj_set_size(dot, 16, 16);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, COLOR_RED, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_margin_right(dot, 10, 0);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_BLACK, 0);
    lv_obj_set_style_text_font(lbl, &ui_font_source_han_20, 0);

    return lbl;
}

static void create_footer(lv_obj_t * parent) {
    lv_obj_t * footer = lv_obj_create(parent);
    lv_obj_set_size(footer, LV_PCT(100), 100);
    lv_obj_set_style_bg_color(footer, COLOR_BG_MAIN, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_set_style_pad_hor(footer, 32, 0);

    lv_obj_set_layout(footer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // ── 左：状态文字区 ──────────────────────────────────────────
    lv_obj_t * status_cont = lv_obj_create(footer);
    lv_obj_set_size(status_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(status_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_cont, 0, 0);
    lv_obj_set_style_pad_all(status_cont, 0, 0);
    lv_obj_set_layout(status_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(status_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(status_cont, 6, 0);

    // 第一行：● 您所在位置
    lv_obj_t * row1 = lv_obj_create(status_cont);
    lv_obj_set_size(row1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_layout(row1, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row1, 8, 0);
    lv_obj_t * dot1 = lv_obj_create(row1);
    lv_obj_set_size(dot1, 14, 14);
    lv_obj_set_style_radius(dot1, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot1, COLOR_RED, 0);
    lv_obj_set_style_border_width(dot1, 0, 0);
    lv_obj_t * lbl_loc = lv_label_create(row1);
    lv_label_set_text(lbl_loc, "您所在位置");
    lv_obj_set_style_text_color(lbl_loc, COLOR_TEXT_NAVY, 0);
    lv_obj_set_style_text_font(lbl_loc, &ui_font_source_han_20, 0);

    // 第二行：● 待机中…… / 已识别用户：张**
    lv_obj_t * row2 = lv_obj_create(status_cont);
    lv_obj_set_size(row2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_set_layout(row2, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row2, 8, 0);
    lv_obj_t * dot2 = lv_obj_create(row2);
    lv_obj_set_size(dot2, 14, 14);
    lv_obj_set_style_radius(dot2, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot2, COLOR_RED, 0);
    lv_obj_set_style_border_width(dot2, 0, 0);
    lbl_status_text = lv_label_create(row2);
    lv_label_set_text(lbl_status_text, "待机中……");
    lv_obj_set_style_text_color(lbl_status_text, COLOR_TEXT_NAVY, 0);
    lv_obj_set_style_text_font(lbl_status_text, &ui_font_source_han_20, 0);

    // ── 中间按钮区（三态切换）──────────────────────────────────
    // 待机态：白色「复位」按钮
    btn_reset_footer = lv_btn_create(footer);
    lv_obj_set_size(btn_reset_footer, 260, 72);
    lv_obj_set_style_bg_color(btn_reset_footer, COLOR_WHITE, 0);
    lv_obj_set_style_bg_color(btn_reset_footer, lv_color_hex(0xE0E0E0), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_reset_footer, 36, 0);
    lv_obj_set_style_border_width(btn_reset_footer, 2, 0);
    lv_obj_set_style_border_color(btn_reset_footer, lv_color_hex(0xC0C0C0), 0);
    lv_obj_set_style_shadow_width(btn_reset_footer, 0, 0);
    lv_obj_add_event_cb(btn_reset_footer, [](lv_event_t*e){
        if(lv_event_get_code(e)==LV_EVENT_CLICKED) ui_set_state_idle();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t * reset_row = lv_obj_create(btn_reset_footer);
    lv_obj_set_size(reset_row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(reset_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(reset_row, 0, 0);
    lv_obj_set_style_pad_all(reset_row, 0, 0);
    lv_obj_set_layout(reset_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(reset_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(reset_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(reset_row, 10, 0);
    lv_obj_t * reset_icon = lv_label_create(reset_row);
    lv_label_set_text(reset_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(reset_icon, COLOR_TEXT_NAVY, 0);
    lv_obj_t * reset_lbl = lv_label_create(reset_row);
    lv_label_set_text(reset_lbl, "复位");
    lv_obj_set_style_text_color(reset_lbl, COLOR_TEXT_NAVY, 0);
    lv_obj_set_style_text_font(reset_lbl, &ui_font_source_han_32, 0);

    // 导航态：天蓝「语音咨询」按钮
    btn_voice_footer = lv_btn_create(footer);
    lv_obj_set_size(btn_voice_footer, 260, 72);
    lv_obj_set_style_bg_color(btn_voice_footer, lv_color_hex(0x3EB5D8), 0);
    lv_obj_set_style_bg_color(btn_voice_footer, lv_color_hex(0x2A9AB8), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_voice_footer, 36, 0);
    lv_obj_set_style_border_width(btn_voice_footer, 0, 0);
    lv_obj_set_style_shadow_width(btn_voice_footer, 0, 0);
    lv_obj_add_event_cb(btn_voice_footer, ui_event_btn_voice, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_vf = lv_label_create(btn_voice_footer);
    lv_label_set_text(lbl_vf, "语音咨询");
    lv_obj_set_style_text_color(lbl_vf, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_vf, &ui_font_source_han_32, 0);
    lv_obj_set_align(lbl_vf, LV_ALIGN_CENTER);
    lv_obj_add_flag(btn_voice_footer, LV_OBJ_FLAG_HIDDEN);

    // 语音态：天蓝「停止识别」按钮
    btn_stop_footer = lv_btn_create(footer);
    lv_obj_set_size(btn_stop_footer, 260, 72);
    lv_obj_set_style_bg_color(btn_stop_footer, lv_color_hex(0x3EB5D8), 0);
    lv_obj_set_style_bg_color(btn_stop_footer, lv_color_hex(0x2A9AB8), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_stop_footer, 36, 0);
    lv_obj_set_style_border_width(btn_stop_footer, 0, 0);
    lv_obj_set_style_shadow_width(btn_stop_footer, 0, 0);
    lv_obj_add_event_cb(btn_stop_footer, ui_event_btn_voice_toggle, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_sf = lv_label_create(btn_stop_footer);
    lv_label_set_text(lbl_sf, "停止识别");
    lv_obj_set_style_text_color(lbl_sf, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_sf, &ui_font_source_han_32, 0);
    lv_obj_set_align(lbl_sf, LV_ALIGN_CENTER);
    lv_obj_add_flag(btn_stop_footer, LV_OBJ_FLAG_HIDDEN);

    // ── 右：深红「紧急求助」按钮 ──────────────────────────────
    lv_obj_t * btn_sos = lv_btn_create(footer);
    lv_obj_set_size(btn_sos, 260, 72);
    lv_obj_set_style_bg_color(btn_sos, lv_color_hex(0xB01C1C), 0);
    lv_obj_set_style_bg_color(btn_sos, lv_color_hex(0x8A1414), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_sos, 36, 0);
    lv_obj_set_style_border_width(btn_sos, 0, 0);
    lv_obj_set_style_shadow_width(btn_sos, 0, 0);
    lv_obj_add_event_cb(btn_sos, ui_event_btn_sos, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_sos = lv_label_create(btn_sos);
    lv_label_set_text(lbl_sos, "紧急求助");
    lv_obj_set_style_text_color(lbl_sos, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_sos, &ui_font_source_han_32, 0);
    lv_obj_set_align(lbl_sos, LV_ALIGN_CENTER);

    btn_stop = lv_btn_create(footer);
    lv_obj_add_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);

}


/**********************
 * GLOBAL FUNCTIONS
 **********************/

void ui_init(void) {
    scr_common = lv_obj_create(NULL);
    lv_obj_set_size(scr_common, 1280, 800);
    lv_obj_set_style_bg_color(scr_common, COLOR_BG_MAIN, 0);
    lv_obj_set_layout(scr_common, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scr_common, LV_FLEX_FLOW_COLUMN);

    create_header(scr_common);

    lv_obj_t * main_row = lv_obj_create(scr_common);
    lv_obj_set_size(main_row, LV_PCT(100), 0);
    lv_obj_set_flex_grow(main_row, 1);
    lv_obj_set_style_bg_opa(main_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_row, 0, 0);
    lv_obj_set_style_pad_all(main_row, 20, 0);
    lv_obj_set_style_pad_column(main_row, 20, 0);
    lv_obj_set_layout(main_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_row, LV_FLEX_FLOW_ROW);

    create_left_map(main_row);

    lv_obj_t * right_container = lv_obj_create(main_row);
    lv_obj_set_size(right_container, 0, LV_PCT(100));
    lv_obj_set_flex_grow(right_container, 1);
    lv_obj_set_style_bg_color(right_container, COLOR_WHITE, 0);
    lv_obj_set_style_border_width(right_container, 3, 0);
    lv_obj_set_style_border_color(right_container, lv_color_hex(0xC9E4F5), 0);
    lv_obj_set_style_radius(right_container, 25, 0);
    lv_obj_set_style_pad_all(right_container, 0, 0);

    create_right_idle(right_container);
    create_right_nav(right_container);
    create_right_voice(right_container);

    create_footer(scr_common);

    ui_set_state_idle();

    lv_scr_load(scr_common);
}

void ui_set_state_idle(void) {
    lv_obj_add_flag(cont_right_nav,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cont_right_voice, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cont_right_idle, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(map_route_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(map_route_head, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(lbl_status_text, "待机中……");

    // footer 中间按钮：仅显示复位
    lv_obj_clear_flag(btn_reset_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_voice_footer,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_stop_footer,    LV_OBJ_FLAG_HIDDEN);
}

void ui_set_state_navigating(void) {
    lv_obj_add_flag(cont_right_idle,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cont_right_voice, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cont_right_nav,  LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(map_route_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(map_route_head, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(lbl_status_text, "已识别用户：张**");

    // footer 中间按钮：显示语音咨询
    lv_obj_add_flag(btn_reset_footer,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_voice_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_stop_footer,    LV_OBJ_FLAG_HIDDEN);
}

void ui_set_state_voice(void) {
    lv_obj_add_flag(cont_right_idle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cont_right_nav,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cont_right_voice, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(map_route_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(map_route_head, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(lbl_status_text, "已识别用户：张**");

    // footer 中间按钮：显示停止识别
    lv_obj_add_flag(btn_reset_footer,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_voice_footer,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_stop_footer, LV_OBJ_FLAG_HIDDEN);
}

void ui_set_nav_info(const char* direction, const char* destination) {
    // TODO: 根据 direction 字符串（"向左"/"向右"/"向前"/"到达"等）更新箭头图标
    // 此处预留，后续可用 lv_label_set_text 动态切换箭头和目的地标签
    (void)direction;
    (void)destination;
}

void ui_set_asr_result(const char* text) {
    if (lbl_asr_result) {
        lv_label_set_text(lbl_asr_result, text);
    }
    // 识别完成后恢复录音提示状态
    if (lbl_rec_hint) {
        lv_label_set_text(lbl_rec_hint, "✓ 识别完成");
    }
    if (lbl_voice_btn) {
        lv_label_set_text(lbl_voice_btn, "重新录音");
    }
}

void ui_set_voice_callbacks(voice_start_fn start_cb,
                             voice_stop_fn  stop_cb,
                             voice_is_rec_fn is_rec_cb) {
    _voice_start_cb  = start_cb;
    _voice_stop_cb   = stop_cb;
    _voice_is_rec_cb = is_rec_cb;
}