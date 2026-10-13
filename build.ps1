# build.ps1 - Compiles project for ESP32-S3 N16R8
$env:IDF_TOOLS_PATH = "D:\.espressif"
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    . "D:\.espressif\v6.0.2\esp-idf\export.ps1"
}
idf.py build
