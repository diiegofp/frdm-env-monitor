/*
 * task_lis3dh.c
 * Tarea de adquisición del acelerómetro LIS3DH (SPI).
 * Lee aceleración en X, Y, Z a 10 Hz (100 ms).
 * Calcula la magnitud del vector y detecta eventos de movimiento/vibración.
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

LOG_MODULE_REGISTER(task_lis3dh, LOG_LEVEL_DBG);

/* Umbral de magnitud para considerar "en reposo" (1g ± margen) */
#define REST_MAG_MIN   0.85f
#define REST_MAG_MAX   1.15f

void task_lis3dh_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct device *dev = (const struct device *)p1;
    struct sensor_value  ax, ay, az;
    lis3dh_data_t local  = { 0 };

    LOG_INF("Tarea LIS3DH iniciada. Período: %u ms", LIS3DH_PERIOD_MS);

    while (1) {
        int rc = sensor_sample_fetch(dev);
        if (rc != 0) {
            LOG_ERR("sensor_sample_fetch LIS3DH: %d", rc);
            k_sleep(K_MSEC(LIS3DH_PERIOD_MS));
            continue;
        }

        sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &ax);
        sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &ay);
        sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &az);

        /* m/s² → g  (dividir entre 9.80665) */
        local.accel_x = sensor_value_to_double(&ax) / 9.80665f;
        local.accel_y = sensor_value_to_double(&ay) / 9.80665f;
        local.accel_z = sensor_value_to_double(&az) / 9.80665f;

        local.magnitude    = sqrtf(local.accel_x * local.accel_x +
                                   local.accel_y * local.accel_y +
                                   local.accel_z * local.accel_z);
        local.timestamp_ms = k_uptime_get_32();
        local.valid        = true;

        LOG_DBG("LIS3DH | X=%.3fg  Y=%.3fg  Z=%.3fg  |M|=%.3fg",
                (double)local.accel_x,
                (double)local.accel_y,
                (double)local.accel_z,
                (double)local.magnitude);

        k_mutex_lock(&g_data_mutex, K_FOREVER);
        g_lis3dh = local;
        k_mutex_unlock(&g_data_mutex);

        k_sleep(K_MSEC(LIS3DH_PERIOD_MS));
    }
}
