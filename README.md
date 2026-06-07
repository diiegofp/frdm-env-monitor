# FRDM-MCXN947 — Environmental & Motion Monitor Station
### Proyecto de RTOS | Universidad Galileo | Grupo Intelecto

---

## Descripción

Sistema embebido de monitoreo ambiental e inercial en tiempo real,
implementado sobre la tarjeta **NXP FRDM-MCXN947** con **Zephyr RTOS**.
Adquiere datos de temperatura, humedad, presión barométrica, calidad del
aire y aceleración triaxial; evalúa alertas en tiempo real; reporta por
UART y controla un LED RGB de estado.

---

## Hardware

| Componente        | Interfaz | Función                                      |
|-------------------|----------|----------------------------------------------|
| **BME680**        | I2C      | Temperatura, humedad, presión, gas (IAQ)     |
| **LIS3DH**        | SPI      | Acelerómetro 3 ejes — movimiento y vibración |
| **P3T1755DP**     | I3C      | Temperatura de referencia interna (embebido) |
| **RGB LED**       | GPIO     | Indicación visual de estado y alertas        |
| **Touch slider**  | ADC      | Alternancia entre modos de visualización     |

---

## Arquitectura de software

```
┌────────────────────────────────────────────────────────────┐
│                     Zephyr Scheduler                       │
├──────────┬──────────┬──────────┬───────────┬──────┬───────┤
│ bme680   │ lis3dh   │ p3t1755  │ reporter  │alert │ touch │
│ prio=5   │ prio=4   │ prio=6   │ prio=7    │prio=3│prio=8 │
│ 2000 ms  │  100 ms  │ 1000 ms  │ 2000 ms   │500ms │200 ms │
└────┬─────┴────┬─────┴────┬─────┴─────┬─────┴──┬───┴───┬──┘
     │          │          │           │         │       │
     └──────────┴──────────┘           │         │       │
          k_mutex (g_data_mutex)       │         │       │
          g_bme680 / g_lis3dh /        │         │       │
          g_p3t1755 / g_alerts /       │         │       │
          g_display_mode               │         │       │
                                       │         │       │
                              k_msgq   │  k_sem  │  ADC  │
                           (reporter)  │ (alert) │(touch)│
```

### Las 6 tareas

| Tarea          | Prioridad | Período  | Descripción                                   |
|----------------|-----------|----------|-----------------------------------------------|
| `bme680`       | 5         | 2 000 ms | Adquisición BME680 por I2C                    |
| `lis3dh`       | 4         | 100 ms   | Adquisición LIS3DH por SPI a 10 Hz            |
| `p3t1755`      | 6         | 1 000 ms | Adquisición P3T1755DP por I3C                 |
| `reporter`     | 7         | 2 000 ms | Reporte formateado por UART (115200 8N1)      |
| `alert`        | 3         | 500 ms   | Evaluación de alertas + control RGB LED       |
| `touch`        | 8         | 200 ms   | Lectura touch slider, cambia modo de display  |

### Sincronización

- **`g_data_mutex`** — protege las variables globales de sensores y estado.
- **`g_data_ready_sem`** — la tarea BME680 señaliza a `alert` cuando hay datos nuevos.
- **`g_report_queue`** — cola de mensajes para desacoplar el reporter (no bloqueante).

---

## Lógica del LED RGB

| Color                 | Condición                                    |
|-----------------------|----------------------------------------------|
| 🟢 Verde sólido       | Sistema nominal, sin alertas                 |
| 🟡 Amarillo (R+G)     | IAQ malo o humedad alta                      |
| 🔴 Rojo sólido        | Temperatura fuera de rango (< 5°C o > 40°C) |
| 🔵 Azul parpadeante   | Movimiento / vibración detectado             |
| 🟣 Magenta (R+B)      | Múltiples alertas simultáneas                |

---

## Umbrales de alerta

| Parámetro      | Umbral         |
|----------------|----------------|
| Temperatura    | < 5°C o > 40°C |
| Humedad        | > 85 %RH       |
| IAQ            | > 200          |
| Aceleración    | > 3.0 g (magnitud) |

---

## Modos de display (touch slider)

| Modo | Descripción              |
|------|--------------------------|
| 0    | Temperatura (BME680)     |
| 1    | Humedad relativa         |
| 2    | Presión barométrica      |
| 3    | Aceleración (LIS3DH)     |
| 4    | Calidad del aire (IAQ)   |

Deslizar hacia la **derecha** avanza el modo.
Deslizar hacia la **izquierda** retrocede.

---

## Estructura de archivos

```
frdm_env_monitor/
├── CMakeLists.txt
├── prj.conf
├── boards/
│   └── frdm_mcxn947.overlay        # Devicetree overlay
├── include/
│   └── sensor_data.h               # Tipos, constantes y globales
└── src/
    ├── main.c                      # Arranque y creación de tareas
    ├── sensor_data.c               # Definición de globales y objetos RTOS
    ├── task_bme680.c               # Tarea adquisición BME680 (I2C)
    ├── task_lis3dh.c               # Tarea adquisición LIS3DH (SPI)
    ├── task_p3t1755.c              # Tarea adquisición P3T1755DP (I3C)
    ├── task_reporter.c             # Tarea reporte UART
    ├── task_alert.c                # Tarea alertas + LED RGB
    └── task_touch.c                # Tarea touch slider
```

---

## Compilación

### Prerrequisitos

- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/)
- Python ≥ 3.10 con `west`
- Toolchain ARM: `arm-zephyr-eabi`

### Comandos

```bash
# Clonar Zephyr y configurar entorno
west init ~/zephyrproject
cd ~/zephyrproject
west update

# Compilar para FRDM-MCXN947
cd <ruta_del_proyecto>
west build -b frdm_mcxn947 .

# Flashear
west flash

# Monitor UART
west espressif monitor        # o cualquier terminal serie: minicom, Putty
# 115200 bps, 8N1
```

---

## Salida UART esperada

```
╔══════════════════════════════════════════════════╗
║   ENV & MOTION MONITOR  |  t=4002 ms
╠══════════════════════════════════════════════════╣
║ [BME680]  Temperatura :  23.45 °C
║           Humedad     :  58.2 %RH
║           Presión     :  1012.8 hPa
║           IAQ         :    87  (Buena)
║
║ [LIS3DH]  Accel X     : +0.021 g
║           Accel Y     : -0.015 g
║           Accel Z     : +0.998 g
║           Magnitud    :  0.999 g
║
║ [P3T1755] T_ref (I3C) :  23.875 °C
║
║ [ALERTAS] Ninguna — sistema nominal
║ [MODO]    TEMPERATURA
╚══════════════════════════════════════════════════╝
```

---

## Notas de implementación

### BME680 — IAQ simplificado
El driver de Zephyr para el BME680 no incluye el algoritmo BSEC de Bosch
(propietario). Se usa una conversión logarítmica de la resistencia de gas
al índice IAQ. Para precisión de producción, integrar la librería BSEC vía
módulo externo.

### P3T1755DP — Driver I3C
El P3T1755DP usa el protocolo I3C. Zephyr incluye soporte básico de I3C
desde la versión 3.4. Verificar que la versión de Zephyr instalada incluya
el driver `nxp,p3t1755` o adaptar a acceso I3C directo via `i3c_transfer()`.

### Touch slider — ADC capacitivo
La FRDM-MCXN947 expone el touch slider mediante el periférico TSI. La
lectura via ADC es una abstracción compatible. Para mayor precisión usar
el driver TSI nativo si está disponible en el BSP.
