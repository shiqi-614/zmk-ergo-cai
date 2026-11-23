/*
 * Pointer settings that change while the keyboard runs, for the trackball and the joystick: an input processor
 * (ergocai,input-processor-pointer-config) swaps x and y, flips either and scales both, and a behavior
 * (ergocai,behavior-pointer-config) changes them from a key, which ZMK Studio offers by name. They are kept over
 * power-off under "pointer_cfg/<settings-name>"; the devicetree has the defaults. Which way a trackball's sensor
 * reads depends on how it sits (read from under the ball or from its side), so a key fixes a ball that runs the
 * wrong way without building the firmware again.
 */

#define DT_DRV_COMPAT ergocai_input_processor_pointer_config

#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <drivers/behavior.h>
#include <drivers/input_processor.h>
#include <dt-bindings/zmk/pointer_config.h>
#include <zmk/behavior.h>

LOG_MODULE_REGISTER(pointer_config, CONFIG_ZMK_LOG_LEVEL);

/* the speeds a step goes through, in quarters: 1/4, 1/2, 3/4, 1, 3/2, 2, 3, 4 */
static const uint8_t speeds[] = {1, 2, 3, 4, 6, 8, 12, 16};
#define SPEED_DEFAULT 3
#define SAVE_DELAY K_SECONDS(2)

struct ptr_settings {
    uint8_t swap;
    uint8_t invert_x;
    uint8_t invert_y;
    uint8_t speed; /* an index into speeds */
};

struct ptr_config {
    const char *name;
    struct ptr_settings defaults;
};

struct ptr_data {
    const struct device *dev;
    struct ptr_settings now;
    int32_t rest_x, rest_y; /* what scaling left over, in quarters, so slow movements still add up */
    struct k_work_delayable save;
};

