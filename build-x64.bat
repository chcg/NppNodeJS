@echo off
setlocal

rem Set DEBUG=1 when a debug build should write NppNodeJS-debug.log by default.
rem menu.json "debug": true/false overrides this default.
if not defined DEBUG set DEBUG=0

if not "%DEBUG%"=="0" if not "%DEBUG%"=="1" (
  echo DEBUG must be 0 or 1.
  exit /b 1
)

if not exist build mkdir build
if not exist third_party\cJSON.c (
  echo Missing third_party\cJSON.c
  echo Run get-cjson.bat first.
  exit /b 1
)
if not exist third_party\cJSON.h (
  echo Missing third_party\cJSON.h
  echo Run get-cjson.bat first.
  exit /b 1
)

windres src\NppNodeJS.rc build\NppNodeJS-res.o
if errorlevel 1 exit /b %errorlevel%

g++ -m64 -std=c++17 -O2 -shared -static -static-libgcc -static-libstdc++ ^
  -DNPPNODEJS_DEBUG_DEFAULT=%DEBUG% ^
  -Isrc -Ithird_party ^
  src/main.cpp third_party/cJSON.c build/NppNodeJS-res.o ^
  -o build/NppNodeJS.dll ^
  -luser32 -lgdi32 -lkernel32 -lcomdlg32 -lshell32

if errorlevel 1 exit /b %errorlevel%
echo Built build\NppNodeJS.dll (DEBUG=%DEBUG%)
endlocal
