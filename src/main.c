/*
 * main.c
 * Punto de entrada del sistema. Inicializa periféricos, verifica que los
 * sensores estén presentes y arranca las seis tareas de Zephyr.
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 *
 * Arquitectura de tareas:
 *   1. task_bme680   — Adquisición BME680  (I2C,  2 s)
 *   2. task_lis3dh   — Adquisición LIS3DH  (SPI, 100 ms)
 *   3. task_p3t1755  — Adquisición P3T1755 (I3C,  1 s)
 *   4. task_reporter — Reporte UART         (2 s)
 *   5. task_alert    — Evaluación de alertas + control RGB LED (500 ms)
 *   6. task_touch    — Lectura touch slider, alterna modos de display (200 ms)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* ── Declaraciones de funciones de tarea (definidas en sus propios .c) ────── */
extern void task_bme680_entry(void *p1, void *p2, void *p3);
extern void task_lis3dh_entry(void *p1, void *p2, void *p3);
extern void task_p3t1755_entry(void *p1, void *p2, void *p3);
extern void task_reporter_entry(void *p1, void *p2, void *p3);
extern void task_alert_entry(void *p1, void *p2, void *p3);
extern void task_touch_entry(void *p1, void *p2, void *p3);

/* ── Definición de stacks ─────────────────────────────────────────────────── */
K_THREAD_STACK_DEFINE(stack_bme680,  STACK_BME680);
K_THREAD_STACK_DEFINE(stack_lis3dh,  STACK_LIS3DH);
K_THREAD_STACK_DEFINE(stack_p3t1755, STACK_P3T1755);
K_THREAD_STACK_DEFINE(stack_reporter,STACK_REPORTER);
K_THREAD_STACK_DEFINE(stack_alert,   STACK_ALERT);
K_THREAD_STACK_DEFINE(stack_touch,   STACK_TOUCH);

/* ── TCBs (Thread Control Blocks) ────────────────────────────────────────── */
static struct k_thread tcb_bme680;
static struct k_thread tcb_lis3dh;
static struct k_thread tcb_p3t1755;
static struct k_thread tcb_reporter;
static struct k_thread tcb_alert;
static struct k_thread tcb_touch;

/* ── Nodos del devicetree ────────────────────────────────────────────────── */
#define NODE_BME680   DT_ALIAS(env_sensor)
#define NODE_LIS3DH   DT_ALIAS(accel)
#define NODE_P3T1755  DT_ALIAS(ref_temp)

/* ────────────────────────────────────────────────────────────────────────── */
/*  Función auxiliar: verifica que un device esté listo                       */
/* ────────────────────────────────────────────────────────────────────────── */
static bool device_check(const struct device *dev, const char *name)
{
    if (!device_is_ready(dev)) {
        LOG_ERR("Dispositivo no listo: %s", name);
        return false;
    }
    LOG_INF("OK: %s", name);
    return true;
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  main                                                                       */
/* ────────────────────────────────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("=================================================");
    LOG_INF(" FRDM-MCXN947  Environmental & Motion Monitor  ");
    LOG_INF(" Zephyr RTOS  |  Grupo Intelecto                ");
    LOG_INF("=================================================");

    /* Verificar sensores */
    const struct device *bme680  = DEVICE_DT_GET(NODE_BME680);
    const struct device *lis3dh  = DEVICE_DT_GET(NODE_LIS3DH);
    const struct device *p3t1755 = DEVICE_DT_GET(NODE_P3T1755);

    bool hw_ok = true;
    hw_ok &= device_check(bme680,  "BME680 (I2C)");
    hw_ok &= device_check(lis3dh,  "LIS3DH (SPI)");
    hw_ok &= device_check(p3t1755, "P3T1755DP (I3C)");

    if (!hw_ok) {
        LOG_ERR("Error de hardware — sistema detenido.");
        return -1;
    }

    /* ── Crear las 6 tareas ─────────────────────────────────────────────── */

    k_thread_create(&tcb_bme680, stack_bme680,
                    K_THREAD_STACK_SIZEOF(stack_bme680),
                    task_bme680_entry,
                    (void *)bme680, NULL, NULL,
                    PRIO_BME680, 0, K_NO_WAIT);
    k_thread_name_set(&tcb_bme680, "bme680");

    k_thread_create(&tcb_lis3dh, stack_lis3dh,
                    K_THREAD_STACK_SIZEOF(stack_lis3dh),
                    task_lis3dh_entry,
                    (void *)lis3dh, NULL, NULL,
                    PRIO_LIS3DH, 0, K_NO_WAIT);
    k_thread_name_set(&tcb_lis3dh, "lis3dh");

    k_thread_create(&tcb_p3t1755, stack_p3t1755,
                    K_THREAD_STACK_SIZEOF(stack_p3t1755),
                    task_p3t1755_entry,
                    (void *)p3t1755, NULL, NULL,
                    PRIO_P3T1755, 0, K_NO_WAIT);
    k_thread_name_set(&tcb_p3t1755, "p3t1755");

    k_thread_create(&tcb_reporter, stack_reporter,
                    K_THREAD_STACK_SIZEOF(stack_reporter),
                    task_reporter_entry,
                    NULL, NULL, NULL,
                    PRIO_REPORTER, 0, K_NO_WAIT);
    k_thread_name_set(&tcb_reporter, "reporter");

    k_thread_create(&tcb_alert, stack_alert,
                    K_THREAD_STACK_SIZEOF(stack_alert),
                    task_alert_entry,
                    NULL, NULL, NULL,
                    PRIO_ALERT, 0, K_NO_WAIT);
    k_thread_name_set(&tcb_alert, "alert");

    k_thread_create(&tcb_touch, stack_touch,
                    K_THREAD_STACK_SIZEOF(stack_touch),
                    task_touch_entry,
                    NULL, NULL, NULL,
                    PRIO_TOUCH, 0, K_NO_WAIT);
    k_thread_name_set(&tcb_touch, "touch");

    LOG_INF("6 tareas iniciadas correctamente.");

    /* main retorna — el scheduler de Zephyr toma el control */
    return 0;
}
