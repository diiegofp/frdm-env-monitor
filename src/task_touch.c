/*
 * task_touch.c
 * Tarea de lectura del touch slider (TSI0) para alternar modos de display.
 *
 * La FRDM-MCXN947 tiene un touch slider conectado al periférico TSI0
 * (Touch Sensing Input). En Zephyr se accede mediante el subsistema
 * de input events — el driver tsi publica eventos INPUT_BTN_3 cuando
 * detecta un toque (configurado en el DTS base con channel-mask=0x08).
 *
 * Estrategia simplificada:
 *   - Usamos k_poll sobre un message queue de input para detectar toques.
 *   - Cada toque avanza el modo de display en orden circular.
 *   - Debounce de TOUCH_DEBOUNCE_MS entre cambios.
 *
 * Proyecto : frdm_env_monitor
 * Board    : NXP FRDM-MCXN947
 * RTOS     : Zephyr
 */

#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include "sensor_data.h"

LOG_MODULE_REGISTER(task_touch, LOG_LEVEL_INF);

#define TOUCH_DEBOUNCE_MS   400U

/* Cola de eventos de input — el driver TSI publica aquí */
static struct input_event touch_evt;
K_MSGQ_DEFINE(touch_msgq, sizeof(struct input_event), 4, 4);

/* Callback registrado con el subsistema de input de Zephyr */
static void touch_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);
    /* Solo nos interesan eventos de botón (toque detectado) */
    if (evt->type == INPUT_EV_KEY) {
        k_msgq_put(&touch_msgq, evt, K_NO_WAIT);
    }
}
INPUT_CALLBACK_DEFINE(NULL, touch_cb, NULL);

static void mode_advance(void)
{
    k_mutex_lock(&g_data_mutex, K_FOREVER);
    g_display_mode = (display_mode_t)((g_display_mode + 1) % DISPLAY_MODE_COUNT);
    int m = g_display_mode;
    k_mutex_unlock(&g_data_mutex);
    LOG_INF("Modo display → %d", m);
}

void task_touch_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("Tarea Touch iniciada (TSI0).");

    uint32_t last_change_ms = 0;

    while (1) {
        /* Esperar evento de toque con timeout */
        int rc = k_msgq_get(&touch_msgq, &touch_evt, K_MSEC(TOUCH_PERIOD_MS));
        if (rc == 0 && touch_evt.value == 1) {
            /* Toque detectado — aplicar debounce */
            uint32_t now = k_uptime_get_32();
            if ((now - last_change_ms) >= TOUCH_DEBOUNCE_MS) {
                mode_advance();
                last_change_ms = now;
            }
        }
    }
}
