# Linux/macOS build for the 880 (TR-808) firmware, ATmega328PB @ 16 MHz.
# Mirrors the Release configuration of TR-808-firmware/880-firmware.cproj.
#
#   make dfp     # download the Microchip ATmega device pack (once)
#   make         # build build/880-firmware.hex
#   make test    # run the host unit tests
#   make flash   # flash with avrdude (set PROGRAMMER/PORT)
#
# Flash limit: the sysex bootloader sits at the top of flash (max 4 KB on the
# ATmega328PB), so the application must stay below 28672 bytes.
FLASH_LIMIT := 28672

# Requires: avr-gcc, avr-libc, binutils-avr (apt install gcc-avr avr-libc binutils-avr)

MCU      := atmega328pb
SRC_DIR  := TR-808-firmware
BUILD    := build
TARGET   := $(BUILD)/880-firmware

DFP_VER  := 1.0.98
DFP_DIR  := tools/ATmega_DFP
DFP_URL  := http://packs.download.atmel.com/Atmel.ATmega_DFP.$(DFP_VER).atpack

CC       := avr-gcc
OBJCOPY  := avr-objcopy
SIZE     := avr-size

SRCS := adc.c clock.c drums.c interrupts.c leds.c main.c midi.c mode.c \
        sequencer.c spi.c switches.c twi.c twi_eeprom.c logic.c ui.c \
        select_instrument_with_soloing.c \
        xnormidi-develop/bytequeue/bytequeue.c \
        xnormidi-develop/bytequeue/interrupt_setting.c \
        xnormidi-develop/midi.c xnormidi-develop/midi_device.c \
        xnormidi-develop/sysex_tools.c
OBJS := $(addprefix $(BUILD)/obj/,$(SRCS:.c=.o))

CFLAGS := -mmcu=$(MCU) -B $(DFP_DIR)/gcc/dev/$(MCU) -I $(DFP_DIR)/include \
          -DNDEBUG -Os -flto -mrelax -mcall-prologues -std=gnu99 -Wall \
          -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums \
          -ffunction-sections -fdata-sections
LDFLAGS := -mmcu=$(MCU) -B $(DFP_DIR)/gcc/dev/$(MCU) -Os -flto -mrelax -mcall-prologues \
           -Wl,--relax -Wl,--gc-sections -Wl,-Map=$(TARGET).map
LDLIBS  := -lm

PROGRAMMER ?= usbasp
PORT       ?= usb

.PHONY: all clean dfp test flash size syx selftest release

all: $(TARGET).hex $(TARGET).eep $(TARGET).syx size

$(TARGET).syx: $(TARGET).hex
	python3 tools/hex2syx.py $< $@ --max-size $(FLASH_LIMIT)

syx: $(TARGET).syx

# every release goes up by 0.01: firmware/880-perform-v1.01.syx, v1.02, ... (firmware/RELEASE holds the number x100)
# (older 880-perform files are deleted so only the newest one is in firmware/)
release: all test
	@n=$$(( $$(cat firmware/RELEASE 2>/dev/null || echo 100) + 1 )); \
	name=firmware/880-perform-v$$((n / 100)).$$(printf %02d $$((n % 100))).syx; \
	find firmware -maxdepth 1 -name '880-perform*.syx' -delete; \
	cp $(TARGET).syx $$name; echo $$n > firmware/RELEASE; \
	echo "Release: $$name"

selftest:
	python3 tools/hex2syx.py --selftest firmware/official/880-1.1.4-MK2.syx

$(DFP_DIR)/include/avr/iom328pb.h:
	@echo "Device pack missing - run 'make dfp' first" && exit 1

dfp:
	mkdir -p $(DFP_DIR)
	curl -fL -o $(DFP_DIR)/pack.atpack $(DFP_URL)
	cd $(DFP_DIR) && unzip -q -o pack.atpack && rm pack.atpack

$(BUILD)/obj/%.o: $(SRC_DIR)/%.c | $(DFP_DIR)/include/avr/iom328pb.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures $< $@

$(TARGET).eep: $(TARGET).elf
	$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom=alloc,load \
	  --change-section-lma .eeprom=0 --no-change-warnings -O ihex $< $@

size: $(TARGET).elf
	@$(SIZE) $<
	@$(SIZE) $< | awk 'NR==2 { flash=$$1+$$2; ram=$$2+$$3; \
	  printf "Flash %d / $(FLASH_LIMIT) B, RAM %d / 2048 B (free %d B for stack)\n", flash, ram, 2048-ram; \
	  if (flash > $(FLASH_LIMIT)) { print "ERROR: flash too large"; exit 1 } \
	  if (2048-ram < 500) { print "ERROR: less than 500 B RAM left for stack"; exit 1 } }'

test: selftest
	cd $(SRC_DIR) && bash run_tests.sh

flash: $(TARGET).hex
	avrdude -p m328pb -c $(PROGRAMMER) -P $(PORT) -U flash:w:$(TARGET).hex:i

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