static int ptr_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                            uint32_t param2, struct zmk_input_processor_state *state) {
    struct ptr_data *data = dev->data;
    const struct ptr_settings *s = &data->now;

    if (event->type != INPUT_EV_REL || (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (s->swap) {
        event->code = event->code == INPUT_REL_X ? INPUT_REL_Y : INPUT_REL_X;
    }
    bool x = event->code == INPUT_REL_X;
    if ((x && s->invert_x) || (!x && s->invert_y)) {
        event->value = -event->value;
    }
    if (speeds[s->speed] != 4) {
        int32_t *rest = x ? &data->rest_x : &data->rest_y;
        int32_t quarters = event->value * speeds[s->speed] + *rest;
        event->value = quarters / 4;
        *rest = quarters % 4;
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api ptr_api = {
    .handle_event = ptr_handle_event,
};

static void ptr_save(struct k_work *work) {
#if IS_ENABLED(CONFIG_SETTINGS)
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct ptr_data *data = CONTAINER_OF(dwork, struct ptr_data, save);
    const struct ptr_config *cfg = data->dev->config;
    char key[48];
    snprintf(key, sizeof(key), "pointer_cfg/%s", cfg->name);
    int err = settings_save_one(key, &data->now, sizeof(data->now));
    if (err < 0) {
        LOG_ERR("%s: could not save the pointer settings (%d)", cfg->name, err);
    }
#endif
}

/* a key's command on one device's settings (the behavior below calls it) */
static int ptr_command(const struct device *dev, uint32_t command) {
    struct ptr_data *data = dev->data;
    const struct ptr_config *cfg = dev->config;
    struct ptr_settings *s = &data->now;

    switch (command) {
    case PTR_INVERT_X:
        s->invert_x = !s->invert_x;
        break;
    case PTR_INVERT_Y:
        s->invert_y = !s->invert_y;
        break;
    case PTR_SWAP_XY:
        s->swap = !s->swap;
        break;
    case PTR_FASTER:
        if (s->speed + 1 < ARRAY_SIZE(speeds)) {
            s->speed++;
        }
        break;
    case PTR_SLOWER:
        if (s->speed > 0) {
            s->speed--;
        }
        break;
    case PTR_RESET:
        *s = cfg->defaults;
        break;
    default:
        return -ENOTSUP;
    }
    data->rest_x = data->rest_y = 0;
    LOG_INF("%s: swap %d, invert x %d, invert y %d, speed %d/4", cfg->name, s->swap, s->invert_x, s->invert_y,
            speeds[s->speed]);
    k_work_reschedule(&data->save, SAVE_DELAY);
    return 0;
}

#define PTR_INST(n)                                                                                \
    static struct ptr_data ptr_data_##n;                                                           \
    static const struct ptr_config ptr_config_##n = {                                              \
        .name = DT_INST_PROP(n, settings_name),                                                    \
        .defaults = {.swap = DT_INST_PROP(n, swap_xy),                                             \
                     .invert_x = DT_INST_PROP(n, invert_x),                                        \
                     .invert_y = DT_INST_PROP(n, invert_y),                                        \
                     .speed = SPEED_DEFAULT},                                                      \
    };                                                                                             \
    static int ptr_init_##n(const struct device *dev) {                                            \
        struct ptr_data *data = dev->data;                                                         \
        data->dev = dev;                                                                           \
        data->now = ptr_config_##n.defaults;                                                       \
        k_work_init_delayable(&data->save, ptr_save);                                              \
        return 0;                                                                                  \
    }                                                                                              \
    DEVICE_DT_INST_DEFINE(n, ptr_init_##n, NULL, &ptr_data_##n, &ptr_config_##n, POST_KERNEL,     \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &ptr_api);

DT_INST_FOREACH_STATUS_OKAY(PTR_INST)

/* the settings kept: "pointer_cfg/<settings-name>" for each device */
#if IS_ENABLED(CONFIG_SETTINGS)
#define PTR_DEV(n) DEVICE_DT_INST_GET(n),
static const struct device *const ptr_devices[] = {DT_INST_FOREACH_STATUS_OKAY(PTR_DEV)};

static int ptr_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    for (size_t i = 0; i < ARRAY_SIZE(ptr_devices); i++) {
        const struct ptr_config *cfg = ptr_devices[i]->config;
        struct ptr_data *data = ptr_devices[i]->data;
        const char *next;
        if (settings_name_steq(name, cfg->name, &next) && !next) {
            struct ptr_settings s;
            if (len != sizeof(s)) {
                return -EINVAL;
            }
            int rc = read_cb(cb_arg, &s, sizeof(s));
            if (rc < 0) {
                return rc;
            }
            if (s.speed >= ARRAY_SIZE(speeds)) {
                s.speed = SPEED_DEFAULT;
            }
            data->now = s;
            return 0;
        }
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(pointer_cfg, "pointer_cfg", NULL, ptr_settings_set, NULL, NULL);
#endif

/* --- the behavior: a key that runs one command on one device's settings --- */

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT ergocai_behavior_pointer_config

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata ptr_commands[] = {
    {.display_name = "Invert X", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_INVERT_X},
    {.display_name = "Invert Y", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_INVERT_Y},
    {.display_name = "Swap X/Y", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_SWAP_XY},
    {.display_name = "Faster", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_FASTER},
    {.display_name = "Slower", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_SLOWER},
    {.display_name = "Reset", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = PTR_RESET},
};

static const struct behavior_parameter_metadata_set ptr_commands_set = {
    .param1_values = ptr_commands,
    .param1_values_len = ARRAY_SIZE(ptr_commands),
};

static const struct behavior_parameter_metadata ptr_metadata = {
    .sets_len = 1,
    .sets = &ptr_commands_set,
};
#endif

struct ptr_behavior_config {
    const struct device *processor;
};

static int ptr_pressed(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct ptr_behavior_config *cfg = dev->config;
    return ptr_command(cfg->processor, binding->param1);
}

static int ptr_released(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api ptr_behavior_api = {
    .binding_pressed = ptr_pressed,
    .binding_released = ptr_released,
    /* the settings are the central's: the keymap runs there */
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &ptr_metadata,
#endif
};

#define PTR_BEHAVIOR_INST(n)                                                                       \
    static const struct ptr_behavior_config ptr_behavior_config_##n = {                            \
        .processor = DEVICE_DT_GET(DT_INST_PHANDLE(n, processor)),                                 \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &ptr_behavior_config_##n, POST_KERNEL,           \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &ptr_behavior_api);

DT_INST_FOREACH_STATUS_OKAY(PTR_BEHAVIOR_INST)
