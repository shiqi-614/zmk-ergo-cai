#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "screen.h"
#include "ui_assets.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* 定义控件ID，方便后续更新 */
typedef enum {
    UI_LABEL_ROLE = 0,
    UI_LABEL_PAIRING,
    UI_LABEL_DEVICE,
    UI_LABEL_BATTERY,
    UI_LABEL_STATUS,
    UI_LABEL_COUNT
} ui_label_id_t;

static lv_obj_t *ui_labels[UI_LABEL_COUNT];

void ui_create(lv_obj_t *scr) {
    LOG_INF("ui_create start");

    if (scr == NULL) {
        LOG_ERR("scr is NULL!");
        return;
    }

    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *panel = lv_obj_create(scr);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(60));
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN); /* 垂直排列 */
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 2. 创建测试文本，显式指定黑色字体 */
    const char *init_texts[] = {
        "Role: Central",
        "Pairing: Pending",
        "Target: ---",
        "Battery: 86%",
        "Status: " LV_SYMBOL_PLAY
    };

    for (int i = 0; i < UI_LABEL_COUNT; i++) {
        lv_obj_t *label = lv_label_create(panel);
        lv_label_set_text(label, init_texts[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0); /* 字体大小16 */
        lv_obj_set_style_text_color(label, lv_color_black(), 0);
        lv_obj_set_width(label, LV_PCT(100));          /* 标签填满容器宽度 */
        ui_labels[i] = label;
    }

    lv_obj_t *panel_bottom = lv_obj_create(scr);
    lv_obj_set_size(panel_bottom, LV_PCT(100), LV_PCT(40));  /* 占屏幕90% */
    lv_obj_set_pos(panel_bottom, 0, LV_PCT(60)); 

    lv_obj_set_style_pad_all(panel_bottom, 0, 0);
    lv_obj_set_style_border_width(panel_bottom, 0, 0); // 顺便去掉边框线

    lv_obj_set_flex_flow(panel_bottom, LV_FLEX_FLOW_COLUMN); 
    lv_obj_set_flex_align(panel_bottom, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *cat_img = lv_image_create(panel_bottom);  
    lv_image_set_src(cat_img, &running);  /* 使用已声明的图像资源 */

    /* ================== 核心修改：创建动画图片组件 ================== */
    /* lv_obj_t *cat_anim_img = lv_animimg_create(panel_bottom); */
    /* static const lv_image_dsc_t * anim_imgs[] = { */
        /* &sleeping, */
        /* &running, */
        /* &charging, */
        /* &connecting, */
        /* &pairing */
    /* };     */

    /* [> 2. 关联图片数组，并告知总帧数（这里是 5 张图） <] */
    /* lv_animimg_set_src(cat_anim_img, (const void **)anim_imgs, 5); */

    /* [> 3. 设置播放参数 <] */
    /* lv_animimg_set_duration(cat_anim_img, 20000);   [> 整个动画播完一轮耗时 2000 毫秒（每张图播 400ms） <] */
    /* lv_animimg_set_repeat_count(cat_anim_img, LV_ANIM_REPEAT_INFINITE); [> 无限循环播放 <] */

    /* [> 4. 开启播放 <] */
    /* lv_animimg_start(cat_anim_img); */

    /* 3. 关键：强制刷新硬件视图 */
    lv_obj_invalidate(panel);
}


int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    LOG_INF("zmk_widget_screen_init entry");

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }
    
    display_blanking_off(display_dev);
    /* lv_timer_handler(); */
    lv_display_t *disp = lv_display_get_default();
    if (!disp) {
        LOG_ERR("No default LVGL display found");
        return 0;
    }

    widget->obj = parent;

    struct display_capabilities caps;
    memset(&caps, 0, sizeof(caps));

    display_get_capabilities(display_dev, &caps);

    LOG_INF("Caps width: %d, height: %d", caps.x_resolution, caps.y_resolution);
    LOG_INF("Caps current_pixel_format: %d", caps.current_pixel_format);
    LOG_INF("Caps screen_info: 0x%X", caps.screen_info);

    ui_create(widget->obj);
    
    LOG_INF("zmk_widget_screen_init Done");
    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { 
    return widget->obj; 
}
