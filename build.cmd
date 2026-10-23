@echo off
setlocal

rem Detect toolchain path or fallback to PATH
set "GCC=arm-none-eabi-gcc"
set "OBJCOPY=arm-none-eabi-objcopy"
if exist "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gcc.exe" (
    set "GCC=C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gcc.exe"
    set "OBJCOPY=C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-objcopy.exe"
)

rem Ensure build directories exist
if not exist build mkdir build
if not exist build\core mkdir build\core

echo Compiling app/src/main.c...
"%GCC%" -std=c11 -O2 -g3 -Wall -Wextra -ffunction-sections -fdata-sections -fno-common -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -DSTM32F3 -DSTM32F3CCM -DSTM32F303K8T6 -Ilibopencm3/include -Iapp/inc -c app/src/main.c -o build/main.o
if errorlevel 1 exit /b 1

echo Compiling app/src/core/system.c...
"%GCC%" -std=c11 -O2 -g3 -Wall -Wextra -ffunction-sections -fdata-sections -fno-common -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -DSTM32F3 -DSTM32F3CCM -DSTM32F303K8T6 -Ilibopencm3/include -Iapp/inc -c app/src/core/system.c -o build/core/system.o
if errorlevel 1 exit /b 1

echo Linking build/blinky.elf...
"%GCC%" -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -nostartfiles -Wl,--gc-sections -T build/generated.stm32f303k8t6.ld build/main.o build/core/system.o -Llibopencm3/lib -L../stm-nucleo/libopencm3/lib -lopencm3_stm32f3 -lc -lgcc -lnosys -o build/blinky.elf
if errorlevel 1 exit /b 1

echo Creating binary build/blinky.bin...
"%OBJCOPY%" -Obinary build/blinky.elf build/blinky.bin
if errorlevel 1 exit /b 1

if exist F:\ (
    echo Flashing via ST-Link drive F:...
    copy /y build\blinky.bin F:\
    echo Flashed successfully!
) else (
    openocd -f interface/stlink.cfg -f target/stm32f3x.cfg -c "adapter speed 4000" -c "program build/blinky.elf verify reset exit"
)
