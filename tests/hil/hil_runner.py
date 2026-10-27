import sys
import os
import time
import subprocess
import argparse
from datetime import datetime, timezone
from typing import Optional, List, Dict, Any

try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

def log(msg: str):
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    print(f"[{ts}] {msg}")

def probe_serial_ports() -> List[Dict[str, Any]]:
    if not SERIAL_AVAILABLE:
        log("[WARN] pyserial library is not available in environment.")
        return []
    ports = serial.tools.list_ports.comports()
    results = []
    for p in ports:
        desc = p.description or ""
        hwid = p.hwid or ""
        is_bluetooth = "bluetooth" in desc.lower() or "bth" in hwid.lower()
        is_esp = any(k in desc.lower() or k in hwid.lower() for k in [
            "esp", "ch340", "ch343", "cp210", "ftdi", "usb-serial", "10c4:ea60", "303a:1001", "303a:1002"
        ])
        results.append({
            "device": p.device,
            "description": desc,
            "hwid": hwid,
            "is_bluetooth": is_bluetooth,
            "is_esp_candidate": is_esp and not is_bluetooth
        })
    return results

def find_esp32_serial_port() -> Optional[str]:
    probed = probe_serial_ports()
    for p in probed:
        if p["is_esp_candidate"]:
            return p["device"]
    return None

def compile_native_firmware(sim_exe: str) -> bool:
    log(f"Compiling high-fidelity native firmware test executable to {sim_exe}...")
    compile_cmd = [
        "gcc", "-Wall", "-Wextra", "-I", "firmware/include",
        "firmware/src/firmware_config.c",
        "firmware/src/dht11.c",
        "firmware/src/sensor_calibration.c",
        "firmware/src/sensor_manager.c",
        "firmware/src/safety_manager.c",
        "firmware/src/actuator_manager.c",
        "firmware/src/command_manager.c",
        "firmware/src/telemetry_client.c",
        "firmware/src/wifi_manager.c",
        "firmware/src/network_client.c",
        "firmware/src/edge_ai_model.c",
        "firmware/src/edge_ai.c",
        "firmware/src/ota_manager.c",
        "firmware/test/test_firmware_native.c",
        "-o", sim_exe
    ]
    res = subprocess.run(compile_cmd, capture_output=True, text=True)
    if res.returncode != 0:
        log(f"[ERROR] GCC compilation failed:\n{res.stderr}")
        return False
    log("Firmware compilation succeeded.")
    return True

def run_simulation() -> bool:
    log("--- Level B: High-Fidelity Firmware Simulation ---")
    sim_exe = os.path.join("firmware", "test", "test_firmware_native.exe")
    # Always rebuild to guarantee test reflects active source
    if not compile_native_firmware(sim_exe):
        return False

    log(f"Executing {sim_exe}...")
    proc = subprocess.run([sim_exe], capture_output=True, text=True)
    print(proc.stdout)
    if proc.returncode != 0:
        log(f"[ERROR] Simulator failed with return code {proc.returncode}")
        return False
    return True

def run_physical_test(port: str, timeout: float = 3.0) -> bool:
    log(f"--- Level C: Physical Hardware-In-The-Loop on {port} ---")
    log(f"Opening Serial connection to {port} at 115200 baud (Timeout: {timeout}s)...")
    ser = None
    try:
        ser = serial.Serial(port, 115200, timeout=timeout)
        time.sleep(0.5) # Allow serial line to settle
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        log("Sending PING / Heartbeat handshake...")
        ser.write(b"PING\n")
        response = ser.readline().decode("utf-8", errors="ignore").strip()

        if response:
            log(f"[SERIAL RECV]: {response}")
            log(f"[PASS] Physical ESP32-S3 Handshake Successful on {port}")
            return True
        else:
            log(f"[WARN] No response received from {port} within {timeout}s.")
            return False
    except Exception as e:
        log(f"[ERROR] Serial Communication Error: {e}")
        return False
    finally:
        if ser and ser.is_open:
            ser.close()
            log("Closed serial port connection.")

def main():
    parser = argparse.ArgumentParser(description="SmartGrow AI — Stratified HIL Test Runner")
    parser.add_argument("--port", type=str, default=None, help="Explicit serial COM port for target ESP32-S3")
    parser.add_argument("--sim-only", action="store_true", help="Force software simulation mode only")
    parser.add_argument("--probe", action="store_true", help="Only probe available serial ports and report status")
    parser.add_argument("--timeout", type=float, default=3.0, help="Serial step timeout in seconds")
    args = parser.parse_args()

    print("=======================================================")
    print("      SMARTGROW AI — HARDWARE-IN-THE-LOOP (HIL) RUNNER ")
    print("=======================================================")

    if args.probe:
        log("Probing host COM / Serial ports...")
        ports = probe_serial_ports()
        if not ports:
            log("No serial ports detected on host system.")
        for p in ports:
            kind = "Bluetooth (ignored)" if p["is_bluetooth"] else ("ESP32 Candidate" if p["is_esp_candidate"] else "Generic Serial")
            log(f"  Port: {p['device']} | Desc: {p['description']} | Type: {kind}")
        return 0

    physical_status = "NOT TESTED"
    simulation_status = "NOT TESTED"

    # 1. Level B: Run Firmware Simulation
    sim_ok = run_simulation()
    simulation_status = "SIMULATION_PASS" if sim_ok else "FAILED"

    # 2. Level C: Physical HIL
    if args.sim_only:
        physical_status = "SKIPPED (Simulation-only mode selected)"
    else:
        target_port = args.port or find_esp32_serial_port()
        if target_port:
            phys_ok = run_physical_test(target_port, timeout=args.timeout)
            physical_status = "PHYSICAL_PASS" if phys_ok else "FAILED"
        else:
            log("[INFO] No Physical ESP32-S3 USB COM port detected on host.")
            physical_status = "SKIPPED (No USB serial ESP32-S3 connected)"

    print("=======================================================")
    print("  HIL VERIFICATION SUMMARY:")
    print(f"  - Level A/B (Firmware & Safety Logic Sim): {simulation_status}")
    print(f"  - Level C   (Physical Board Serial Test): {physical_status}")
    print("=======================================================")

    if simulation_status == "FAILED" or physical_status == "FAILED":
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
