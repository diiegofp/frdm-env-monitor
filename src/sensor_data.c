/*
 * sensor_data.c
 * Definición de variables globales, mutex, colas y semáforos compartidos
 * entre todas las tareas del sistema.
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 */

#include "sensor_data.h"
#include <zephyr/kernel.h>
#include <string.h>

/* ── Mutex de acceso a datos compartidos ─────────────────────────────────── */
K_MUTEX_DEFINE(g_data_mutex);

/* ── Estado global de cada sensor ────────────────────────────────────────── */
bme680_data_t  g_bme680   = { .valid = false };
lis3dh_data_t  g_lis3dh   = { .valid = false };
p3t1755_data_t g_p3t1755  = { .valid = false };

/* ── Estado global de alertas y modo de display ──────────────────────────── */
alert_status_t g_alerts       = { 0 };
display_mode_t g_display_mode = DISPLAY_MODE_TEMP;

/* ── Cola de mensajes hacia el reporter ─────────────────────────────────── */
K_MSGQ_DEFINE(g_report_queue, sizeof(report_payload_t), QUEUE_DEPTH, 4);

/* ── Semáforo: señaliza que hay datos nuevos listos para evaluar alertas ─── */
K_SEM_DEFINE(g_data_ready_sem, 0, 1);
