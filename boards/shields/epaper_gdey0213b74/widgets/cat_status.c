#include "cat_status.h"
#include "screen.h"
#include "../assets/cat/cat_images.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(epaper_screen, CONFIG_ZMK_LOG_LEVEL);

/* ========================================================================= */
/* 1. 基础接口                                                               */
/* ========================================================================= */

void zmk_widget_cat_status_init(struct zmk_widget_cat_status *widget, lv_obj_t *parent) {
    LOG_INF("zmk_widget_cat_status_init enter");
    if (!widget) return;
    
    widget->obj = lv_image_create(parent);
    widget->state = CAT_STATE_RUNNING; // 默认状态
    
    /* 默认加载一张图片 */
    lv_image_set_src(widget->obj, &running);
}

lv_obj_t *zmk_widget_cat_status_obj(struct zmk_widget_cat_status *widget) {
    return widget ? widget->obj : NULL;
}

void zmk_widget_cat_status_set_state(struct zmk_widget_cat_status *widget, cat_state_t new_state) {
    if (!widget || widget->state == new_state) {
        return;
    }

    widget->state = new_state;
    const lv_image_dsc_t *img_src = NULL;

    switch (new_state) {
        case CAT_STATE_RUNNING:
            img_src = &running;
            break;
        case CAT_STATE_CONNECTING:
            img_src = &connecting;
            break;
        case CAT_STATE_PAIRING:
            img_src = &pairing;
            break;
        case CAT_STATE_SLEEPING:
            img_src = &sleeping;
            break;
    }

    if (img_src != NULL && widget->obj != NULL) {
        lv_image_set_src(widget->obj, img_src);
    }
    LOG_INF("zmk_widget_cat_status_init done");
}

/* ========================================================================= */
/* 2. 新增：根据 screen_state 全局缓存评估并更新猫咪状态                       */
/* ========================================================================= */

void zmk_widget_cat_status_update_from_state(struct zmk_widget_screen *screen) {
    if (!screen) return;

    cat_state_t target_state = CAT_STATE_RUNNING;

    /* 优先级 1: 检查是否处于休眠/待机状态 */
    if (screen->state.is_sleeping) { 
        /* 注：请确保在 struct screen_state 中定义了 bool is_sleeping */
        target_state = CAT_STATE_SLEEPING;
    }

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    /* 优先级 2: 检查小键盘是否连接上了 */
    else if (!screen->state.is_split_connected) {
        target_state = CAT_STATE_PAIRING;
    }
#endif

    /* 优先级 3: 检查连接状态 (蓝牙未连上且分体未连上) */
    else if (!screen->state.is_bluetooth_connected) {
        target_state = CAT_STATE_CONNECTING;
    }
    /* 优先级 4: 正常就绪 */
    else {
        target_state = CAT_STATE_RUNNING;
    }

    /* 触发更新（带内部 Diff 检查，状态相同不会重复设图） */
    zmk_widget_cat_status_set_state(&screen->cat_status, target_state);
}

/* ========================================================================= */
/* 3. 新增：监听休眠与活动状态变更 (Activity/Sleep Events)                   */
/* ========================================================================= */

/* 本地传递给 Update 函数的状态 payload */
struct cat_activity_state {
    bool is_sleeping;
};

/* 1. get_state 直接返回事件指针或事件结构体 */
static struct zmk_activity_state_changed cat_activity_get_state(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    return ev ? *ev : (struct zmk_activity_state_changed){ .state = ZMK_ACTIVITY_ACTIVE };
}

/* 2. cb 直接接收 struct zmk_activity_state_changed */
static void cat_activity_update_cb(struct zmk_activity_state_changed ev) {
    struct zmk_widget_screen *screen;

    bool is_sleeping = (ev.state == ZMK_ACTIVITY_SLEEP);

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, screen, node) {
        screen->state.is_sleeping = is_sleeping;
        zmk_widget_cat_status_update_from_state(screen);
    }
}

/* 3. 注册 Listener */
ZMK_DISPLAY_WIDGET_LISTENER(widget_cat_activity, 
                            struct zmk_activity_state_changed,
                            cat_activity_update_cb, 
                            cat_activity_get_state);
ZMK_SUBSCRIPTION(widget_cat_activity, zmk_activity_state_changed);


void zmk_widget_cat_activity_init() {
    widget_cat_activity_init();
}
