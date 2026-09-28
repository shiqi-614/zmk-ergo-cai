/*
 * Copyright (c) 2022 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_analog_input

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input.h>
#include <stdlib.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/gpio/nordic-nrf-gpio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ANALOG_INPUT, CONFIG_ANALOG_INPUT_LOG_LEVEL);

#include "analog_input.h"

/* 硬编码供电开启后的硬件稳定延迟 (毫秒) */
#define POWER_ON_DELAY_MS 20
/* 开机自动校准参数 */
#define CALIBRATION_SAMPLES 32      /* 校准采样次数 */
#define CALIBRATION_INTERVAL_MS 10 /* 采样间隔毫秒 (32 * 10ms = 320ms 校准时长) */

static void set_sampling_timer_hz(struct analog_input_data *data, uint32_t hz) {
    if (hz == 0) {
        k_timer_stop(&data->sampling_timer);
        return;
    }
    uint32_t usec = 1000000UL / hz;
    k_timer_start(&data->sampling_timer, K_USEC(usec), K_USEC(usec));
}

static int analog_input_report_data(const struct device *dev) {
    struct analog_input_data *data = dev->data;
    const struct analog_input_config *config = dev->config;

    if (unlikely(!data->ready)) {
        LOG_WRN("Device is not initialized yet");
        return -EBUSY;
    }

    /* 1. 动态供电：采样前拉高 GPIO，硬编码延迟等待电平稳定 */
    if (config->power_gpio.port != NULL) {
        /* gpio_pin_configure_dt(&config->power_gpio, GPIO_OUTPUT_ACTIVE); */
        gpio_pin_set_dt(&config->power_gpio, 1); // 1 代表 Active (高电平)
#if POWER_ON_DELAY_MS > 0
        k_msleep(POWER_ON_DELAY_MS);
#endif
    }

#if CONFIG_ANALOG_INPUT_REPORT_INTERVAL_MIN > 0
    static int64_t last_smp_time = 0;
    static int64_t last_rpt_time = 0;
    int64_t now = k_uptime_get();
#endif

    struct adc_sequence* as = &data->as;

    for (uint8_t i = 0; i < config->io_channels_len; i++) {
        struct analog_input_io_channel ch_cfg = (struct analog_input_io_channel)config->io_channels[i];
        const struct device* adc = ch_cfg.adc_channel.dev;

        if (i == 0) {
#ifdef CONFIG_ADC_ASYNC
            int err = adc_read_async(adc, as, &data->async_sig);
            if (err < 0) {
                LOG_ERR("AIN%u read_async returned %d", i, err);
                goto cleanup_power;
            }
            err = k_poll(&data->async_evt, 1, K_FOREVER);
            if (err < 0) {
                LOG_ERR("AIN%u k_poll returned %d", i, err);
                goto cleanup_power;
            }
            if (!data->async_evt.signal->signaled) {
                goto cleanup_power;
            }
            data->async_evt.signal->signaled = 0;
            data->async_evt.state = K_POLL_STATE_NOT_READY;
#else
            int err = adc_read(adc, as);
            if (err < 0) {
                LOG_ERR("AIN%u read returned %d", i, err);
                goto cleanup_power;
            }
#endif
        }

        int32_t raw = data->as_buff[i];
#if CONFIG_ANALOG_INPUT_ADC_RES > 16
        raw = ((uint32_t *)data->as_buff)[i];
#else
        raw = ((uint16_t *)data->as_buff)[i];
#endif
        int32_t mv = raw;
        adc_raw_to_millivolts_dt(&ch_cfg.adc_channel, &mv);

        int32_t mid = (data->calibrated_mv_mid != NULL) ? data->calibrated_mv_mid[i] : ch_cfg.mv_min_max;
        int16_t v = mv - mid;
        int16_t dz = ch_cfg.mv_deadzone;

#if IS_ENABLED(CONFIG_ANALOG_INPUT_LOG_DBG_REPORT)
        LOG_INF("AIN%u raw: %d, mv: %d, mid: %d, v: %d, dz: %d",
                i,
                raw,
                mv,
                mid,
                v,
                dz);
#endif

        if (dz) {
            if (v > 0) {
                if (v < dz) v = 0; else v -= dz;
            }
            if (v < 0) {
                if (v > -dz) v = 0; else v += dz;
            }
        }
        uint16_t mm = ch_cfg.mv_min_max;
        if (mm) {
            if (v > 0 && v > mm) v = mm;
            if (v < 0 && v < -mm) v = -mm;
        }

        if (ch_cfg.invert) v *= -1;
        v = (int16_t)((v * ch_cfg.scale_multiplier) / ch_cfg.scale_divisor);

        if (ch_cfg.report_on_change_only) {
            data->delta[i] = v;
        } else {
            int32_t delta = data->delta[i];
            int32_t dv = delta + v;
            data->delta[i] = dv;
        }
    }

    as->calibrate = false;

#if CONFIG_ANALOG_INPUT_REPORT_INTERVAL_MIN > 0
    if (now - last_smp_time >= CONFIG_ANALOG_INPUT_REPORT_INTERVAL_MIN) {
        for (uint8_t i = 0; i < config->io_channels_len; i++) {
            data->delta[i] = 0;
            data->prev[i] = 0;
        }
    }
    last_smp_time = now;

    if (now - last_rpt_time < CONFIG_ANALOG_INPUT_REPORT_INTERVAL_MIN) {
        goto cleanup_power;
    }
#endif

    if (!data->actived) {
        goto cleanup_power;
    }

    int8_t idx_to_sync = -1;
    for (int8_t i = config->io_channels_len - 1; i >= 0; i--) {
        int32_t dv = data->delta[i];
        int32_t pv = data->prev[i];
        if (dv != pv) {
            idx_to_sync = i;
            break;
        }
    }

    bool has_activity = false;

    for (uint8_t i = 0; i < config->io_channels_len; i++) {
        struct analog_input_io_channel ch_cfg = (struct analog_input_io_channel)config->io_channels[i];
        int32_t dv = data->delta[i];
        int32_t pv = data->prev[i];
        if (dv != pv) {
#if CONFIG_ANALOG_INPUT_REPORT_INTERVAL_MIN > 0
            last_rpt_time = now;
#endif
            data->delta[i] = 0;
            if (ch_cfg.report_on_change_only) {
                data->prev[i] = dv;
            }

            /* 只有真正产生非零上报数据时，才算作有效活动 */
            if (dv != 0) {
                has_activity = true;
            }

#if IS_ENABLED(CONFIG_ANALOG_INPUT_LOG_DBG_REPORT)
            LOG_INF("input_report %u (idx_sync:%u, is_sync:%d) rv: %d  e:%d  c:%d",
                    i,
                    idx_to_sync,
                    (i == idx_to_sync),
                    dv,
                    ch_cfg.evt_type,
                    ch_cfg.input_code);
#endif
            input_report(dev, ch_cfg.evt_type, ch_cfg.input_code, dv, i == idx_to_sync, K_NO_WAIT);
        }
    }

    /* 重构后的 Idle 降频控制逻辑 */
    int64_t current_time = k_uptime_get();
    if (has_activity) {
        data->last_activity_time = current_time;
        if (data->is_idle) {
            data->is_idle = false;
            LOG_INF("Joystick active! Switch to high sampling rate: %u Hz", config->sampling_hz);
            set_sampling_timer_hz(data, config->sampling_hz);
        }
    } else {
        if (!data->is_idle && config->idle_sampling_hz > 0 && config->idle_timeout_ms > 0) {
            if ((current_time - data->last_activity_time) >= config->idle_timeout_ms) {
                data->is_idle = true;
                LOG_INF("Joystick idle timeout. Switch to low sampling rate: %u Hz", config->idle_sampling_hz);
                set_sampling_timer_hz(data, config->idle_sampling_hz);
            }
        }
    }

cleanup_power:
    /* 采样结束：断开分压电路供电 */
    if (config->power_gpio.port != NULL) {
        /* gpio_pin_configure_dt(&config->power_gpio, GPIO_OUTPUT_INACTIVE); */
        gpio_pin_set_dt(&config->power_gpio, 0); // 0 代表 Inactive (低电平)
    }
    return 0;
}

