/*
 * task_touch.c
 * Tarea de lectura del touch slider embebido en la FRDM-MCXN947.
 * Detecta la posición del dedo sobre el slider y alterna el modo de
 * visualización entre los cinco modos disponibles.
 *
 * El touch slider de la FRDM-MCXN947 usa el periférico TSI (Touch Sensing
 * Input) de NXP. En Zephyr se accede mediante el driver de ADC capacitivo
 * o, si está disponible, por el driver kscan/tsc.
 *
 * Estrategia de detección:
 *   - Se leen N muestras del canal ADC asociado al slider.
 *   - Si el valor supera el umbral de detección de toque, se determina
 *     la zona (izquierda / derecha) comparando con el punto medio.
 *   - Un deslizamiento hacia la derecha avanza el modo.
 *   - Un deslizamiento hacia la izquierda retrocede el modo.
 *   - Hay un debounce de TOUCH_DEBOUNCE_MS entre cambios.
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(task_touch, LOG_LEVEL_INF);

/* ── Parámetros del ADC / touch ──────────────────────────────────────────── */
#define TOUCH_ADC_CHANNEL      0
#define TOUCH_ADC_RESOLUTION   12
#define TOUCH_ADC_VREF_MV      3300

/* Umbral de detección de toque (valor ADC raw, 12 bits) */
#define TOUCH_THRESHOLD_RAW    2000
/* Punto medio: valores < este → zona izquierda; >= → zona derecha */
#define TOUCH_MIDPOINT_RAW     2048

/* Debounce mínimo entre cambios de modo */
#define TOUCH_DEBOUNCE_MS      400U

static const struct device *adc_dev = DEVICE_DT_GET(DT_ALIAS(touch_adc));

static const struct adc_channel_cfg ch_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = TOUCH_ADC_CHANNEL,
};

static int16_t  adc_buf[1];
static struct adc_sequence adc_seq = {
    .channels    = BIT(TOUCH_ADC_CHANNEL),
    .buffer      = adc_buf,
    .buffer_size = sizeof(adc_buf),
    .resolution  = TOUCH_ADC_RESOLUTION,
};

/* ── Avanzar o retroceder el modo de display ────────────────────────────── */
static void mode_advance(bool forward)
{
    k_mutex_lock(&g_data_mutex, K_FOREVER);

    int m = (int)g_display_mode;
    if (forward) {
        m = (m + 1) % DISPLAY_MODE_COUNT;
    } else {
        m = (m - 1 + DISPLAY_MODE_COUNT) % DISPLAY_MODE_COUNT;
    }
    g_display_mode = (display_mode_t)m;

    k_mutex_unlock(&g_data_mutex);

    LOG_INF("Modo cambiado → %d", m);
}

/* ── Entrada de tarea ─────────────────────────────────────────────────────── */
void task_touch_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC no listo. Tarea touch terminada.");
        return;
    }

    int rc = adc_channel_setup(adc_dev, &ch_cfg);
    if (rc != 0) {
        LOG_ERR("adc_channel_setup: %d", rc);
        return;
    }

    LOG_INF("Tarea Touch iniciada. Período: %u ms", TOUCH_PERIOD_MS);

    uint32_t last_change_ms = 0;
    bool     was_touched    = false;
    bool     last_right     = false;  /* última zona tocada */

    while (1) {
        k_sleep(K_MSEC(TOUCH_PERIOD_MS));

        rc = adc_read(adc_dev, &adc_seq);
        if (rc != 0) {
            LOG_DBG("adc_read error: %d", rc);
            continue;
        }

        int16_t raw = adc_buf[0];
        bool touched = (raw > TOUCH_THRESHOLD_RAW);

        if (touched && !was_touched) {
            /* Flanco de subida: inicio del toque */
            uint32_t now = k_uptime_get_32();
            if ((now - last_change_ms) >= TOUCH_DEBOUNCE_MS) {
                bool right_zone = (raw >= TOUCH_MIDPOINT_RAW);
                mode_advance(right_zone);
                last_right     = right_zone;
                last_change_ms = now;
                LOG_DBG("Touch raw=%d zona=%s", raw,
                        right_zone ? "DERECHA" : "IZQUIERDA");
            }
        }

        was_touched = touched;
        (void)last_right;
    }
}
