#include "screen.h"
#include "info_panel.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/central.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT)

/* 1. 本地状态结构体 */
struct role_status_state {
    bool is_split;
    bool is_central; // true 为主手 (Central)，false 为副手 (Peripheral)
};

/* 2. 获取当前 Role 状态的 Getter 函数 */
static struct role_status_state role_get_state(const zmk_event_t *eh) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    return (struct role_status_state){
        .is_split = true,
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        .is_central = true,
#else
        .is_central = false,
#endif
    };
#else
    return (struct role_status_state){
        .is_split = false,
        .is_central = true,
    };
#endif
}

/* 3. 界面更新 Callback 函数 (带 screen->state 脏检查) */
static void role_update_cb(struct role_status_state state) {
    struct zmk_widget_screen *screen;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, screen, node) {
        /* 1. 脏检查 (Diff Check)：角色与分体模式未改变时直接跳过 */
        if (screen->state.is_split_connected == state.is_split) {
            /* 如果 screen_state 后续添加了 is_central 字段，可补充更精准的比对 */
            // continue; 
        }

        /* 2. 更新状态缓存 */
        screen->state.is_split_connected = state.is_split;

        /* 3. 格式化 Role 显示文本 */
        char buf[32];
        if (!state.is_split) {
            snprintf(buf, sizeof(buf), "Role: Solo");
        } else if (state.is_central) {
            snprintf(buf, sizeof(buf), "Role: Central");
        } else {
            snprintf(buf, sizeof(buf), "Role: Peripheral");
        }

        /* 4. 更新 screen->info_panel 上的 Role Label (触发墨水屏局部刷新) */
        lv_label_set_text(screen->info_panel.labels[INFO_ITEM_ROLE], buf);
    }
}

/* 4. 注册 ZMK 监听器与订阅事件 */
ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_role, struct zmk_split_peripheral_status_changed,
                            role_update_cb, role_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_role, zmk_split_peripheral_status_changed);
#endif
