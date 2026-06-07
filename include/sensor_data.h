/*
 * sensor_data.h
 * Tipos de datos compartidos entre tareas y colas de mensajes.
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 */

#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Constantes de configuración ─────────────────────────────────────────── */

/* Períodos de muestreo de cada tarea (ms) */
#define BME680_PERIOD_MS       2000U
#define LIS3DH_PERIOD_MS        100U   /* 10 Hz */
#define P3T1755_PERIOD_MS      1000U
#define REPORTER_PERIOD_MS     2000U
#define ALERT_PERIOD_MS         500U
#define TOUCH_PERIOD_MS         200U

/* Tamaños de stack por tarea */
#define STACK_BME680           1536U
#define STACK_LIS3DH           1024U
#define STACK_P3T1755          1024U
#define STACK_REPORTER         2048U
#define STACK_ALERT            1024U
#define STACK_TOUCH             768U

/* Prioridades (menor número = mayor prioridad en Zephyr) */
#define PRIO_BME680               5
#define PRIO_LIS3DH               4   /* mayor prioridad: 10 Hz */
#define PRIO_P3T1755              6
#define PRIO_REPORTER             7
#define PRIO_ALERT                3   /* mayor prioridad: seguridad */
#define PRIO_TOUCH                8

/* Profundidad de las colas de mensajes */
#define QUEUE_DEPTH               4

/* ── Umbrales de alerta ───────────────────────────────────────────────────── */
#define ALERT_TEMP_HIGH_C      40.0f   /* °C  */
#define ALERT_TEMP_LOW_C        5.0f   /* °C  */
#define ALERT_HUMIDITY_HIGH    85.0f   /* %RH */
#define ALERT_IAQ_BAD         200.0f   /* IAQ index */
#define ALERT_ACCEL_G          2.0f    /* g   */

/* ── Modos de visualización (touch slider) ───────────────────────────────── */
typedef enum {
    DISPLAY_MODE_TEMP = 0,   /* Muestra temperatura  */
    DISPLAY_MODE_HUMIDITY,   /* Muestra humedad      */
    DISPLAY_MODE_PRESSURE,   /* Muestra presión      */
    DISPLAY_MODE_ACCEL,      /* Muestra aceleración  */
    DISPLAY_MODE_IAQ,        /* Muestra calidad aire */
    DISPLAY_MODE_COUNT
} display_mode_t;

/* ── Datos del BME680 ─────────────────────────────────────────────────────── */
typedef struct {
    float    temperature;   /* °C        */
    float    humidity;      /* %RH       */
    float    pressure;      /* hPa       */
    float    iaq;           /* IAQ index */
    uint32_t timestamp_ms;
    bool     valid;
} bme680_data_t;

/* ── Datos del LIS3DH ─────────────────────────────────────────────────────── */
typedef struct {
    float    accel_x;       /* g */
    float    accel_y;       /* g */
    float    accel_z;       /* g */
    float    magnitude;     /* g */
    uint32_t timestamp_ms;
    bool     valid;
} lis3dh_data_t;

/* ── Datos del P3T1755DP ──────────────────────────────────────────────────── */
typedef struct {
    float    temperature;   /* °C */
    uint32_t timestamp_ms;
    bool     valid;
} p3t1755_data_t;

/* ── Estado de alertas ────────────────────────────────────────────────────── */
typedef struct {
    bool temp_high;
    bool temp_low;
    bool humidity_high;
    bool iaq_bad;
    bool motion_detected;
    bool any_active;
} alert_status_t;

/* ── Payload del reporter (mensaje en cola) ──────────────────────────────── */
typedef struct {
    bme680_data_t  env;
    lis3dh_data_t  motion;
    p3t1755_data_t ref_temp;
    alert_status_t alerts;
    display_mode_t mode;
} report_payload_t;

/* ── Variables globales compartidas (definidas en sensor_data.c) ─────────── */
extern struct k_mutex      g_data_mutex;
extern bme680_data_t       g_bme680;
extern lis3dh_data_t       g_lis3dh;
extern p3t1755_data_t      g_p3t1755;
extern alert_status_t      g_alerts;
extern display_mode_t      g_display_mode;

/* Cola hacia la tarea reporter */
extern struct k_msgq       g_report_queue;

/* Semáforo: notifica a alert task cuando hay nuevos datos */
extern struct k_sem        g_data_ready_sem;

#endif /* SENSOR_DATA_H */