/* 开机静态校准实现：连续采样 CALIBRATION_SAMPLES 次取平均值 */
static void perform_calibration(const struct device *dev) {
    struct analog_input_data *data = dev->data;
    const struct analog_input_config *config = dev->config;

    LOG_INF("Starting analog input auto-calibration...");

    int32_t *sum_mv = calloc(config->io_channels_len, sizeof(int32_t));
    if (!sum_mv) return;

    if (config->power_gpio.port != NULL) {
        /* gpio_pin_configure_dt(&config->power_gpio, GPIO_OUTPUT_ACTIVE); */
        gpio_pin_set_dt(&config->power_gpio, 1);
        k_msleep(POWER_ON_DELAY_MS);
    }

    for (int sample = 0; sample < CALIBRATION_SAMPLES; sample++) {
        for (uint8_t i = 0; i < config->io_channels_len; i++) {
            struct analog_input_io_channel ch_cfg = (struct analog_input_io_channel)config->io_channels[i];
            const struct device* adc = ch_cfg.adc_channel.dev;

            if (i == 0) {
                adc_read(adc, &data->as);
            }

            int32_t raw = data->as_buff[i];
#if CONFIG_ANALOG_INPUT_ADC_RES > 16
            raw = ((uint32_t *)data->as_buff)[i];
#else
            raw = ((uint16_t *)data->as_buff)[i];
#endif
            int32_t mv = raw;
            adc_raw_to_millivolts(
                adc_ref_internal(adc),
                ch_cfg.adc_channel.channel_cfg.gain,
                data->as.resolution,
                &mv
            );
            sum_mv[i] += mv;
        }
        k_msleep(CALIBRATION_INTERVAL_MS);
    }

    if (config->power_gpio.port != NULL) {
        /* gpio_pin_configure_dt(&config->power_gpio, GPIO_OUTPUT_INACTIVE); */
        gpio_pin_set_dt(&config->power_gpio, 0);
    }

    for (uint8_t i = 0; i < config->io_channels_len; i++) {
        data->calibrated_mv_mid[i] = sum_mv[i] / CALIBRATION_SAMPLES;
        LOG_INF("Channel %u calibrated mv_mid: %d (DTS fallback: %d)", 
                i, data->calibrated_mv_mid[i], config->io_channels[i].mv_min_max);
    }

    free(sum_mv);
}

