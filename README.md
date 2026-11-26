# 880 firmware

Written for Atemga328PB in Microchip Studio

Version 1.1.4

See https://github.com/minisystem/880-firmware/tree/1.1.3 for earlier firmware versions

## Build on Linux / macOS

```
sudo apt install gcc-avr avr-libc binutils-avr   # toolchain
make dfp      # download Microchip ATmega device pack (ATmega328PB support)
make          # -> build/880-firmware.hex
make test     # host unit tests
make flash PROGRAMMER=usbasp   # optional, via avrdude
```

Panel variant: `mode.c` (`update_clock_mode_leds`) is set to MK2 sync-LED layout by default; the MK1 block is commented out.
