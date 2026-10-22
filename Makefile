PROJECT := blinky
BUILD_DIR := build
OPENCM3_DIR ?= libopencm3
DEVICE := stm32f303k8t6

PREFIX ?= arm-none-eabi
CC := $(PREFIX)-gcc
OBJCOPY := $(PREFIX)-objcopy
SIZE := $(PREFIX)-size

OPENOCD ?= openocd
OOCD_INTERFACE ?= interface/stlink.cfg
OOCD_TARGET ?= target/stm32f3x.cfg

TARGET_ELF := $(BUILD_DIR)/$(PROJECT).elf
TARGET_BIN := $(BUILD_DIR)/$(PROJECT).bin

C_SOURCES := app/src/main.c
C_SOURCES += app/src/core/system.c
OBJECTS := $(patsubst app/src/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
DEVICES_DATA := $(OPENCM3_DIR)/ld/devices.data
GENLINK_DEFS := -DSTM32F3 -DSTM32F3CCM -DSTM32F303K8T6 -D_ROM=64K -D_RAM=12K -D_CCM=4K -D_CCM_OFF=0x10000000 -D_ROM_OFF=0x08000000 -D_RAM_OFF=0x20000000
GENLINK_CPPFLAGS := -DSTM32F3 -DSTM32F3CCM -DSTM32F303K8T6
ARCH_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDSCRIPT := $(BUILD_DIR)/generated.$(DEVICE).ld

CFLAGS := -std=c11 -O2 -g3 -Wall -Wextra -ffunction-sections -fdata-sections -fno-common
CFLAGS += $(ARCH_FLAGS) $(GENLINK_CPPFLAGS) -I$(OPENCM3_DIR)/include -Iapp/inc
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Wl,--gc-sections
LDLIBS := -L$(OPENCM3_DIR)/lib -lopencm3_stm32f3 -lc -lgcc -lnosys

.PHONY: all clean flash openocd reset check-lib

all: check-lib $(TARGET_ELF) $(TARGET_BIN)

check-lib:
	@powershell -Command "if (!(Test-Path '$(OPENCM3_DIR)/lib/libopencm3_stm32f3.a')) { Write-Host 'libopencm3 for STM32F3 is missing.'; Write-Host 'Run: powershell -ExecutionPolicy Bypass -File scripts/setup_libopencm3.ps1'; exit 1 }"

$(BUILD_DIR):
	@powershell -Command "if (!(Test-Path '$(BUILD_DIR)')) { New-Item -ItemType Directory -Force -Path '$(BUILD_DIR)' | Out-Null }"

$(BUILD_DIR)/%.o: app/src/%.c | $(BUILD_DIR)
	@powershell -Command "if (!(Test-Path '$(dir $@)')) { New-Item -ItemType Directory -Force -Path '$(dir $@)' | Out-Null }"
	$(CC) $(CFLAGS) -c $< -o $@

$(LDSCRIPT): $(OPENCM3_DIR)/ld/linker.ld.S $(DEVICES_DATA) | $(BUILD_DIR)
	$(CC) -E -P $(GENLINK_DEFS) -x c $< -o $@

$(TARGET_ELF): $(OBJECTS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) -T$(LDSCRIPT) $(OBJECTS) $(LDLIBS) -o $@
	$(SIZE) $@

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -Obinary $< $@

flash: $(TARGET_ELF)
	$(OPENOCD) -f $(OOCD_INTERFACE) -f $(OOCD_TARGET) -c "adapter speed 4000" -c "program $(TARGET_ELF) verify reset exit"

openocd:
	$(OPENOCD) -f $(OOCD_INTERFACE) -f $(OOCD_TARGET) -c "adapter speed 4000"

reset:
	$(OPENOCD) -f $(OOCD_INTERFACE) -f $(OOCD_TARGET) -c "init" -c "reset run" -c "exit"

clean:
	@powershell -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }"
