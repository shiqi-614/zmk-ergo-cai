#include "info_panel.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(epaper_screen, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/keymap.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
#include <zmk/split/central.h>
#endif


extern void zmk_widget_info_panel_layer_init(void);
extern void zmk_widget_cat_activity_init(void);
extern void zmk_widget_info_panel_usb_init(void);
extern void zmk_widget_info_panel_battery_init(void);
extern void zmk_widget_info_panel_profile_init(void);
extern void zmk_widget_info_panel_endpoint_init(void);

/* ==================== Widget 初始化 ==================== */
int zmk_widget_info_panel_init(struct zmk_widget_info_panel *widget, lv_obj_t *parent) {
    LOG_INF("zmk_widget_info_panel_init enter");
    if (!widget || !parent) return -EINVAL;

    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, LV_PCT(100), LV_PCT(60));
    lv_obj_set_flex_flow(widget->obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget->obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 0, 0);



#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    static const char *init_texts[INFO_ITEM_COUNT] = {
        [INFO_ITEM_ROLE]    = "Role: ---",
        [INFO_ITEM_PROFILE] = "Profile: ---",
        [INFO_ITEM_SPLIT]   = "Split: ---",
        [INFO_ITEM_BATTERY] = "Battery: ---",
        [INFO_ITEM_LAYER]   = "Layer: ---",
    };
#else
    static const char *init_texts[INFO_ITEM_COUNT] = {
        [INFO_ITEM_PROFILE] = "Profile: ---",
        [INFO_ITEM_BATTERY] = "Battery: ---",
        [INFO_ITEM_LAYER]   = "Layer: ---",
    };

#endif
    /* 初始化 Label */
    for (int i = 0; i < INFO_ITEM_COUNT; i++) {
        widget->labels[i] = lv_label_create(widget->obj);

        lv_label_set_text(widget->labels[i], init_texts[i]);
        lv_obj_set_style_text_font(widget->labels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_width(widget->labels[i], LV_PCT(100));
    }

    zmk_widget_info_panel_layer_init();
    zmk_widget_cat_activity_init();
    zmk_widget_info_panel_usb_init();
    zmk_widget_info_panel_battery_init();
    zmk_widget_info_panel_profile_init();
    zmk_widget_info_panel_endpoint_init();
    LOG_INF("zmk_widget_info_panel_init done");


    return 0;
}

lv_obj_t *zmk_widget_info_panel_obj(struct zmk_widget_info_panel *widget) {
    return widget->obj;
}

