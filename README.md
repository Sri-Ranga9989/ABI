# Abi AI Assistant - ESP32-S3 (N16R8) Network & Hardware Diagnostics

This firmware project tests hardware capabilities (16MB Flash, 8MB Octal PSRAM) and provides network diagnostics (Wi-Fi connectivity, DNS resolution, and Internet connectivity).

---

## Hardware Configuration (N16R8)

- **Target Chip**: ESP32-S3 (Dual-Core 240MHz)
- **Flash Memory**: 16 MB (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`, DIO Mode)
- **PSRAM Memory**: 8 MB Octal SPI PSRAM (`CONFIG_SPIRAM_MODE_OCT=y`, 80MHz)
- **App Partition**: 4 MB factory application space (`partitions.csv`)

---

## Quick Setup & Commands

### 1. Configure Your Wi-Fi Credentials
Choose one of the two methods:
- **Method A (Direct in Code)**: Open `main/wifi_config.h` and uncomment:
  ```c
  #define WIFI_SSID_OVERRIDE   "Your_Home_WiFi"
  #define WIFI_PASS_OVERRIDE   "Your_WiFi_Password"
  ```
- **Method B (menuconfig)**: Run `idf.py menuconfig` and navigate to **Wi-Fi Configuration**.

### 2. Activate ESP-IDF (Just like your previous projects)
In your terminal, activate the toolchain using:
```powershell
. .\get_idf.ps1
```
*(Or if you have `get_idf` in your PowerShell profile, run: `$env:IDF_TOOLS_PATH="D:\.espressif"; get_idf`)*

### 3. Build the Project
Once activated:
```powershell
idf.py build
```
*(Or simply run `.\build.bat` or `.\build.ps1`)*

### 4. Flash and Monitor
Your board is connected on **COM5**:
```powershell
idf.py -p COM5 flash monitor
```
*(Or simply run `.\flash.bat` or `.\flash.ps1`)*

To exit the serial monitor, press `Ctrl + ]`.
