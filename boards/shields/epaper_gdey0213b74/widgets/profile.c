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

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(epaper_screen, CONFIG_ZMK_LOG_LEVEL);

extern int zmk_ble_active_profile_index(void);

/* 统一的状态更新与刷新逻辑 */
static void update_profile_ui(void) {
    struct zmk_widget_screen *screen;

    /* 从 ZMK API 实时获取最新的 Profile 序号和连接状态 */
    uint8_t active_profile = zmk_ble_active_profile_index();
    bool connected = zmk_ble_active_profile_is_connected();
    LOG_INF("is connected %d, profile %d", connected, active_profile);

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, screen, node) {
        /* 1. 严格的脏检查：只要连接状态或 Profile 序号有一个变了，就不跳过 */
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
            snprintf(buf, sizeof(buf), "Profile: ---");
        }

        /* 4. 更新 UI Label 并刷新状态 */
        lv_label_set_text(screen->info_panel.labels[INFO_ITEM_PROFILE], buf);

        zmk_widget_cat_status_update_from_state(screen);
    }
}

/* ==================== 1. Profile 切换事件监听 ==================== */
static struct zmk_ble_active_profile_changed profile_get_state(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *ev = as_zmk_ble_active_profile_changed(eh);
    if (ev) {
        return *ev;
    }
    return (struct zmk_ble_active_profile_changed){0};
}

static void profile_changed_cb(struct zmk_ble_active_profile_changed ev) {
    update_profile_ui();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_profile, struct zmk_ble_active_profile_changed,
                            profile_changed_cb, profile_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_profile, zmk_ble_active_profile_changed);


/* ==================== 2. 端点状态变更事件监听 ==================== */
static struct zmk_endpoint_changed endpoint_get_state(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *ev = as_zmk_endpoint_changed(eh);
    if (ev) {
        return *ev;
    }
    return (struct zmk_endpoint_changed){0};
}

static void endpoint_changed_cb(struct zmk_endpoint_changed ev) {
    update_profile_ui();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_endpoint, struct zmk_endpoint_changed,
                            endpoint_changed_cb, endpoint_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_endpoint, zmk_endpoint_changed);


void zmk_widget_info_panel_endpoint_init() {
    widget_info_panel_endpoint_init();
}

void zmk_widget_info_panel_profile_init() {
    widget_info_panel_profile_init();
}
