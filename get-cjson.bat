@echo off
setlocal
if not exist third_party mkdir third_party
powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -UseBasicParsing https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c -OutFile third_party/cJSON.c; Invoke-WebRequest -UseBasicParsing https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h -OutFile third_party/cJSON.h"
if errorlevel 1 exit /b %errorlevel%
echo cJSON downloaded.
endlocal
