#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_bt.h>

// =================================================
// 核心配置区域
// =================================================

// iBeacon 数据包定义 (标准结构)
// 包含 Flags (02 01 06) 和 Manufacturer Data
static const uint8_t IBEACON_DATA[] = {
    // 1. Flags: LE General Discoverable Mode, BR/EDR Not Supported
    0x02, 0x01, 0x06,

    // 2. Manufacturer Specific Data
    // Length: 0x1A (26 bytes)
    // Type: 0xFF (Manufacturer Specific Data)
    0x1A, 0xFF, 
    
    // Apple Company ID (0x004C -> Little Endian: 4C 00)
    0x4C, 0x00, 
    
    // iBeacon Type (0x02) & Length (0x15 = 21 bytes remaining)
    0x02, 0x15, 
    
    // UUID: 0FF5AAE2-C3FE-45B2-B247-C06BF2BC297C
    0x0F, 0xF5, 0xAA, 0xE2, 0xC3, 0xFE, 0x45, 0xB2, 
    0xB2, 0x47, 0xC0, 0x6B, 0xF2, 0xBC, 0x29, 0x7C,
    
    // Major: 10033 -> Hex: 27 31
    0x27, 0x31, 
    
    // Minor: 10113 -> Hex: 27 81
    0x27, 0x81, 
    
    // Measured Power (RSSI at 1m): -59 dBm -> Hex: C5
    0xC5 
};

// =================================================
// 应用逻辑
// =================================================

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    bool is_advertising;
} CheckinApp;

// 绘制界面
static void draw_callback(Canvas* canvas, void* ctx) {
    CheckinApp* app = ctx;
    canvas_clear(canvas);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Checkin Beacon");
    
    canvas_set_font(canvas, FontSecondary);
    if(app->is_advertising) {
        canvas_draw_str(canvas, 2, 26, "Status: BROADCASTING!");
        canvas_draw_str(canvas, 2, 38, "Major: 10033");
        canvas_draw_str(canvas, 2, 50, "Minor: 10113");
        // 提示用户 MAC 必须在系统设置里改
        canvas_draw_str(canvas, 2, 62, "MAC: Use System Settings!");
    } else {
        canvas_draw_str(canvas, 2, 30, "Stopping...");
    }
}

// 输入回调
static void input_callback(InputEvent* input_event, void* ctx) {
    CheckinApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

// 启动蓝牙广播 (使用 Extra Beacon API)
void start_advertising() {
    // 1. 确保蓝牙开启
    if(!furi_hal_bt_is_active()) {
        furi_hal_bt_start_advertising(); 
    }

    // 2. 停止之前的 Extra Beacon 广播 (如果有)
    furi_hal_bt_extra_beacon_stop();
    
    // 3. 设置广播数据
    // 这是一个标准的 Furi HAL 函数，用于设置自定义 Beacon 数据
    furi_hal_bt_extra_beacon_set_data(IBEACON_DATA, sizeof(IBEACON_DATA));
    
    // 4. 启动 Extra Beacon 广播
    furi_hal_bt_extra_beacon_start();
}

// 主入口
int32_t checkin_beacon_app(void* p) {
    UNUSED(p);
    
    CheckinApp* app = malloc(sizeof(CheckinApp));
    
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    // 启动广播
    app->is_advertising = true;
    start_advertising();

    InputEvent event;
    while(1) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if(event.key == InputKeyBack && event.type == InputTypeShort) {
                break;
            }
        }
    }
    
    // 清理与退出
    furi_hal_bt_extra_beacon_stop(); // 停止广播
    
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_GUI);
    free(app);
    
    return 0;
}
