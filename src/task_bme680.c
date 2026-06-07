/*
 * task_bme680.c
 * Tarea de adquisición del sensor ambiental BME680 (I2C).
 * Lee temperatura, humedad, presión y gas (IAQ aproximado)
 * cada BME680_PERIOD_MS milisegundos.
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <math.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(task_bme680, LOG_LEVEL_DBG);

/* ── Conversión resistencia de gas → IAQ (aproximación simple) ─────────────
 * El driver Zephyr de BME680 no implementa el algoritmo BSEC de Bosch.
 * Usamos una conversión logarítmica normalizada:
 *   IAQ = 500 * (1 - log10(R_gas) / log10(R_max))
 * donde R_max ≈ 500 kΩ (aire limpio seco)                                   */
#define GAS_REFERENCE_OHM   500000.0f

static float gas_resistance_to_iaq(float gas_ohm)
{
    if (gas_ohm <= 0.0f) {
        return 500.0f; /* valor de error: muy malo */
    }
    float ratio = gas_ohm / GAS_REFERENCE_OHM;
    /* IAQ 0 = excelente, 500 = peligroso */
    float iaq = 500.0f * (1.0f - (logf(gas_ohm) / logf(GAS_REFERENCE_OHM)));
    if (iaq < 0.0f)   iaq = 0.0f;
    if (iaq > 500.0f) iaq = 500.0f;
    (void)ratio;
    return iaq;
}

/* ── Entrada de tarea ─────────────────────────────────────────────────────── */
void task_bme680_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct device *dev = (const struct device *)p1;
    struct sensor_value  temp, hum, press, gas;
    bme680_data_t local = { 0 };

    LOG_INF("Tarea BME680 iniciada. Período: %u ms", BME680_PERIOD_MS);

    while (1) {
        /* Trigger de medición */
        int rc = sensor_sample_fetch(dev);
        if (rc != 0) {
            LOG_ERR("sensor_sample_fetch BME680: %d", rc);
            k_sleep(K_MSEC(BME680_PERIOD_MS));
            continue;
        }

        sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY,     &hum);
        sensor_channel_get(dev, SENSOR_CHAN_PRESS,        &press);
        sensor_channel_get(dev, SENSOR_CHAN_GAS_RES,      &gas);

        /* Convertir sensor_value → float */
        local.temperature  = sensor_value_to_double(&temp);
        local.humidity     = sensor_value_to_double(&hum);
        local.pressure     = sensor_value_to_double(&press) / 1000.0f; /* Pa → hPa */
        local.iaq          = gas_resistance_to_iaq(sensor_value_to_double(&gas));
        local.timestamp_ms = k_uptime_get_32();
        local.valid        = true;

        LOG_DBG("BME680 | T=%.2f°C  HR=%.1f%%  P=%.1fhPa  IAQ=%.0f",
                (double)local.temperature,
                (double)local.humidity,
                (double)local.pressure,
                (double)local.iaq);

        /* Actualizar variable global protegida por mutex */
        k_mutex_lock(&g_data_mutex, K_FOREVER);
        g_bme680 = local;
        k_mutex_unlock(&g_data_mutex);

        /* Notificar a la tarea de alertas */
        k_sem_give(&g_data_ready_sem);

        k_sleep(K_MSEC(BME680_PERIOD_MS));
    }
}
