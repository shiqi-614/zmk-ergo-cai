/*
 * 层颜色指示灯 (zmk,layer-color-led): 灯带上每颗灯显示当前最高激活层的颜色, 颜色按层序号取自节点的
 * colors; 列表之外的层、睡眠时、以及 idle 时 (CONFIG_ZMK_LAYER_COLOR_LED_OFF_IDLE) 灯灭。
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

LOG_MODULE_REGISTER(layer_color_led, CONFIG_ZMK_LOG_LEVEL);

#define LAYER_LED DT_COMPAT_GET_ANY_STATUS_OKAY(zmk_layer_color_led)
#define STRIP DT_PHANDLE(LAYER_LED, led_strip)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(zmk_layer_color_led) == 1,
             "only one zmk,layer-color-led node is supported");

static const struct device *const strip = DEVICE_DT_GET(STRIP);
static const uint32_t colors[] = DT_PROP(LAYER_LED, colors);
static struct led_rgb pixels[DT_PROP(STRIP, chain_length)];

static K_MUTEX_DEFINE(lock);
static uint32_t shown = UINT32_MAX; /* 灯上现在的颜色, 开机时未知 */

static uint32_t wanted_color(void) {
    enum zmk_activity_state state = zmk_activity_get_state();
    if (state == ZMK_ACTIVITY_SLEEP ||
        (IS_ENABLED(CONFIG_ZMK_LAYER_COLOR_LED_OFF_IDLE) && state == ZMK_ACTIVITY_IDLE)) {
        return 0;
    }
    zmk_keymap_layer_index_t layer = zmk_keymap_highest_layer_active();
    return layer < ARRAY_SIZE(colors) ? colors[layer] : 0;
}

/* 在事件里同步发给灯: ZMK 发出睡眠事件后马上关机, 放进工作队列就来不及关灯, 灯会在睡眠中一直亮着 */
static void show(void) {
    if (!device_is_ready(strip)) {
        return;
    }
    k_mutex_lock(&lock, K_FOREVER);
    uint32_t color = wanted_color();
    if (color != shown) {
        for (size_t i = 0; i < ARRAY_SIZE(pixels); i++) {
            pixels[i] = (struct led_rgb){
                .r = (color >> 16) & 0xff, .g = (color >> 8) & 0xff, .b = color & 0xff};
        }
        int err = led_strip_update_rgb(strip, pixels, ARRAY_SIZE(pixels));
        if (err < 0) {
            LOG_ERR("Failed to update LED strip %s: %d", strip->name, err);
        } else {
            shown = color;
        }
    }
    k_mutex_unlock(&lock);
}

static int layer_color_led_listener(const zmk_event_t *eh) {
    show();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(layer_color_led, layer_color_led_listener);
ZMK_SUBSCRIPTION(layer_color_led, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(layer_color_led, zmk_activity_state_changed);

static int layer_color_led_init(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip %s is not ready", strip->name);
        return -ENODEV;
    }
    show();
    return 0;
}

SYS_INIT(layer_color_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
