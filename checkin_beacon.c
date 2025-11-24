#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <furi_hal_bt.h>

// =================================================
// 终极版信标 (集成内存级 MAC 欺骗)
// 修复：移除不兼容的 privacy_mode 字段
// =================================================

// 目标 MAC: C8:FD:19:A4:A6:F7
static uint8_t TARGET_MAC[6] = {0xC8, 0xFD, 0x19, 0xA4, 0xA6, 0xF7};

// iBeacon Payload (Major: 10033, Minor: 10113)
static const uint8_t IBEACON_DATA[] = {
    0x02, 0x01, 0x06, 
    0x1A, 0xFF, 
    0x4C, 0x00, 
    0x02, 0x15, 
    0x0F, 0xF5, 0xAA, 0xE2, 0xC3, 0xFE, 0x45, 0xB2, 
    0xB2, 0x47, 0xC0, 0x6B, 0xF2, 0xBC, 0x29, 0x7C,
    0x27, 0x31, 
    0x27, 0x81, 
    0xC5 
};

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    bool is_broadcasting;
} CheckinApp;

// 辅助函数：反转 MAC 地址字节序
void reverse_mac_addr(uint8_t* mac) {
    uint8_t tmp;
    for(int i = 0; i < 3; i++) {
        tmp = mac[i];
        mac[i] = mac[5 - i];
        mac[5 - i] = tmp;
    }
}

// 核心逻辑：配置 MAC 并启动
void start_spoofed_beacon(CheckinApp* app) {
    if(app->is_broadcasting) return;

    // 1. 确保蓝牙开启
    if(!furi_hal_bt_is_active()) {
        furi_hal_bt_start_advertising();
    }
    
    // 2. 停止当前广播
    furi_hal_bt_extra_beacon_stop();

    // 3. 准备配置结构体
    // 移除 .privacy_mode 以修复编译错误
    GapExtraBeaconConfig config = {
        .min_adv_interval_ms = 100, 
        .max_adv_interval_ms = 150,
        .adv_channel_map = GapAdvChannelMapAll,
        .adv_power_level = GapAdvPowerLevel_0dBm,
        .address_type = GapAddressTypePublic
    };

    // 4. 设置 MAC 地址 (拷贝并反转)
    uint8_t spoof_mac[6];
    memcpy(spoof_mac, TARGET_MAC, 6);
    reverse_mac_addr(spoof_mac); 
    memcpy(config.address, spoof_mac, 6);

    // 5. 应用硬件配置
    furi_hal_bt_extra_beacon_set_config(&config);

    // 6. 设置数据 Payload
    furi_hal_bt_extra_beacon_set_data(IBEACON_DATA, sizeof(IBEACON_DATA));

    // 7. 启动
    furi_hal_bt_extra_beacon_start();
    
    app->is_broadcasting = true;
}

void stop_beacon(CheckinApp* app) {
    if(app->is_broadcasting) {
        furi_hal_bt_extra_beacon_stop();
        app->is_broadcasting = false;
    }
}

static void draw_callback(Canvas* canvas, void* ctx) {
    CheckinApp* app = ctx;
    canvas_clear(canvas);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "WZXY Ultimate");
    
    canvas_set_font(canvas, FontSecondary);
    if(app->is_broadcasting) {
        canvas_draw_str(canvas, 0, 24, "Broadcasting...");
        canvas_draw_str(canvas, 0, 36, "Type: RAM Spoofing");
        canvas_draw_str(canvas, 0, 48, "MAC: C8:FD:19:A4:A6:F7");
        canvas_draw_str(canvas, 0, 60, "Major: 10033 | Minor: 10113");
    } else {
        canvas_draw_str(canvas, 0, 30, "Stopped.");
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    CheckinApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

int32_t checkin_beacon_app(void* p) {
    UNUSED(p);
    CheckinApp* app = malloc(sizeof(CheckinApp));
    memset(app, 0, sizeof(CheckinApp));
    
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    start_spoofed_beacon(app);

    InputEvent event;
    while(1) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) break;
        }
    }
    
    stop_beacon(app);
    
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
