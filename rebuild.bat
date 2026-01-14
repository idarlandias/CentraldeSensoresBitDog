@echo off
echo Check SDK libs...
dir "C:\Users\Admini\.pico-sdk\sdk\1.5.1\lib" /b
echo Cleaning build...
if exist build_fresh rmdir /s /q build_fresh
mkdir build_fresh
cd build_fresh
echo Configuring...
set PATH=%PATH%;C:\Users\Admini\.pico-sdk\ninja\v1.12.1;C:\Users\Admini\.pico-sdk\toolchain\13_2_Rel1\bin
cmake -G "Ninja" -DPICO_SDK_PATH=C:/Users/Admini/.pico-sdk/sdk/1.5.1 -DPICO_CYW43_ARCH=polling ..
if %errorlevel% neq 0 (
    echo Configuration failed
    exit /b %errorlevel%
)
echo Building...
ninja -j 1 -v
