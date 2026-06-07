cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(frdm_env_monitor VERSION 1.0.0)

# ── Fuentes principales ────────────────────────────────────────────────────────
target_sources(app PRIVATE
    src/main.c
    src/task_bme680.c
    src/task_lis3dh.c
    src/task_p3t1755.c
    src/task_reporter.c
    src/task_alert.c
    src/task_touch.c
    src/sensor_data.c
)

# ── Includes ──────────────────────────────────────────────────────────────────
target_include_directories(app PRIVATE include)
