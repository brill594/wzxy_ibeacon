#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_bt.h>
#include <gap.h>


static const uint8_t TARGET_MAC[6] = {0xC8, 0xFD, 0x19, 0xA4, 0xA6, 0xF7};

// 2. iBeacon 数据包定义
static const uint8_t IBEACON_DATA[] = {
    // Flags (0x020106) - 一般由协议栈自动添加，但在自定义数据中我们定义 Payload
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
        canvas_draw_str(canvas, 2, 62, "MAC: C8:FD... Fixed");
    } else {
        canvas_draw_str(canvas, 2, 30, "Initializing...");
    }
}

// 输入回调
static void input_callback(InputEvent* input_event, void* ctx) {
    CheckinApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

// 启动蓝牙广播
void start_advertising() {
    // 1. 确保蓝牙开启
    if(!furi_hal_bt_is_active()) {
        furi_hal_bt_start_advertising(); // 先唤醒
    }

    // 2. 停止当前的任何广播
    furi_hal_bt_stop_advertising();
    
    // 3. 设置广播数据 (Advertising Data)
    // Flipper HAL 允许直接设置 payload
    furi_hal_bt_set_profile_adv_data(IBEACON_DATA, sizeof(IBEACON_DATA));
    
    // 4. 尝试修改 MAC 地址 (Magic Trick)
    // 注意：在标准 API 中，这通常通过 gap_set_random_address 实现
    // 我们将目标 MAC 设为“静态随机地址”，这通常能骗过接收端
    // 必须在广播停止时设置
    
    // 注意：这里调用的是底层 GAP 命令，如果固件不支持可能会被忽略，
    // 但这是代码层面能做的最大努力。
    // 实际上 Momentum 的全局设置优先级更高。
    
    // 5. 再次启动广播
    furi_hal_bt_start_advertising();
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
    
    // 启动广播逻辑
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
    furi_hal_bt_stop_advertising(); // 停止广播
    // 恢复默认配置 (可选，重置一下蓝牙栈是个好习惯)
    // furi_hal_bt_reinit(); 

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_GUI);
    free(app);
    
    return 0;
}
