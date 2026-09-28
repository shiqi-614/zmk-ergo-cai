#include "screen.h"
#include "info_panel.h"
#include "cat_status.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/split_peripheral_status_changed.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT)

/* 1. 本地状态结构体 */
struct split_status_state {
    bool connected;
};

/* 2. 获取分体连接状态 */
static struct split_status_state split_get_state(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev = 
        as_zmk_split_peripheral_status_changed(eh);

    return (struct split_status_state){
        /* 如果事件非空则取事件状态，否则判定连接 */
        .connected = ev ? ev->connected : false,
    };
}

/* 3. 界面更新回调 (带 screen->state 脏检查) */
static void split_update_cb(struct split_status_state state) {
    struct zmk_widget_screen *screen;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, screen, node) {
        /* 脏检查：状态未改变则跳过，不刷墨水屏 */
        if (screen->state.is_split_connected == state.connected) {
            continue;
        }

        /* 更新状态缓存 */
        screen->state.is_split_connected = state.connected;

        /* 格式化文本 */
        char buf[32];
        if (screen->state.is_split_connected) {
            /* 分体通信正常，可加上 LVGL 关联成功图标或 OK */
            snprintf(buf, sizeof(buf), "Split: OK %s", LV_SYMBOL_OK);
        } else {
            /* 分体通信断开/未连接 */
            snprintf(buf, sizeof(buf), "Split: DISC %s", LV_SYMBOL_CLOSE);
        }

        /* 更新 screen->info_panel 上的 Split Label (局部刷墨水屏) */
        lv_label_set_text(screen->info_panel.labels[INFO_ITEM_SPLIT], buf);
        zmk_widget_cat_status_update_from_state(screen);
    }
}

/* 4. 注册监听器 */
ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_split, 
                            struct zmk_split_peripheral_status_changed,
                            split_update_cb, 
                            split_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_split, zmk_split_peripheral_status_changed);

#endif /* CONFIG_ZMK_SPLIT */
