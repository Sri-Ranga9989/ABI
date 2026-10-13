@echo off
set IDF_TOOLS_PATH=D:\.espressif
call "D:\.espressif\v6.0.2\esp-idf\export.bat" >nul 2>&1
idf.py build %*
