# USB Power Gauge firmware. Host tests do not need avr-gcc.
#   make test
#   make
#   make flash PROGRAMMER=usbtiny CONFIRM=yes

include firmware/fuses.mk

F_CPU ?= 8000000
MCU ?= attiny85
PROGRAMMER ?= usbtiny

BUILD := build/fw-$(F_CPU)
HOST_BUILD := build/host

ifeq ($(origin AVR_GCC), undefined)
  AVR_GCC := $(shell command -v avr-gcc 2>/dev/null)
  ifeq ($(AVR_GCC),)
    ifneq ($(wildcard $(HOME)/.local/avr-sdk/avr/bin/avr-gcc),)
      AVR_GCC := $(HOME)/.local/avr-sdk/avr/bin/avr-gcc
    endif
  endif
endif

AVR_BIN := $(patsubst %/,%,$(dir $(AVR_GCC)))
OBJCOPY := $(AVR_BIN)/avr-objcopy
OBJDUMP := $(AVR_BIN)/avr-objdump
SIZE := $(AVR_BIN)/avr-size

HOST_CC ?= gcc
HOST_CFLAGS := -std=c11 -Wall -Wextra -Werror -Wstrict-prototypes -Wmissing-prototypes \
	-Wshadow -Wundef -O2 -Ifirmware -Ifirmware/portable -Ifirmware/tests

AVR_CFLAGS := -std=c11 -mmcu=$(MCU) -Os -Wall -Wextra -Werror -Wstrict-prototypes \
	-Wmissing-prototypes -Wshadow -Wundef -ffunction-sections -fdata-sections \
	-DF_CPU=$(F_CPU)UL -Ifirmware -Ifirmware/portable -Ifirmware/avr
AVR_LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections -Wl,--relax -Wl,-Map,$(BUILD)/firmware.map

PORTABLE_SRC := $(wildcard firmware/portable/*.c)
AVR_SRC := $(wildcard firmware/avr/*.c)
TEST_SRC := $(wildcard firmware/tests/*.c)

PORTABLE_OBJ := $(patsubst firmware/portable/%.c,$(BUILD)/portable/%.o,$(PORTABLE_SRC))
AVR_OBJ := $(patsubst firmware/avr/%.c,$(BUILD)/avr/%.o,$(AVR_SRC))
HOST_OBJ := $(patsubst firmware/portable/%.c,$(HOST_BUILD)/portable/%.o,$(PORTABLE_SRC)) \
	$(patsubst firmware/tests/%.c,$(HOST_BUILD)/tests/%.o,$(TEST_SRC))

ELF := $(BUILD)/usb-power-gauge.elf
HEX := $(BUILD)/usb-power-gauge.hex

.PHONY: all test test-c test-py firmware size-check flash fuses eeprom-write clean

all: firmware size-check

firmware: $(HEX)

test: test-c test-py

test-c: $(HOST_BUILD)/run-tests
	./$(HOST_BUILD)/run-tests

test-py: test-c
	python3 tools/test_tools.py

$(HOST_BUILD)/run-tests: $(HOST_OBJ)
	@mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(HOST_OBJ)

$(HOST_BUILD)/portable/%.o: firmware/portable/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HOST_BUILD)/tests/%.o: firmware/tests/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(ELF): $(PORTABLE_OBJ) $(AVR_OBJ)
	@mkdir -p $(BUILD)
	$(AVR_GCC) $(AVR_CFLAGS) $(AVR_LDFLAGS) -o $@ $(PORTABLE_OBJ) $(AVR_OBJ)

$(BUILD)/portable/%.o: firmware/portable/%.c
	@test -n "$(AVR_GCC)" || { echo "avr-gcc not found. Run tools/fetch-toolchain.sh or install gcc-avr."; exit 1; }
	@mkdir -p $(dir $@)
	$(AVR_GCC) $(AVR_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/avr/%.o: firmware/avr/%.c
	@test -n "$(AVR_GCC)" || { echo "avr-gcc not found. Run tools/fetch-toolchain.sh or install gcc-avr."; exit 1; }
	@mkdir -p $(dir $@)
	$(AVR_GCC) $(AVR_CFLAGS) -MMD -MP -c $< -o $@

size-check: $(ELF)
	$(SIZE) -C --mcu=$(MCU) $(ELF)
	$(SIZE) -A $(ELF) | python3 tools/check_size.py

flash: $(HEX)
	@test "$(CONFIRM)" = "yes" || { echo "Refusing to program. Read docs/hardware.md, then: make flash PROGRAMMER=$(PROGRAMMER) CONFIRM=yes"; exit 1; }
	avrdude -c $(PROGRAMMER) -p t85 -U flash:w:$(HEX):i

fuses:
	@test "$(CONFIRM)" = "yes" || { echo "Refusing to write fuses ($(LFUSE)/$(HFUSE)/$(EFUSE)). See firmware/fuses.mk. Then: make fuses PROGRAMMER=$(PROGRAMMER) CONFIRM=yes"; exit 1; }
	avrdude -c $(PROGRAMMER) -p t85 -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m

eeprom-write:
	@test "$(CONFIRM)" = "yes" || { echo "Refusing. make eeprom-write IMAGE=cal.bin PROGRAMMER=$(PROGRAMMER) CONFIRM=yes"; exit 1; }
	@test -n "$(IMAGE)" || { echo "Set IMAGE=path/to/cal.bin"; exit 1; }
	avrdude -c $(PROGRAMMER) -p t85 -U eeprom:w:$(IMAGE):r

clean:
	rm -rf build

-include $(wildcard $(BUILD)/portable/*.d) $(wildcard $(BUILD)/avr/*.d)
-include $(wildcard $(HOST_BUILD)/portable/*.d) $(wildcard $(HOST_BUILD)/tests/*.d)
