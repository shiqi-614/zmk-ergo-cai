#include "screen.h"
#include "info_panel.h"

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

/* 1. Getter：直接返回 ZMK 原生事件结构体 */
static struct zmk_layer_state_changed layer_get_state(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev) {
        return *ev;
    }

    /* 初始状态或空事件时，主动获取当前最高激活图层 */
    return (struct zmk_layer_state_changed){
        .layer = zmk_keymap_highest_layer_active(),
        .state = true,
    };
}

/* 2. Callback：参数类型严格匹配为 struct zmk_layer_state_changed */
static void layer_update_cb(struct zmk_layer_state_changed ev) {
    struct zmk_widget_screen *screen;

    /* 获取当前最高激活图层 */
    uint8_t current_layer = zmk_keymap_highest_layer_active();

    /* 遍历屏幕链表进行状态更新 */
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, screen, node) {

        /* 状态脏检查 (Diff Check) */
        if (screen->state.layer_index == current_layer) {
            continue;
        }
        screen->state.layer_index = current_layer;

        /* 格式化并更新 Label (局部刷新墨水屏) */
        char buf[32];
        snprintf(buf, sizeof(buf), "Layer: %u", current_layer);
        
        lv_label_set_text(screen->info_panel.labels[INFO_ITEM_LAYER], buf);
    }
}

/* 3. 注册 Listener 与事件订阅 */
ZMK_DISPLAY_WIDGET_LISTENER(widget_info_panel_layer, struct zmk_layer_state_changed,
                            layer_update_cb, layer_get_state);
ZMK_SUBSCRIPTION(widget_info_panel_layer, zmk_layer_state_changed);

void zmk_widget_info_panel_layer_init(void) {
    widget_info_panel_layer_init();
}