K_THREAD_STACK_DEFINE(analog_input_q_stack, CONFIG_ANALOG_INPUT_WORKQUEUE_STACK_SIZE);
static struct k_work_q analog_input_work_q;

static void sampling_work_handler(struct k_work *work) {
    struct analog_input_data *data = CONTAINER_OF(work, struct analog_input_data, sampling_work);
    analog_input_report_data(data->dev);
}

static void sampling_timer_handler(struct k_timer *timer) {
    struct analog_input_data *data = CONTAINER_OF(timer, struct analog_input_data, sampling_timer);
    k_work_submit_to_queue(&analog_input_work_q, &data->sampling_work);
}

static int active_set_value(const struct device *dev, bool active) {
    struct analog_input_data *data = dev->data;
    if (data->actived == active) return 0;
    LOG_DBG("%d", active ? 1 : 0);
    data->actived = active;
    return 0;
}

static int sample_hz_set_value(const struct device *dev, uint32_t hz) {
    struct analog_input_data *data = dev->data;

    if (unlikely(!data->ready)) {
        LOG_DBG("Device is not initialized yet");
        return -EBUSY;
    }

    if (data->enabled) {
        LOG_DBG("Device is busy, would not update sampling rate in enable state.");
        return -EBUSY;
    }

    LOG_DBG("%d", hz);
    data->sampling_hz = hz;
    return 0;
}

static int enable_set_value(const struct device *dev, bool enable) {
    struct analog_input_data *data = dev->data;

    if (unlikely(!data->ready)) {
        LOG_DBG("Device is not initialized yet");
        return -EBUSY;
    }

    if (data->enabled == enable) {
        return 0;
    }
    
    LOG_DBG("%d", enable ? 1 : 0);
    if (enable) {
        data->is_idle = false;
        data->last_activity_time = k_uptime_get();
        set_sampling_timer_hz(data, data->sampling_hz);
        data->enabled = true;
    } else {
        k_timer_stop(&data->sampling_timer);
        data->enabled = false;
    }

    return 0;
}

