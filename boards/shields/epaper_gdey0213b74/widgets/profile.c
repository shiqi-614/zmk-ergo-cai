#include "screen.h"
#include "info_panel.h"
#include "cat_status.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/events/endpoint_changed.h>

extern int zmk_ble_active_profile_index(void);

/* 1. Getter：统一返回原生事件结构体 struct zmk_endpoint_changed */
static struct zmk_endpoint_changed profile_get_state(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *ev = as_zmk_endpoint_changed(eh);
    if (ev) {
        return *ev;
    }

    return (struct zmk_endpoint_changed){0};
}

/* 2. Callback：参数严格匹配为 struct zmk_endpoint_changed */
static void profile_update_cb(struct zmk_endpoint_changed ev) {
    struct zmk_widget_screen *screen;

    /* 从 ZMK API 实时获取最新的 Profile 序号和连接状态 */
    uint8_t active_profile = zmk_ble_active_profile_index();
    bool connected = zmk_ble_active_profile_is_connected();

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, screen, node) {
        /* 1. 脏检查 (Diff Check)：状态无变化时跳过 */
        if (screen->state.is_bluetooth_connected == connected &&
            screen->state.profile_index == active_profile) { 
            continue;
        }

        /* 2. 更新 screen 实例中的状态缓存 */
        screen->state.is_bluetooth_connected = connected;
        screen->state.profile_index = active_profile;

        /* 3. 格式化蓝牙 Profile 显示文本 */
        char buf[32];
        if (connected) {
            snprintf(buf, sizeof(buf), "Profile: %u", active_profile);
        } else {
            snprintf(buf, sizeof(buf), "Profile: %u Disconn", active_profile);
        }

        /* 4. 更新 UI Label 并刷新状态 */
        lv_label_set_text(screen->info_panel.labels[INFO_ITEM_PROFILE], buf);

        zmk_widget_cat_status_update_from_state(screen);
    }
}

/* 3. 注册 ZMK 监听器与订阅事件 */

/* 监听 1: 当前蓝牙通道切换 (Active Profile Changed) */
ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_profile, struct zmk_endpoint_changed,
                            profile_update_cb, profile_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_profile, zmk_ble_active_profile_changed);

/* 监听 2: 端点状态变更 (Endpoint Changed - 包含蓝牙连接/断开事件) */
ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_endpoint, struct zmk_endpoint_changed,
                            profile_update_cb, profile_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_endpoint, zmk_endpoint_changed);

void zmk_widget_info_panel_endpoint_init() {
    widget_info_panel_endpoint_init();
}

void zmk_widget_info_panel_profile_init() {
    widget_info_panel_profile_init();
}
