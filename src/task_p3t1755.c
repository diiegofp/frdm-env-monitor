/*
 * task_p3t1755.c
 * Tarea de adquisición del sensor de temperatura P3T1755DP por I3C.
 * El P3T1755DP es el sensor de temperatura digital embebido en la tarjeta
 * FRDM-MCXN947. Se conecta a través del bus I3C0.
 *
 * Resolución: 11 bits → 0.125 °C/LSB
 * Rango: -55 °C … +125 °C
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(task_p3t1755, LOG_LEVEL_DBG);

void task_p3t1755_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct device *dev = (const struct device *)p1;
    struct sensor_value  temp;
    p3t1755_data_t local = { 0 };

    LOG_INF("Tarea P3T1755DP iniciada. Período: %u ms", P3T1755_PERIOD_MS);

    while (1) {
        int rc = sensor_sample_fetch(dev);
        if (rc != 0) {
            LOG_ERR("sensor_sample_fetch P3T1755: %d", rc);
            k_sleep(K_MSEC(P3T1755_PERIOD_MS));
            continue;
        }

        sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);

        local.temperature  = sensor_value_to_double(&temp);
        local.timestamp_ms = k_uptime_get_32();
        local.valid        = true;

        LOG_DBG("P3T1755 | T_ref=%.3f°C", (double)local.temperature);

        k_mutex_lock(&g_data_mutex, K_FOREVER);
        g_p3t1755 = local;
        k_mutex_unlock(&g_data_mutex);

        k_sleep(K_MSEC(P3T1755_PERIOD_MS));
    }
}
