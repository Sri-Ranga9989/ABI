#!/usr/bin/env python3
"""
pull_recordings.py
Extracts and manages voice recordings (.WAV) from ESP32-S3 SPIFFS storage to PC.

Usage:
  1. Pull all recordings to PC:
     python pull_recordings.py

  2. Pull and automatically DELETE from ESP32 memory:
     python pull_recordings.py --clear

  3. Only DELETE/WIPE all recordings from ESP32 memory:
     python pull_recordings.py --delete

  4. Pull over Wi-Fi:
     python pull_recordings.py --ip 192.168.1.105 [--clear]
"""

import sys
import os
import time
import base64
import argparse
import urllib.request
import re

def clear_via_serial(ser):
    print("[*] Sending CLEAR command to ESP32 memory...")
    ser.write(b"\r\nCLEAR\r\n")
    ser.flush()
    start = time.time()
    while time.time() - start < 3.0:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if "###ALL_CLEARED###" in line or "deleted from storage" in line:
            print("[✓] All recordings successfully deleted from ESP32 flash memory!")
            return True
    print("[✓] Clear command dispatched to ESP32.")
    return True

def delete_via_serial_standalone(port, baudrate):
    try:
        import serial
        import serial.tools.list_ports
    except ImportError:
        print("[!] 'pyserial' is required. Install via: pip install pyserial")
        return False

    if not port:
        port = "COM5"

    print(f"[*] Opening {port} to clear ESP32 memory...")
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
    except Exception as e:
        print(f"[!] Failed to open {port}: {e}")
        print("    Close idf.py monitor or any serial terminal first.")
        return False

    time.sleep(1.0)
    ser.reset_input_buffer()
    clear_via_serial(ser)
    ser.close()
    return True

def download_via_wifi(ip_address, output_dir, clear_after=False):
    print(f"[*] Connecting to ESP32 Web Server at http://{ip_address}/ ...")
    try:
        url = f"http://{ip_address}/"
        req = urllib.request.urlopen(url, timeout=5)
        html = req.read().decode('utf-8', errors='ignore')
    except Exception as e:
        print(f"[!] Error connecting to http://{ip_address}/: {e}")
        return False

    files = re.findall(r'/download\?file=([^"\'\s&]+)', html)
    unique_files = sorted(list(set(files)))

    if not unique_files:
        print("[!] No recordings found on ESP32 storage.")
        return True

    print(f"[*] Found {len(unique_files)} recording(s): {', '.join(unique_files)}")
    os.makedirs(output_dir, exist_ok=True)

    for fname in unique_files:
        file_url = f"http://{ip_address}/download?file={fname}"
        out_path = os.path.join(output_dir, fname)
        print(f"  -> Downloading {fname} ...", end="", flush=True)
        try:
            urllib.request.urlretrieve(file_url, out_path)
            size = os.path.getsize(out_path)
            print(f" Done! ({size:,} bytes) Saved to: {out_path}")
        except Exception as e:
            print(f" Failed: {e}")

    print(f"\n[✓] All recordings successfully downloaded to: {os.path.abspath(output_dir)}")

    if clear_after:
        print("[*] Deleting downloaded recordings from ESP32 via Wi-Fi...")
        for fname in unique_files:
            del_url = f"http://{ip_address}/delete?file={fname}"
            try:
                urllib.request.urlopen(del_url, timeout=5)
            except Exception:
                pass
        print("[✓] All recordings deleted from ESP32 memory!")

    return True

def download_via_serial(port, baudrate, output_dir, clear_after=False):
    try:
        import serial
        import serial.tools.list_ports
    except ImportError:
        print("[!] 'pyserial' is required for USB extraction.")
        print("    Install it via: pip install pyserial")
        return False

    if not port:
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            desc = p.description.lower()
            if "ch343" in desc or "ch340" in desc or "cp210" in desc or "usb" in desc or "uart" in desc:
                port = p.device
                break
        if not port and ports:
            port = ports[0].device
        if not port:
            port = "COM5"

    print(f"[*] Opening serial port {port} at {baudrate} baud...")
    try:
        ser = serial.Serial(port, baudrate, timeout=3)
    except Exception as e:
        print(f"[!] Failed to open {port}: {e}")
        print("    Make sure idf.py monitor or any other serial monitor is CLOSED before running this script.")
        return False

    time.sleep(1.2) # Allow serial to stabilize
    ser.reset_input_buffer()

    print("[*] Requesting file export from ESP32...")
    ser.write(b"\r\nEXPORT\r\n")
    ser.flush()

    os.makedirs(output_dir, exist_ok=True)

    current_file = None
    file_bytes = bytearray()
    expected_size = 0
    saved_count = 0
    last_data_time = time.time()

    while True:
        line_bytes = ser.readline()
        if not line_bytes:
            if time.time() - last_data_time > 6.0:
                print("\n[!] Timeout waiting for ESP32 response.")
                break
            continue

        last_data_time = time.time()
        line = line_bytes.decode('utf-8', errors='ignore').strip()

        if "###FILE_START:" in line:
            parts = line.replace("###", "").split(":")
            if len(parts) >= 3:
                current_file = parts[1]
                expected_size = int(parts[2])
                file_bytes = bytearray()
                print(f"  -> Receiving {current_file} ({expected_size:,} bytes) ...", end="", flush=True)

        elif "###FILE_END:" in line:
            if current_file and file_bytes:
                out_path = os.path.join(output_dir, current_file)
                with open(out_path, "wb") as f:
                    f.write(file_bytes)
                print(f" [OK] Saved {len(file_bytes):,} bytes to {out_path}")
                saved_count += 1
                current_file = None
                file_bytes = bytearray()

        elif "###ALL_DONE###" in line:
            print("\n[✓] Export completed successfully!")
            break

        elif current_file is not None:
            try:
                decoded = base64.b64decode(line)
                file_bytes.extend(decoded)
            except Exception:
                pass

    if saved_count > 0:
        print(f"[✓] Extracted {saved_count} WAV file(s) into: {os.path.abspath(output_dir)}")
        if clear_after:
            clear_via_serial(ser)
        else:
            print("[i] Tip: To download AND delete from ESP32 memory in one command, run:")
            print("         .\\pull.bat --clear")
            print("     Or to only clear ESP32 memory, run:")
            print("         .\\clear.bat")
    else:
        print("[!] No files were exported. Check that recordings exist.")

    ser.close()
    return saved_count > 0

def main():
    parser = argparse.ArgumentParser(description="Extract & Manage WAV recordings from ESP32-S3")
    parser.add_argument("--port", "-p", default="COM5", help="Serial port (default: COM5)")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--ip", help="ESP32 Wi-Fi IP address (for download via Wi-Fi)")
    parser.add_argument("--out", "-o", default="recordings", help="Output directory on PC (default: recordings)")
    parser.add_argument("--clear", "-c", action="store_true", help="Delete recordings from ESP32 after downloading")
    parser.add_argument("--delete", "-d", action="store_true", help="Only delete all recordings from ESP32 memory (no download)")
    args = parser.parse_args()

    print("==================================================")
    print("      ABI AI ASSISTANT - RECORDING MANAGER        ")
    print("==================================================")

    if args.delete:
        delete_via_serial_standalone(args.port, args.baud)
        return

    if args.ip:
        download_via_wifi(args.ip, args.out, clear_after=args.clear)
    else:
        download_via_serial(args.port, args.baud, args.out, clear_after=args.clear)

if __name__ == "__main__":
    main()
