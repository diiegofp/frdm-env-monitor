/*
 * main.c
 * Punto de entrada. Inicializa periféricos y arranca las 6 tareas.
 * El sistema continúa aunque algún sensor no esté disponible.
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

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

extern void task_bme680_entry(void *p1, void *p2, void *p3);
extern void task_lis3dh_entry(void *p1, void *p2, void *p3);
extern void task_p3t1755_entry(void *p1, void *p2, void *p3);
extern void task_reporter_entry(void *p1, void *p2, void *p3);
extern void task_alert_entry(void *p1, void *p2, void *p3);
extern void task_touch_entry(void *p1, void *p2, void *p3);

K_THREAD_STACK_DEFINE(stack_bme680,   STACK_BME680);
K_THREAD_STACK_DEFINE(stack_lis3dh,   STACK_LIS3DH);
K_THREAD_STACK_DEFINE(stack_p3t1755,  STACK_P3T1755);
K_THREAD_STACK_DEFINE(stack_reporter, STACK_REPORTER);
K_THREAD_STACK_DEFINE(stack_alert,    STACK_ALERT);
K_THREAD_STACK_DEFINE(stack_touch,    STACK_TOUCH);

static struct k_thread tcb_bme680;
static struct k_thread tcb_lis3dh;
static struct k_thread tcb_p3t1755;
static struct k_thread tcb_reporter;
static struct k_thread tcb_alert;
static struct k_thread tcb_touch;

#define NODE_BME680   DT_NODELABEL(bme680)
#define NODE_LIS3DH   DT_NODELABEL(lis3dh)
#define NODE_P3T1755  DT_NODELABEL(p3t1755)

static bool device_check(const struct device *dev, const char *name)
{
    if (!device_is_ready(dev)) {
        LOG_WRN("No disponible: %s", name);
        return false;
    }
    LOG_INF("OK: %s", name);
    return true;
}

int main(void)
{
    LOG_INF("=================================================");
    LOG_INF(" FRDM-MCXN947  Environmental & Motion Monitor  ");
    LOG_INF(" Zephyr RTOS  |  Grupo Intelecto                ");
    LOG_INF("=================================================");

    const struct device *bme680  = DEVICE_DT_GET(NODE_BME680);
    const struct device *lis3dh  = DEVICE_DT_GET(NODE_LIS3DH);
    const struct device *p3t1755 = DEVICE_DT_GET(NODE_P3T1755);

    bool bme_ok = device_check(bme680,  "BME680 (I2C)");
    bool lis_ok = device_check(lis3dh,  "LIS3DH (I2C)");
    bool p3t_ok = device_check(p3t1755, "P3T1755DP (I3C)");

    if (!bme_ok && !lis_ok && !p3t_ok) {
        LOG_ERR("Sin sensores disponibles — sistema detenido.");
        return -1;
    }

    LOG_INF("Iniciando tareas...");

    /* Tarea BME680 — solo si el sensor está disponible */
    if (bme_ok) {
        k_thread_create(&tcb_bme680, stack_bme680,
                        K_THREAD_STACK_SIZEOF(stack_bme680),
                        task_bme680_entry,
                        (void *)bme680, NULL, NULL,
                        PRIO_BME680, 0, K_NO_WAIT);
        k_thread_name_set(&tcb_bme680, "bme680");
    }

    /* Tarea LIS3DH — solo si el sensor está disponible */
    if (lis_ok) {
        k_thread_create(&tcb_lis3dh, stack_lis3dh,
                        K_THREAD_STACK_SIZEOF(stack_lis3dh),
                        task_lis3dh_entry,
                        (void *)lis3dh, NULL, NULL,
                        PRIO_LIS3DH, 0, K_NO_WAIT);
        k_thread_name_set(&tcb_lis3dh, "lis3dh");
    }

    /* Tarea P3T1755 — solo si el sensor está disponible */
    if (p3t_ok) {
        k_thread_create(&tcb_p3t1755, stack_p3t1755,
                        K_THREAD_STACK_SIZEOF(stack_p3t1755),
                        task_p3t1755_entry,
                        (void *)p3t1755, NULL, NULL,
                        PRIO_P3T1755, 0, K_NO_WAIT);
        k_thread_name_set(&tcb_p3t1755, "p3t1755");
    }

    /* Estas tareas siempre corren */
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

    LOG_INF("Tareas iniciadas. Sistema corriendo.");
    return 0;
}
