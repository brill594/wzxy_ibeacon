#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <furi_hal_bt.h>

// =================================================
// 纯净版 Checkin Beacon
// 仅用于验证发射功能，不涉及文件操作
// =================================================

// 目标 UUID: 0FF5AAE2-C3FE-45B2-B247-C06BF2BC297C
// Major: 10033, Minor: 10113
static const uint8_t IBEACON_DATA[] = {
    0x02, 0x01, 0x06,
    0x1A, 0xFF, 
    0x4C, 0x00, 
    0x02, 0x15, 
    0x0F, 0xF5, 0xAA, 0xE2, 0xC3, 0xFE, 0x45, 0xB2, 
    0xB2, 0x47, 0xC0, 0x6B, 0xF2, 0xBC, 0x29, 0x7C,
    0x27, 0x31, // Major 10033
    0x27, 0x81, // Minor 10113
    0xC5        // RSSI
};

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    bool is_active;
} CheckinApp;

static void draw_callback(Canvas* canvas, void* ctx) {
    CheckinApp* app = ctx;
    canvas_clear(canvas);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "WZXY Pure Beacon");
    
    canvas_set_font(canvas, FontSecondary);
    if(app->is_active) {
        canvas_draw_str(canvas, 0, 24, "Broadcasting...");
        canvas_draw_str(canvas, 0, 36, "Major: 10033");
        canvas_draw_str(canvas, 0, 48, "Minor: 10113");
        canvas_draw_str(canvas, 0, 60, "MAC: System Default");
    } else {
        canvas_draw_str(canvas, 0, 30, "Stopped.");
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    CheckinApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

// 极其保守的启动逻辑
void start_advertising_pure() {
    // 1. 如果蓝牙正在运行，先停止，防止状态冲突
    if(furi_hal_bt_is_active()) {
        furi_hal_bt_extra_beacon_stop();
    } else {
        furi_hal_bt_start_advertising();
    }
    
    // 2. 设置数据
    furi_hal_bt_extra_beacon_set_data(IBEACON_DATA, sizeof(IBEACON_DATA));
    
    // 3. 启动
    furi_hal_bt_extra_beacon_start();
}

int32_t checkin_beacon_app(void* p) {
    UNUSED(p);
    CheckinApp* app = malloc(sizeof(CheckinApp));
    
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->is_active = true;
    start_advertising_pure();

    InputEvent event;
    while(1) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) break;
        }
    }
    
    furi_hal_bt_extra_beacon_stop();
    
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
