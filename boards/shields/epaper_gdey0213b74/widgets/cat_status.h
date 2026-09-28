#ifndef CAT_STATUS_H
#define CAT_STATUS_H

#include <zephyr/kernel.h>
#include <lvgl.h>


struct zmk_widget_screen;

/**
 * @brief The four states of our cat-themed status icon.
 */
typedef enum {
    CAT_STATE_RUNNING,   /* System is active and connected */
    CAT_STATE_CONNECTING,/* Attempting to connect via Bluetooth */
    CAT_STATE_PAIRING,   /* Handshaking/Bonding (especially for split) */
    CAT_STATE_SLEEPING   /* System is in low-power/sleep mode */
} cat_state_t;

/**
 * @brief The state structure for the cat widget.
 */
struct zmk_widget_cat_status {
    lv_obj_t *obj;          /* The LVGL object (canvas or image) */
    cat_state_t state;      /* The current mood of the cat */
};

/* Initialization function */
void zmk_widget_cat_status_init(struct zmk_widget_cat_status *widget, lv_obj_t *parent);

/* Get the LVGL object for the widget */
lv_obj_t *zmk_widget_cat_status_obj(struct zmk_widget_cat_status *widget);

/* Update the cat's state */
void zmk_widget_cat_status_set_state(struct zmk_widget_cat_status *widget, cat_state_t new_state);
void zmk_widget_cat_status_update_from_state(struct zmk_widget_screen *screen);

#endif /* CAT_STATUS_H */
