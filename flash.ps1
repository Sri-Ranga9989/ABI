# flash.ps1 - Flashes firmware and opens serial monitor
param(
    [string]$Port = "COM5"
)

$env:IDF_TOOLS_PATH = "D:\.espressif"
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    . "D:\.espressif\v6.0.2\esp-idf\export.ps1"
}

Write-Host "[ESP-IDF] Flashing to $Port and starting monitor..." -ForegroundColor Green
idf.py -p $Port flash monitor