static void analog_input_async_init(struct k_work *work) {
    struct k_work_delayable *work_delayable = (struct k_work_delayable *)work;
    struct analog_input_data *data = CONTAINER_OF(work_delayable, struct analog_input_data, init_work);
    const struct device *dev = data->dev;
    const struct analog_input_config *config = dev->config;

    uint32_t ch_mask = 0;

    if (config->power_gpio.port != NULL) {
        LOG_INF("Power GPIO Configured: port=%s, pin=%u, flags=0x%x",
                config->power_gpio.port->name,
                config->power_gpio.pin,
                config->power_gpio.dt_flags);
        if (!gpio_is_ready_dt(&config->power_gpio)) {
            LOG_ERR("Power GPIO device is not ready");
        } else {
            int ret = gpio_pin_configure_dt(&config->power_gpio,
                                GPIO_OUTPUT_INACTIVE | NRF_GPIO_DRIVE_H0H1);
            if (ret < 0) {
                LOG_ERR("Failed to configure power GPIO: %d", ret);
            }
        }
    }

    for (uint8_t i = 0; i < config->io_channels_len; i++) {
        struct analog_input_io_channel ch_cfg = (struct analog_input_io_channel)config->io_channels[i];
        const struct device* adc = ch_cfg.adc_channel.dev;
        uint8_t channel_id = ch_cfg.adc_channel.channel_id;
        struct adc_channel_cfg channel_cfg = ch_cfg.adc_channel.channel_cfg;

        ch_mask |= BIT(channel_id);

        if (!device_is_ready(adc)) {
            LOG_ERR("AIN%u device is not ready %s", i, adc->name);
            continue;
        }

        int err = adc_channel_setup(adc, &channel_cfg);
        if (err < 0) {
            LOG_ERR("AIN%u setup returned %d", i, err);
        }
    }

    uint16_t delta_size = config->io_channels_len * sizeof(int32_t);
    data->delta = malloc(delta_size);
    memset(data->delta, 0, delta_size);

    uint16_t prev_size = config->io_channels_len * sizeof(int32_t);
    data->prev = malloc(prev_size);
    memset(data->prev, 0, prev_size);

    data->calibrated_mv_mid = malloc(config->io_channels_len * sizeof(int32_t));

#if CONFIG_ANALOG_INPUT_ADC_RES > 16
    uint16_t buff_size = config->io_channels_len * sizeof(uint32_t);
#else
    uint16_t buff_size = config->io_channels_len * sizeof(uint16_t);
#endif
    data->as_buff = malloc(buff_size);
    memset(data->as_buff, 0, buff_size);

    data->as = (struct adc_sequence){
        .channels = ch_mask,
        .buffer = data->as_buff,
        .buffer_size = buff_size,
        .oversampling = 0,
        .resolution = CONFIG_ANALOG_INPUT_ADC_RES,
        .calibrate = true,
    };

#ifdef CONFIG_ADC_ASYNC
    k_poll_signal_init(&data->async_sig);
    struct k_poll_event async_evt = K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                                             K_POLL_MODE_NOTIFY_ONLY,
                                                             &data->async_sig);
    data->async_evt = async_evt;
#endif

    /* 执行开机平均值自动校准 */
    perform_calibration(dev);

    data->ready = true;

    k_work_init(&data->sampling_work, sampling_work_handler);
    k_work_queue_start(&analog_input_work_q,
                       analog_input_q_stack, K_THREAD_STACK_SIZEOF(analog_input_q_stack),
                       CONFIG_ANALOG_INPUT_WORKQUEUE_PRIORITY, NULL);

    k_timer_init(&data->sampling_timer, sampling_timer_handler, NULL);

    sample_hz_set_value(dev, config->sampling_hz);
    active_set_value(dev, true);
    if (data->sampling_hz) {
        enable_set_value(dev, true);
    }
}

static int analog_input_init(const struct device *dev) {
    LOG_INF("analog_input_init");
    struct analog_input_data *data = dev->data;
    data->dev = dev;
    k_work_init_delayable(&data->init_work, analog_input_async_init);
    k_work_schedule(&data->init_work, K_MSEC(1));
    return 0;
}

#if IS_ENABLED(CONFIG_PM_DEVICE)
static int analog_input_pm_action(const struct device *dev, enum pm_device_action action) {
    struct analog_input_data *data = dev->data;
    const struct analog_input_config *config = dev->config;

    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        k_timer_stop(&data->sampling_timer);
        if (config->power_gpio.port != NULL) {
            gpio_pin_configure_dt(&config->power_gpio, GPIO_OUTPUT_INACTIVE);
        }
        break;
    case PM_DEVICE_ACTION_RESUME:
        if (data->enabled) {
            set_sampling_timer_hz(data, data->is_idle ? config->idle_sampling_hz : data->sampling_hz);
        }
        break;
    default:
        return -ENOTSUP;
    }
    return 0;
}
#endif

