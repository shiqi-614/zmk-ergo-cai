#ifndef INFO_PANEL_H
#define INFO_PANEL_H

#include <zephyr/kernel.h>
#include <lvgl.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
typedef enum {
    INFO_ITEM_ROLE = 0,
    INFO_ITEM_PROFILE,
    INFO_ITEM_SPLIT,
    INFO_ITEM_BATTERY,
    INFO_ITEM_LAYER,
    INFO_ITEM_COUNT // 自动计算项数 (值为 5)
} info_item_t;

#else

typedef enum {
    INFO_ITEM_PROFILE = 0,
    INFO_ITEM_BATTERY,
    INFO_ITEM_LAYER,
    INFO_ITEM_COUNT // 自动计算项数 (值为 5)
} info_item_t;

#endif


/**
 * @brief The structure for the information panel widget.
 */
struct zmk_widget_info_panel {
    lv_obj_t *obj;          /* The LVGL object (container) */
    lv_obj_t *labels[INFO_ITEM_COUNT];    /* Array of labels for different information types */
};

/* Initialization function */
int zmk_widget_info_panel_init(struct zmk_widget_info_panel *widget, lv_obj_t *parent);

void zmk_widget_info_panel_update_text(struct zmk_widget_info_panel *widget, info_item_t item, const char *text);

/* 导出各个子监听器的初始化接口 */
void zmk_widget_info_panel_layer_init(void);
void zmk_widget_info_panel_battery_init(void);
void zmk_widget_info_panel_profile_init(void);

#endif /* INFO_PANEL_H */
