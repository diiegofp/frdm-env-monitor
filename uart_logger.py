#!/usr/bin/env python3
"""
uart_logger.py
Script de logging para FRDM-MCXN947 Environmental & Motion Monitor.
Lee el puerto serie, parsea los bloques de reporte y los guarda en CSV.

Uso:
    python uart_logger.py --port /dev/ttyACM0 --baud 115200 --out datos.csv

Requiere:
    pip install pyserial
"""

import argparse
import csv
import re
import sys
import time
from datetime import datetime
import serial

# ── Patrones de parseo ────────────────────────────────────────────────────────
PAT_TEMP     = re.compile(r"Temperatura\s*:\s*([\d.+-]+)")
PAT_HUM      = re.compile(r"Humedad\s*:\s*([\d.+-]+)")
PAT_PRESS    = re.compile(r"Presi.n\s*:\s*([\d.+-]+)")
PAT_IAQ      = re.compile(r"IAQ\s*:\s*([\d.+-]+)")
PAT_AX       = re.compile(r"Accel X\s*:\s*([+-][\d.]+)")
PAT_AY       = re.compile(r"Accel Y\s*:\s*([+-][\d.]+)")
PAT_AZ       = re.compile(r"Accel Z\s*:\s*([+-][\d.]+)")
PAT_MAG      = re.compile(r"Magnitud\s*:\s*([\d.]+)")
PAT_TREF     = re.compile(r"T_ref .I3C.\s*:\s*([\d.+-]+)")
PAT_ALERTAS  = re.compile(r"\[ALERTAS\]\s+(.+)")
PAT_MODO     = re.compile(r"\[MODO\]\s+(\w+)")
PAT_BLOCK_END= re.compile(r"╚")

CSV_HEADER = [
    "timestamp_pc", "temp_c", "humidity_pct", "pressure_hpa", "iaq",
    "accel_x_g", "accel_y_g", "accel_z_g", "mag_g",
    "t_ref_c", "alertas", "modo"
]

def parse_block(lines: list[str]) -> dict | None:
    text = "\n".join(lines)
    row = {}

    def extract(pat, key, cast=float):
        m = pat.search(text)
        if m:
            try:
                row[key] = cast(m.group(1))
            except ValueError:
                row[key] = None

    extract(PAT_TEMP,  "temp_c")
    extract(PAT_HUM,   "humidity_pct")
    extract(PAT_PRESS, "pressure_hpa")
    extract(PAT_IAQ,   "iaq")
    extract(PAT_AX,    "accel_x_g")
    extract(PAT_AY,    "accel_y_g")
    extract(PAT_AZ,    "accel_z_g")
    extract(PAT_MAG,   "mag_g")
    extract(PAT_TREF,  "t_ref_c")

    m_alr = PAT_ALERTAS.search(text)
    row["alertas"] = m_alr.group(1).strip() if m_alr else ""

    m_modo = PAT_MODO.search(text)
    row["modo"] = m_modo.group(1).strip() if m_modo else ""

    row["timestamp_pc"] = datetime.now().isoformat(timespec="milliseconds")

    if row.get("temp_c") is None:
        return None  # bloque incompleto
    return row


def run(port: str, baud: int, out: str):
    print(f"Conectando a {port} @ {baud} bps …")
    try:
        ser = serial.Serial(port, baud, timeout=2)
    except serial.SerialException as e:
        print(f"Error al abrir puerto: {e}")
        sys.exit(1)

    print(f"Guardando en: {out}")
    block_lines: list[str] = []
    in_block = False

    with open(out, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_HEADER)
        writer.writeheader()
        f.flush()

        try:
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                try:
                    line = raw.decode("utf-8", errors="replace").rstrip()
                except Exception:
                    continue

                if "╔" in line:
                    in_block = True
                    block_lines = [line]
                elif in_block:
                    block_lines.append(line)
                    if PAT_BLOCK_END.search(line):
                        row = parse_block(block_lines)
                        if row:
                            writer.writerow(row)
                            f.flush()
                            print(f"[{row['timestamp_pc']}] "
                                  f"T={row.get('temp_c'):.2f}°C  "
                                  f"HR={row.get('humidity_pct'):.1f}%  "
                                  f"IAQ={row.get('iaq'):.0f}  "
                                  f"Alertas={row.get('alertas') or 'OK'}")
                        in_block = False
                        block_lines = []
        except KeyboardInterrupt:
            print("\nLogging terminado.")
        finally:
            ser.close()


def main():
    ap = argparse.ArgumentParser(description="Logger UART para FRDM-MCXN947")
    ap.add_argument("--port", default="/dev/ttyACM0",
                    help="Puerto serie (default: /dev/ttyACM0)")
    ap.add_argument("--baud", type=int, default=115200,
                    help="Baud rate (default: 115200)")
    ap.add_argument("--out", default=f"monitor_{int(time.time())}.csv",
                    help="Archivo CSV de salida")
    args = ap.parse_args()
    run(args.port, args.baud, args.out)


if __name__ == "__main__":
    main()
