#!/usr/bin/env python3
"""
uart_logger.py
Logger UART para FRDM-MCXN947 Environmental & Motion Monitor.
Lee el puerto serie, parsea los bloques de reporte y los guarda en:
  1. CSV local (siempre)
  2. InfluxDB v3 (si está disponible)

Uso:
    python uart_logger.py --port COM3 --baud 115200 --out datos.csv

Requiere:
    pip install pyserial influxdb3-python
"""

import argparse
import csv
import re
import sys
import time
from datetime import datetime, timezone
import serial

# ── Importar cliente InfluxDB v3 (opcional) ───────────────────────────────────
try:
    from influxdb_client_3 import InfluxDBClient3, Point
    INFLUX_AVAILABLE = True
except ImportError:
    INFLUX_AVAILABLE = False
    print("[WARN] influxdb3-python no instalado. Solo se guardará CSV.")
    print("       Instala con: pip install influxdb3-python")

# ── Configuración de InfluxDB ─────────────────────────────────────────────────
INFLUX_HOST  = "http://localhost:8181"
INFLUX_TOKEN = "apiv3_y71MYLzo1smRlOPenxFyl3AR416g5QtTWRqDvXW1veWaPaSyyOqg8sBIclxxV7AERsmBI3EEUT-q82Rf364eyw"
INFLUX_DB    = "sensores"

# ── Patrones de parseo ────────────────────────────────────────────────────────
PAT_TEMP    = re.compile(r"Temperatura\s*:\s*([\d.+-]+)")
PAT_HUM     = re.compile(r"Humedad\s*:\s*([\d.+-]+)")
PAT_PRESS   = re.compile(r"Presi.n\s*:\s*([\d.+-]+)")
PAT_IAQ     = re.compile(r"IAQ\s*:\s*([\d.+-]+)")
PAT_AX      = re.compile(r"Accel X\s*:\s*([+-][\d.]+)")
PAT_AY      = re.compile(r"Accel Y\s*:\s*([+-][\d.]+)")
PAT_AZ      = re.compile(r"Accel Z\s*:\s*([+-][\d.]+)")
PAT_MAG     = re.compile(r"Magnitud\s*:\s*([\d.]+)")
PAT_TREF    = re.compile(r"T_ref .I3C.\s*:\s*([\d.+-]+)")
PAT_ALERTAS = re.compile(r"\[ALERTAS\]\s+(.+)")
PAT_MODO    = re.compile(r"\[MODO\]\s+(\w+)")
PAT_END     = re.compile(r"╚")

CSV_HEADER = [
    "timestamp_pc", "temp_c", "humidity_pct", "pressure_hpa", "iaq",
    "accel_x_g", "accel_y_g", "accel_z_g", "mag_g",
    "t_ref_c", "alertas", "modo"
]

def parse_block(lines):
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

    m = PAT_ALERTAS.search(text)
    row["alertas"] = m.group(1).strip() if m else ""
    m = PAT_MODO.search(text)
    row["modo"] = m.group(1).strip() if m else ""
    row["timestamp_pc"] = datetime.now(timezone.utc).isoformat()

    if row.get("temp_c") is None:
        return None
    return row

def send_to_influx(client, row):
    """Manda una fila de datos a InfluxDB como dos measurements."""
    try:
        # Measurement: datos ambientales
        p_env = (Point("ambiente")
                 .field("temperatura_c",   row.get("temp_c"))
                 .field("humedad_pct",      row.get("humidity_pct"))
                 .field("presion_hpa",      row.get("pressure_hpa"))
                 .field("iaq",              row.get("iaq"))
                 .field("t_ref_c",          row.get("t_ref_c")))

        # Measurement: aceleración
        p_mot = (Point("movimiento")
                 .field("accel_x_g",  row.get("accel_x_g"))
                 .field("accel_y_g",  row.get("accel_y_g"))
                 .field("accel_z_g",  row.get("accel_z_g"))
                 .field("magnitud_g", row.get("mag_g")))

        client.write([p_env, p_mot])
    except Exception as e:
        print(f"[WARN] Error escribiendo a InfluxDB: {e}")

def run(port, baud, out):
    print(f"Conectando a {port} @ {baud} bps ...")
    try:
        ser = serial.Serial(port, baud, timeout=2)
    except serial.SerialException as e:
        print(f"Error al abrir puerto: {e}")
        sys.exit(1)

    # Inicializar cliente InfluxDB
    influx_client = None
    if INFLUX_AVAILABLE:
        try:
            influx_client = InfluxDBClient3(
                host=INFLUX_HOST,
                token=INFLUX_TOKEN,
                database=INFLUX_DB
            )
            print(f"InfluxDB conectado → {INFLUX_HOST}/{INFLUX_DB}")
        except Exception as e:
            print(f"[WARN] No se pudo conectar a InfluxDB: {e}")

    print(f"Guardando CSV en: {out}")
    block_lines = []
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
                    if PAT_END.search(line):
                        row = parse_block(block_lines)
                        if row:
                            # Guardar CSV
                            writer.writerow(row)
                            f.flush()
                            # Mandar a InfluxDB
                            if influx_client:
                                send_to_influx(influx_client, row)
                            # Imprimir resumen
                            print(f"[{row['timestamp_pc']}] "
                                  f"T={row.get('temp_c', '?'):.2f}°C  "
                                  f"HR={row.get('humidity_pct', '?'):.1f}%  "
                                  f"P={row.get('pressure_hpa', '?'):.1f}hPa  "
                                  f"IAQ={row.get('iaq', '?'):.0f}  "
                                  f"|a|={row.get('mag_g', '?'):.3f}g  "
                                  f"Alertas={row.get('alertas') or 'OK'}")
                        in_block = False
                        block_lines = []
        except KeyboardInterrupt:
            print("\nLogging terminado.")
        finally:
            ser.close()
            if influx_client:
                influx_client.close()

def main():
    ap = argparse.ArgumentParser(description="Logger UART para FRDM-MCXN947")
    ap.add_argument("--port", default="COM3",
                    help="Puerto serie (default: COM3)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out",  default=f"monitor_{int(time.time())}.csv")
    args = ap.parse_args()
    run(args.port, args.baud, args.out)

if __name__ == "__main__":
    main()
