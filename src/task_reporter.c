/*
 * task_reporter.c
 * Tarea de reporte de datos por UART.
 * Cada REPORTER_PERIOD_MS publica un bloque de texto con todos los valores
 * de sensor, el estado de alertas y el modo de display activo.
 * El formato es legible para un terminal (115200, 8N1) y también parseable
 * por un script Python de logging.
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(task_reporter, LOG_LEVEL_INF);

/* Nombres de modos de display para el reporte */
static const char *const mode_names[DISPLAY_MODE_COUNT] = {
    [DISPLAY_MODE_TEMP]     = "TEMPERATURA",
    [DISPLAY_MODE_HUMIDITY] = "HUMEDAD",
    [DISPLAY_MODE_PRESSURE] = "PRESION",
    [DISPLAY_MODE_ACCEL]    = "ACELERACION",
    [DISPLAY_MODE_IAQ]      = "CALIDAD_AIRE",
};

/* Calificación IAQ */
static const char *iaq_label(float iaq)
{
    if (iaq < 50)  return "Excelente";
    if (iaq < 100) return "Buena";
    if (iaq < 150) return "Leve";
    if (iaq < 200) return "Moderada";
    if (iaq < 300) return "Mala";
    return "Peligrosa";
}

/* ── Entrada de tarea ─────────────────────────────────────────────────────── */
void task_reporter_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    bme680_data_t   env;
    lis3dh_data_t   mot;
    p3t1755_data_t  ref;
    alert_status_t  alr;
    display_mode_t  mode;

    LOG_INF("Tarea Reporter iniciada. Período: %u ms", REPORTER_PERIOD_MS);

    while (1) {
        k_sleep(K_MSEC(REPORTER_PERIOD_MS));

        /* Captura atómica de todos los datos */
        k_mutex_lock(&g_data_mutex, K_FOREVER);
        env  = g_bme680;
        mot  = g_lis3dh;
        ref  = g_p3t1755;
        alr  = g_alerts;
        mode = g_display_mode;
        k_mutex_unlock(&g_data_mutex);

        uint32_t ts = k_uptime_get_32();

        /* ── Bloque de reporte ─────────────────────────────────────────── */
        printk("\n");
        printk("╔══════════════════════════════════════════════════╗\n");
        printk("║   ENV & MOTION MONITOR  |  t=%u ms         \n", ts);
        printk("╠══════════════════════════════════════════════════╣\n");

        /* Sensor BME680 */
        if (env.valid) {
            printk("║ [BME680]  Temperatura : %6.2f °C\n",
                   (double)env.temperature);
            printk("║           Humedad     : %6.1f %%RH\n",
                   (double)env.humidity);
            printk("║           Presión     : %7.1f hPa\n",
                   (double)env.pressure);
            printk("║           IAQ         : %5.0f  (%s)\n",
                   (double)env.iaq, iaq_label(env.iaq));
        } else {
            printk("║ [BME680]  Sin datos válidos\n");
        }

        printk("║\n");

        /* Sensor LIS3DH */
        if (mot.valid) {
            printk("║ [LIS3DH]  Accel X     : %+6.3f g\n",
                   (double)mot.accel_x);
            printk("║           Accel Y     : %+6.3f g\n",
                   (double)mot.accel_y);
            printk("║           Accel Z     : %+6.3f g\n",
                   (double)mot.accel_z);
            printk("║           Magnitud    : %6.3f g\n",
                   (double)mot.magnitude);
        } else {
            printk("║ [LIS3DH]  Sin datos válidos\n");
        }

        printk("║\n");

        /* Sensor P3T1755DP */
        if (ref.valid) {
            printk("║ [P3T1755] T_ref (I3C) : %6.3f °C\n",
                   (double)ref.temperature);
        } else {
            printk("║ [P3T1755] Sin datos válidos\n");
        }

        printk("║\n");

        /* Alertas activas */
        printk("║ [ALERTAS] ");
        if (!alr.any_active) {
            printk("Ninguna — sistema nominal\n");
        } else {
            if (alr.temp_high)       printk("TEMP_ALTA ");
            if (alr.temp_low)        printk("TEMP_BAJA ");
            if (alr.humidity_high)   printk("HUMEDAD_ALTA ");
            if (alr.iaq_bad)         printk("AIRE_MALO ");
            if (alr.motion_detected) printk("MOVIMIENTO ");
            printk("\n");
        }

        /* Modo de display activo */
        const char *mname = (mode < DISPLAY_MODE_COUNT) ? mode_names[mode] : "?";
        printk("║ [MODO]    %-20s\n", mname);

        printk("╚══════════════════════════════════════════════════╝\n");
    }
}