static int analog_input_attr_set(const struct device *dev, enum sensor_channel chan,
                                 enum sensor_attribute attr, const struct sensor_value *val) {
    struct analog_input_data *data = dev->data;

    if (chan != SENSOR_CHAN_ALL) {
        return -ENOTSUP;
    }
    if (unlikely(!data->ready)) {
        return -EBUSY;
    }

    switch ((uint32_t)attr) {
    case ANALOG_INPUT_ATTR_SAMPLING_HZ:
        return sample_hz_set_value(dev, ANALOG_INPUT_SVALUE_TO_SAMPLING_HZ(*val));
    case ANALOG_INPUT_ATTR_ENABLE:
        return enable_set_value(dev, ANALOG_INPUT_SVALUE_TO_ENABLE(*val));
    case ANALOG_INPUT_ATTR_ACTIVE:
        return active_set_value(dev, ANALOG_INPUT_SVALUE_TO_ACTIVE(*val));
    default:
        return -ENOTSUP;
    }
}

static int analog_input_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    struct analog_input_data *data = dev->data;

    if (chan != SENSOR_CHAN_ALL) {
        return -ENOTSUP;
    }
    if (unlikely(!data->ready)) {
        return -EBUSY;
    }

    return analog_input_report_data(data->dev);
}

static int analog_input_channel_get(const struct device *dev, enum sensor_channel chan,
                                    struct sensor_value *val) {
    struct analog_input_data *data = dev->data;
    const struct analog_input_config *config = dev->config;

    if (unlikely(chan != SENSOR_CHAN_ALL)) {
        return -ENOTSUP;
    }
    if (unlikely(!data->ready)) {
        return -EBUSY;
    }

    for (uint8_t i = 0; i < config->io_channels_len; i++) {
        struct analog_input_io_channel ch_cfg = (struct analog_input_io_channel)config->io_channels[i];
        if (!ch_cfg.report_on_change_only) {
            continue;
        }
        if (i == 0)      val->val1 = data->delta[i];
        else if (i == 1) val->val2 = data->delta[i];
    }

    return 0;
}

static const struct sensor_driver_api analog_input_driver_api = {
    .attr_set = analog_input_attr_set,
    .sample_fetch = analog_input_sample_fetch,
    .channel_get = analog_input_channel_get,
};

#define TRANSFORMED_IO_CHANNEL_ENTRY(node_id)                                                       \
    {                                                                                               \
        .adc_channel = ADC_DT_SPEC_GET_BY_IDX(node_id, 0),                                         \
        .mv_min_max = DT_PROP(node_id, mv_min_max),                                                 \
        .mv_deadzone = DT_PROP(node_id, mv_deadzone),                                               \
        .invert = DT_PROP(node_id, invert),                                                         \
        .report_on_change_only = DT_PROP(node_id, report_on_change_only),                           \
        .scale_multiplier = DT_PROP(node_id, scale_multiplier),                                     \
        .scale_divisor = DT_PROP(node_id, scale_divisor),                                           \
        .evt_type = DT_PROP(node_id, evt_type),                                                     \
        .input_code = DT_PROP(node_id, input_code),                                                 \
    }

#define ANIN_IOC_CHILD_LEN_PLUS_ONE(node) 1 +

#define ANALOG_INPUT_DEFINE(n)                                                                      \
    static struct analog_input_data data##n = {};                                                   \
    static const struct analog_input_config config##n = {                                           \
        .sampling_hz = DT_PROP(DT_DRV_INST(n), sampling_hz),                                        \
        .power_gpio = GPIO_DT_SPEC_INST_GET_OR(n, power_gpios, {0}),                               \
        .idle_sampling_hz = DT_INST_PROP_OR(n, idle_sampling_hz, 0),                               \
        .idle_timeout_ms = DT_INST_PROP_OR(n, idle_timeout_ms, 0),                                 \
        .io_channels_len = (DT_FOREACH_CHILD(DT_DRV_INST(n), ANIN_IOC_CHILD_LEN_PLUS_ONE) 0),       \
        .io_channels = { DT_INST_FOREACH_CHILD_SEP(n, TRANSFORMED_IO_CHANNEL_ENTRY, (, )) },        \
    };                                                                                              \
                                                                                                    \
    PM_DEVICE_DT_INST_DEFINE(n, analog_input_pm_action);                                           \
                                                                                                    \
    DEVICE_DT_INST_DEFINE(n, analog_input_init, PM_DEVICE_DT_INST_GET(n),                          \
                          &data##n, &config##n, POST_KERNEL,                                        \
                          CONFIG_SENSOR_INIT_PRIORITY, &analog_input_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ANALOG_INPUT_DEFINE)
