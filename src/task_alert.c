/*
 * task_alert.c
 * Tarea de evaluación de alertas y control del LED RGB.
 *
 * Los LEDs en la FRDM-MCXN947 son GPIO activo bajo (GPIO_ACTIVE_LOW):
 *   gpio_pin_set_dt(&led, 1) → LED encendido
 *   gpio_pin_set_dt(&led, 0) → LED apagado
 *
 * Nodos del DTS base: red_led (gpio0.10), green_led (gpio0.27), blue_led (gpio1.2)
 *
 * Colores de alerta:
 *   Verde        — nominal
 *   Amarillo R+G — IAQ malo o humedad alta
 *   Rojo         — temperatura fuera de rango
 *   Azul blink   — movimiento/vibración
 *   Magenta R+B  — múltiples alertas
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(task_alert, LOG_LEVEL_INF);

/* ── Nodos GPIO del LED RGB ──────────────────────────────────────────────
 * DT_NODELABEL(red_led) apunta al nodo red_led definido en frdm_mcxn947.dtsi
 */
static const struct gpio_dt_spec led_r =
    GPIO_DT_SPEC_GET(DT_NODELABEL(red_led),   gpios);
static const struct gpio_dt_spec led_g =
    GPIO_DT_SPEC_GET(DT_NODELABEL(green_led), gpios);
static const struct gpio_dt_spec led_b =
    GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led),  gpios);

typedef enum {
    LED_GREEN = 0,
    LED_YELLOW,
    LED_RED,
    LED_BLUE_BLINK,
    LED_MAGENTA,
} led_state_t;

static void led_set(led_state_t state, bool blink_phase)
{
    int r = 0, g = 0, b = 0;

    switch (state) {
    case LED_GREEN:       g = 1; break;
    case LED_YELLOW:      r = 1; g = 1; break;
    case LED_RED:         r = 1; break;
    case LED_BLUE_BLINK:  b = blink_phase ? 1 : 0; break;
    case LED_MAGENTA:     r = 1; b = 1; break;
    }

    gpio_pin_set_dt(&led_r, r);
    gpio_pin_set_dt(&led_g, g);
    gpio_pin_set_dt(&led_b, b);
}

static int led_init(void)
{
    if (!gpio_is_ready_dt(&led_r) ||
        !gpio_is_ready_dt(&led_g) ||
        !gpio_is_ready_dt(&led_b)) {
        LOG_ERR("GPIO LED no listo");
        return -ENODEV;
    }
    gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
    return 0;
}

static alert_status_t evaluate_alerts(const bme680_data_t *env,
                                      const lis3dh_data_t *mot)
{
    alert_status_t alr = { 0 };

    if (env->valid) {
        alr.temp_high     = (env->temperature > ALERT_TEMP_HIGH_C);
        alr.temp_low      = (env->temperature < ALERT_TEMP_LOW_C);
        alr.humidity_high = (env->humidity    > ALERT_HUMIDITY_HIGH);
        alr.iaq_bad       = (env->iaq         > ALERT_IAQ_BAD);
    }
    if (mot->valid) {
        alr.motion_detected = (mot->magnitude > ALERT_ACCEL_G + 1.0f);
    }
    alr.any_active = alr.temp_high || alr.temp_low || alr.humidity_high ||
                     alr.iaq_bad   || alr.motion_detected;
    return alr;
}

static led_state_t select_led_state(const alert_status_t *alr)
{
    if (!alr->any_active)                                          return LED_GREEN;
    if (alr->motion_detected && !alr->temp_high && !alr->temp_low) return LED_BLUE_BLINK;
    if (alr->temp_high || alr->temp_low) {
        if (alr->iaq_bad || alr->motion_detected)                  return LED_MAGENTA;
        return LED_RED;
    }
    if (alr->iaq_bad || alr->humidity_high)                        return LED_YELLOW;
    return LED_GREEN;
}

void task_alert_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    if (led_init() != 0) {
        LOG_ERR("Fallo al inicializar LED RGB.");
        return;
    }

    LOG_INF("Tarea Alertas iniciada. Período: %u ms", ALERT_PERIOD_MS);

    bme680_data_t env;
    lis3dh_data_t mot;
    bool blink_phase = false;

    while (1) {
        k_sem_take(&g_data_ready_sem, K_MSEC(ALERT_PERIOD_MS));

        k_mutex_lock(&g_data_mutex, K_FOREVER);
        env = g_bme680;
        mot = g_lis3dh;
        k_mutex_unlock(&g_data_mutex);

        alert_status_t alr = evaluate_alerts(&env, &mot);

        k_mutex_lock(&g_data_mutex, K_FOREVER);
        g_alerts = alr;
        k_mutex_unlock(&g_data_mutex);

        blink_phase = !blink_phase;
        led_set(select_led_state(&alr), blink_phase);

        if (alr.any_active) {
            LOG_WRN("ALERTA: %s%s%s%s%s",
                alr.temp_high       ? "TEMP_ALTA "  : "",
                alr.temp_low        ? "TEMP_BAJA "  : "",
                alr.humidity_high   ? "HUMEDAD "    : "",
                alr.iaq_bad         ? "AIRE_MALO "  : "",
                alr.motion_detected ? "MOVIMIENTO " : "");
        }
    }
}
