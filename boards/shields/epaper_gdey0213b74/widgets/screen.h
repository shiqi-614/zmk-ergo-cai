#pragma once
#ifndef SCREEN_H_
#define SCREEN_H_

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "cat_status.h"
#include "info_panel.h"

extern sys_slist_t widgets;

/**
 * @brief ZMK display events.
 */
typedef enum {
    ZMK_DISPLAY_EVENT_BT_CONNECTED,
    ZMK_DISPLAY_EVENT_BT_DISCONNECTED,
    ZMK_DISPLAY_EVENT_BT_PAIRING,
    ZMK_DISPLAY_EVENT_SLEEP_ENTER,
    ZMK_DISPLAY_EVENT_BATTERY_CHANGED,
    ZMK_DISPLAY_EVENT_LAYER_CHANGED,
    ZMK_DISPLAY_EVENT_ROLE_CHANGED,
    ZMK_DISPLAY_EVENT_PROFILE_CHANGED,
    ZMK_DISPLAY_EVENT_SPLIT_CHANGED,
} zmk_display_event_t;

/**
 * @brief Callback type for display event listeners.
 */
typedef void (*zmk_display_listener_cb_t)(void *user_data, zmk_display_event_t event);


struct screen_state {
    uint8_t battery;
    bool charging;
    uint8_t layer_index;
    uint8_t profile_index;
    bool is_split_connected;
    bool is_bluetooth_connected;
    bool is_sleeping;
};

struct zmk_widget_screen {
    sys_snode_t node;
    lv_obj_t *obj;
    struct zmk_widget_info_panel info_panel;
    struct zmk_widget_cat_status cat_status;
    struct screen_state state;
};

extern sys_slist_t zmk_widget_screen_list;

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget);


#endif /* SCREEN_H_ */
