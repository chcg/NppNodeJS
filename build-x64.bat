@echo off
setlocal
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

g++ -m64 -std=c++17 -O2 -shared -static -static-libgcc -static-libstdc++ ^
  -Isrc -Ithird_party ^
  src/main.cpp third_party/cJSON.c ^
  -o build/NppNodeJS.dll ^
  -luser32 -lgdi32 -lkernel32 -lcomdlg32

if errorlevel 1 exit /b %errorlevel%
echo Built build\NppNodeJS.dll
endlocal
