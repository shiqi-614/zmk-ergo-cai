#include "screen.h"
#include "cat_status.h"
#include "info_panel.h"
#include <zephyr/drivers/display.h>
#include <zmk/display.h>


#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(epaper_screen, LOG_LEVEL_INF);

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);


void ui_create(struct zmk_widget_screen *widget) {
    lv_obj_t *scr = widget->obj;
    LOG_INF("ui_create start");

    if (scr == NULL) {
        LOG_ERR("scr is NULL!");
        return;
    }

    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 1. Initialize Information Panel (Top 60%) */
    zmk_widget_info_panel_init(&widget->info_panel, scr);
    lv_obj_set_size(widget->info_panel.obj, LV_PCT(100), LV_PCT(60));
    lv_obj_set_pos(widget->info_panel.obj, 0, 0);

    /* 2. Initialize Cat Status Widget (Bottom 40%) */
    /* For simplicity, we'll pass NULL for cbuf as we are using image assets directly */
    zmk_widget_cat_status_init(&widget->cat_status, scr);
    lv_obj_set_size(widget->cat_status.obj, LV_PCT(100), LV_PCT(40));
    lv_obj_set_pos(widget->cat_status.obj, 0, LV_PCT(60));

    /* If using canvas-based drawing, cbuf would be used here.
       For now, we use image-based switching in cat_status.c */

    sys_slist_append(&widgets, &widget->node);

}


int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    LOG_INF("zmk_widget_screen_init entry");

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    display_blanking_off(display_dev);

    lv_display_t *disp = lv_display_get_default();
    if (!disp) {
        LOG_ERR("No default LVGL display found");
        return -ENODEV;
    }

    widget->obj = parent;
    ui_create(widget);

    lv_obj_invalidate(widget->obj);

    LOG_INF("zmk_widget_screen_init Done");
    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) {
    return widget->obj;
}
