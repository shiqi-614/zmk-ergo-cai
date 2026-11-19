/*
 * Which module sits on the joystick mount (ErgoCai.modules joystick-mount-pogo): the trackball (PMW3610) or the
 * joystick. They share the mount's six pins: the stick's wipers are the sensor's SCLK and SDIO, its push switch the
 * sensor's MOTION, its supply the module's. The pogo_detect shield defers both modules' devices, and this starts one
 * of them once at boot: it turns the module's supply on, writes the sensor's power-up reset and reads its product
 * ID twice over the trackball's SPI. 0x3e both times is a trackball: its supply, the sensor and the physical layout
 * whose kscan is the matrix alone (the switch's pin is the sensor's MOTION line) are started. Anything else is the
 * joystick: the SPI's pins go to their sleep state, disconnected, so that the ADC reads the wipers, and the joystick
 * starts with the stock layout.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include <zmk/physical_layouts.h>

LOG_MODULE_REGISTER(pogo_detect, CONFIG_ZMK_LOG_LEVEL);

#define TRACKBALL_NODE DT_NODELABEL(trackball)
#define POWER_NODE DT_NODELABEL(trackball_power)
#define JOYSTICK_NODE DT_NODELABEL(joystick)

/* PMW3610DM-SUDU registers, as Zephyr's driver has them (drivers/input/input_pmw3610.c) */
#define PMW3610_PROD_ID 0x00
#define PMW3610_POWER_UP_RESET 0x3a
#define POWER_UP_RESET 0x5a
#define PRODUCT_ID_PMW3610 0x3e
#define SPI_WRITE BIT(7)
#define POWER_UP_MS 50
#define RESET_MS 50
#define DETECT_DELAY_MS 200 /* after settings_load() in main(), which restores a saved physical layout */

static const struct spi_dt_spec spi = SPI_DT_SPEC_GET(
    TRACKBALL_NODE, SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_TRANSFER_MSB, 0);
static const struct gpio_dt_spec power = GPIO_DT_SPEC_GET(POWER_NODE, enable_gpios);

/* 3-wire SPI: SDIO is the SPIM's MOSI and MISO, so a read sends the address and then reads over the same pin */
static int read_reg(uint8_t addr, uint8_t *value) {
    const struct spi_buf tx_buf = {.buf = &addr, .len = 1};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    struct spi_buf rx_buf[] = {{.buf = NULL, .len = 1}, {.buf = value, .len = 1}};
    const struct spi_buf_set rx = {.buffers = rx_buf, .count = ARRAY_SIZE(rx_buf)};
    return spi_transceive_dt(&spi, &tx, &rx);
}

static int write_reg(uint8_t addr, uint8_t value) {
    uint8_t buf[] = {addr | SPI_WRITE, value};
    const struct spi_buf tx_buf = {.buf = buf, .len = sizeof(buf)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    return spi_write_dt(&spi, &tx);
}

static bool trackball_found(void) {
    int err = device_init(spi.bus);
    if (err < 0 && !device_is_ready(spi.bus)) {
        LOG_ERR("%s did not start: %d", spi.bus->name, err);
        return false;
    }
    k_msleep(POWER_UP_MS);
    if (write_reg(PMW3610_POWER_UP_RESET, POWER_UP_RESET) < 0) {
        return false;
    }
    k_msleep(RESET_MS);
    uint8_t first = 0, second = 0;
    if (read_reg(PMW3610_PROD_ID, &first) < 0 || read_reg(PMW3610_PROD_ID, &second) < 0) {
        return false;
    }
    LOG_DBG("product id 0x%02x 0x%02x", first, second);
    return first == PRODUCT_ID_PMW3610 && second == PRODUCT_ID_PMW3610;
}

/* the physical layout that scans with this kscan, selected unless it is already */
static void select_layout(const struct device *kscan) {
    struct zmk_physical_layout const *const *layouts;
    size_t count = zmk_physical_layouts_get_list(&layouts);
    for (size_t i = 0; i < count; i++) {
        if (layouts[i]->kscan == kscan) {
            if (zmk_physical_layouts_get_selected() != (int)i) {
                zmk_physical_layouts_select(i);
            }
            return;
        }
    }
    LOG_WRN("no physical layout scans with %s", kscan->name);
}

static void detect(struct k_work *work) {
    /* the module's supply, for either module while probing */
    gpio_pin_configure_dt(&power, GPIO_OUTPUT_ACTIVE);

    if (trackball_found()) {
        int err = device_init(DEVICE_DT_GET(POWER_NODE));
        if (err == 0) {
            err = device_init(DEVICE_DT_GET(TRACKBALL_NODE));
        }
        LOG_INF("trackball module%s", err ? ", which did not start" : "");
#if DT_NODE_EXISTS(DT_NODELABEL(kscan_matrix))
        select_layout(DEVICE_DT_GET(DT_NODELABEL(kscan_matrix)));
#endif
        return;
    }

    if (device_is_ready(spi.bus)) {
        pm_device_action_run(spi.bus, PM_DEVICE_ACTION_SUSPEND);
    }
    gpio_pin_configure_dt(&power, GPIO_OUTPUT_INACTIVE);
    int err = device_init(DEVICE_DT_GET(JOYSTICK_NODE));
    LOG_INF("joystick module%s", err ? ", which did not start" : "");
#if DT_HAS_CHOSEN(zmk_kscan)
    select_layout(DEVICE_DT_GET(DT_CHOSEN(zmk_kscan)));
#endif
}

static K_WORK_DELAYABLE_DEFINE(detect_work, detect);

static int pogo_detect_init(void) {
    k_work_schedule(&detect_work, K_MSEC(DETECT_DELAY_MS));
    return 0;
}

SYS_INIT(pogo_detect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
