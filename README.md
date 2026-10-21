# Abi AI Assistant - ESP32-S3 (N16R8)

ESP32-S3 AI Assistant hardware foundation with INMP441 I2S microphone, push-to-talk recording to SPIFFS flash storage, web dashboard, and PC extraction tool.

---

## Hardware Pin Connections

### 1. INMP441 MEMS Microphone
| INMP441 Pin | ESP32-S3 Pin | Function |
| :--- | :--- | :--- |
| **VDD** | **3V3** | 3.3V Power |
| **GND** | **GND** | Ground |
| **SCK** | **GPIO 4** | I²S Bit Clock (BCLK) |
| **WS** | **GPIO 5** | I²S Word Select (LRCLK) |
| **SD** | **GPIO 6** | I²S Serial Data Output (DIN) |
| **L/R** | **GND** | Left channel select |

### 2. Push Button (Voice Recording Trigger)
| Push Button | ESP32-S3 Pin | Description |
| :--- | :--- | :--- |
| **Terminal 1** | **GPIO 7** | Input with internal pull-up |
| **Terminal 2** | **GND** | Ground (active low on press) |

*(Note: The on-board **BOOT button (GPIO 0)** also works simultaneously as a recording button!)*

### 3. ST7735 1.8" SPI TFT Display (160x128)
| ST7735 Pin | ESP32-S3 Pin | Function |
| :--- | :--- | :--- |
| **CS** | **GPIO 11** | Chip Select (Active Low) |
| **RST** | **GPIO 17** | Hardware Reset |
| **DC / A0** | **GPIO 18** | Data / Command Select |
| **SDA / MOSI** | **GPIO 15** | SPI Data (Master Out Slave In) |
| **SCL / SCK** | **GPIO 16** | SPI Clock (20 MHz, SPI2_HOST) |
| **VCC** | **3.3V** | Power Supply |
| **GND** | **GND** | Ground Reference |
| **BLK / LED** | **3.3V** | Display Backlight |

---

## Voice Recording & Storage

1. **Push-to-Talk Recording**:
   * **Press & Hold** GPIO 7 (or BOOT button GPIO 0): INMP441 starts capturing audio into a high-speed PSRAM buffer.
   * **Release** button: The audio is saved as standard **16 kHz, 16-bit Mono WAV** (`/storage/rec_XXXX.wav`) inside the 8MB SPIFFS flash partition.
2. **Directory Listing**:
   * Upon release, the monitor lists all stored recordings with their file sizes and durations.

---

## How to Review & Listen to Recordings on PC

### Method 1: Pull to Computer via USB (One-Command)
Make sure the serial monitor is closed, then run in your terminal:
```powershell
.\pull.bat
```
*(Or `python pull_recordings.py`)*

This connects to `COM5`, downloads all `.wav` files into [`recordings/`](file:///d:/eps32projects/Abi/recordings), and lets you play them directly on Windows!

### Method 2: Listen in Browser via Web Dashboard
When connected to Wi-Fi, open the ESP32's IP address in your browser:
```text
http://<ESP32_IP>/
```
You will see a dark-mode web dashboard with audio players to listen to recordings directly in your browser, plus download buttons!

---

## Commands

| Action | Command |
| :--- | :--- |
| **Build Firmware** | `.\build.bat` |
| **Flash & Monitor** | `.\flash.bat` |
| **Pull Files to PC** | `.\pull.bat` |
