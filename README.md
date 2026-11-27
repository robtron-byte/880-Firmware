# 880 firmware – 880 Perform

**880 Perform** ist eine Live-Firmware für die System80 880 Desktop (MK2), basierend auf 1.1.5:
- [Bedienungsanleitung](docs/BEDIENUNGSANLEITUNG.md)
- [Firmware aufspielen (per MIDI)](docs/UPDATE-ANLEITUNG.md)
- [Test-Checkliste](docs/TEST-CHECKLISTE.md)
- Fertige Datei: `880-perform-v1.xx.syx` im Ordner [`firmware`](firmware) (jede Version +0,01), Original zum Zurückspielen: [`firmware/official/880-1.1.4-MK2.syx`](firmware/official/880-1.1.4-MK2.syx)

---

## Original README


Written for Atemga328PB in Microchip Studio

Version 1.1.4

See https://github.com/minisystem/880-firmware/tree/1.1.3 for earlier firmware versions

## Build on Linux / macOS

```
sudo apt install gcc-avr avr-libc binutils-avr   # toolchain
make dfp      # download Microchip ATmega device pack (ATmega328PB support)
make          # -> build/880-firmware.hex and build/880-firmware.syx (MIDI update file)
make release  # copy the .syx to firmware/880-perform-v<version>.syx (+0.01 each release)
make test     # host unit tests
make flash PROGRAMMER=usbasp   # optional, via avrdude
```

Panel variant: `mode.c` (`update_clock_mode_leds`) is set to MK2 sync-LED layout by default; the MK1 block is commented out.
