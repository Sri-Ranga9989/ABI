@echo off
set IDF_TOOLS_PATH=D:\.espressif
call "D:\.espressif\v6.0.2\esp-idf\export.bat" >nul 2>&1
if "%~1"=="" (
    idf.py -p COM5 flash monitor
) else (
    idf.py -p %1 flash monitor
)
