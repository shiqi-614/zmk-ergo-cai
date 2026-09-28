#include "screen.h"
#include "info_panel.h"
#include "cat_status.h"

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/battery.h>
#include <zmk/usb.h>

/* 1. Getter：统一返回原生事件结构体 struct zmk_battery_state_changed */
static struct zmk_battery_state_changed battery_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    if (ev) {
        return *ev;
    }

    /* 默认/初始状态构建 */
    return (struct zmk_battery_state_changed){
        .state_of_charge = zmk_battery_state_of_charge()
    };
}

/* 2. Callback：参数严格匹配为 struct zmk_battery_state_changed (值传递) */
static void battery_update_cb(struct zmk_battery_state_changed ev) {
    struct zmk_widget_screen *screen;

    /* 从 ZMK API 实时获取电量和 USB 供电/充电状态 */
    uint8_t level = zmk_battery_state_of_charge();
    bool charging = zmk_usb_is_powered();

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, screen, node) {
        /* 1. 脏检查 (Diff Check)：状态无变化时直接跳过 */
        if (screen->state.battery == level && 
            screen->state.charging == charging) {
            continue;
        }

        /* 2. 更新屏幕实例中的状态缓存 */
        screen->state.battery = level;
        screen->state.charging = charging;

        /* 3. 格式化文本 */
        char buf[32];
        if (charging) {
            snprintf(buf, sizeof(buf), "Battery: %u%% %s", 
                     level, LV_SYMBOL_CHARGE);
        } else {
            snprintf(buf, sizeof(buf), "Battery: %u%%", level);
        }

        /* 4. 更新 screen->info_panel 上的 Battery Label */
        lv_label_set_text(screen->info_panel.labels[INFO_ITEM_BATTERY], buf);

        zmk_widget_cat_status_update_from_state(screen);
    }
}

/* 3. 注册监听电量变化与 USB 插拔事件 */
ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_battery, struct zmk_battery_state_changed,
                            battery_update_cb, battery_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_battery, zmk_battery_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_usb, struct zmk_battery_state_changed,
                            battery_update_cb, battery_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_usb, zmk_usb_conn_state_changed);

void zmk_widget_info_panel_battery_init() {
    widget_info_panel_battery_init();
}

void zmk_widget_info_panel_usb_init() {
    widget_info_panel_usb_init();
}
