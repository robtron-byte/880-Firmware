# Notes for the original firmware (minisystem/880-firmware 1.1.5)

Hi! These are build optimizations and bugs we found in the 1.1.5 code (commit `bf810b4`) while working on a fork for the 880 Desktop (MK2). They don't cover our new features, only things that may be useful for the official firmware. File and line references point to `bf810b4`.

## 1. Build: about 3.9 KB less flash, just from compiler options

The Atmel Studio project builds with `-O1`. The same 1.1.5 sources built with avr-gcc 7.3 and the ATmega DFP 1.0.98:

| Options | Flash (text + data) |
|---|---|
| `-O1` (current Release config) | 27,076 B |
| `-Os -flto -mrelax -mcall-prologues`, linker `-Os -flto -mrelax -Wl,--relax -Wl,--gc-sections` | 23,200 B |

That is **3,876 B (14 %) less**, with no source change. The bootloader limit is 28,672 B, so this leaves a lot more room for features.

- In Atmel Studio:
  - Optimization level "Optimize for size (-Os)".
  - Add `-flto -mrelax -mcall-prologues` to the compiler's and linker's "Other flags".
  - Enable "Garbage collect unused sections" (linker).
- `-mcall-prologues` makes function entry and exit a few cycles slower. We did not notice any timing effect. Without it, the build is still well below the limit.
- With LTO, objects from an old build without header dependency tracking (`-MMD -MP`) can cause confusing type mismatch errors. A clean build fixes them.

## 2. Bugs

### 2.1 Recall data overlaps rhythm track 1 in the EEPROM

`twi_eeprom.h:27-29`

```c
#define RHYTHM_TRACK_ADDR_OFFSET NUM_BANKS*BANK_SIZE + 1
#define RECALL_ADDR_OFFSET (RHYTHM_TRACK_ADDR_OFFSET*NUM_TRACKS + 1)
```

The first macro has no parentheses. `RECALL_ADDR_OFFSET` therefore expands to `NUM_BANKS*BANK_SIZE + 1*NUM_TRACKS + 1`:

- `RECALL_ADDR_OFFSET` = 49165
- `RHYTHM_TRACK_ADDR_OFFSET` = 49153

Rhythm track 0 (the first rhythm, AC key) is 65 bytes long and occupies 49153–49217, so the recall data (last pattern/bank, MIDI channel, trigger assignment, sync mode) lies **inside** it:

- Writing rhythm 1 in COMPOSE overwrites the recall settings.
- Saving the recall settings corrupts measures of rhythm 1.

Fix: put the macros in parentheses and give the recall data its own area after the last track (e.g. `RHYTHM_TRACK_ADDR_OFFSET + NUM_TRACKS*TRACK_SIZE`). This moves existing recall data, so it needs a one-time migration or a reset of the settings.

Minor detail: `int memory_address = RECALL_ADDR_OFFSET;` (`twi_eeprom.c:242/279`) does not fit 49165 into a signed 16-bit `int`. It works by accident; `uint16_t` would be correct.

### 2.2 Missing `break`: instrument keys in PLAY/COMPOSE RHYTHM

`switches.c:355-373`. `case PLAY_RHYTHM: case COMPOSE_RHYTHM:` has no `break` and falls through into `case MANUAL_PLAY:`. As a result:

- **SHIFT + instrument** calls `assign_mutes()` twice. The mute is toggled on and immediately off again, so muting in PLAY/COMPOSE does nothing.
- **Instrument without SHIFT** also runs `select_instrument()` after selecting the rhythm track. This changes the selected instrument, and `assign_triggers()` with SHIFT + I/F-VAR is reachable from there as well.

Fix: add a `break;` before `case MANUAL_PLAY:`.

### 2.3 TAP is only handled at the next step and not rounded

`sequencer.c:631/643` and `check_tap()` at `sequencer.c:1440`. `flag.tap` is only evaluated inside `process_step()`, i.e. when the next step starts. The hit is then written into `sequencer.current_step`:

- A TAP shortly *after* a step (the most common case when playing along) lands one step later.
- A TAP is never rounded to the nearest step.
- No sound is triggered until the next step.

In our fork, TAP is evaluated right away in the main loop:

- It is rounded to the nearest step using `clock.ppqn_counter` and `clock.divider`, including shuffle.
- If the step has already played, the sound is triggered immediately.

### 2.4 Keys are not really debounced, and pending presses can cancel each other

`spi.c:124ff` and `switches.c:77`. The block marked `//debounce` in `spi.c` only detects rising edges. The comment above it already points out that this is not debouncing. A bouncing contact can therefore produce two presses, e.g. a step toggled twice, which means no change.

`parse_switch_data()` then uses `button[i].state ^= current_state;`. If a second edge arrives before the first one has been handled, the two cancel and the press is lost.

Fix:

- A simple 2-sample debounce: a key counts as pressed when it is pressed in two consecutive scans.
- `|=` instead of `^=` for the pending flag. The handler clears it anyway.

### 2.5 TWI runs at 800 kHz

`twi_eeprom.c:69`. `TWI_init(F_CPU, 800000L, ...)` gives `TWBR = 2` and really runs the bus at 800 kHz. Many 24C512/24LC512 EEPROMs are only specified up to 400 kHz; some newer ones (e.g. AT24C512C) up to 1 MHz. If the fitted chip is a 400 kHz part, this is out of spec. Please check the part. We left it unchanged because it works on our unit.

### 2.6 `volatile` bitfield flags shared between interrupt and main loop

`sequencer.h:82/220`. `struct flag` is a `volatile` bitfield:

- Interrupts set several bits of it: `trig_finished`, `blink`, `din_start`, `din_stop`, `slave_start`, … (`interrupts.c`, `clock.c`).
- The main loop writes other bits in the same bytes.

Every bitfield write is a read-modify-write and not atomic. An interrupt between the read and the write is lost, e.g. a single missed `blink` or `din_stop`. It happens rarely and is hard to reproduce.

Fix: put bits written by interrupts into their own bytes (or separate `volatile uint8_t` variables), or wrap the main-loop writes in `ATOMIC_BLOCK`. **Not fixed in our fork either.**

## 3. Optimization: EEPROM access without blocking the main loop

`twi_eeprom.c:93-108` and `166-183`. `read_pattern()` and `write_pattern_page()` wait in `while(TWI_busy);` until the transfer is finished.

At the end of a measure, the main loop therefore stalls for several milliseconds:

- writing the edited pattern (2 pages),
- right after that, reading the next pattern while the EEPROM is still in its internal write cycle. The NACK makes the TWI restart repeatedly until the chip is ready, up to about 5 ms.

MIDI, step timing and the UI wait during this time.

In our fork, writes run as a background job, page by page:

- The main loop only starts the next transfer once the previous one is finished.
- Retries after a NACK happen in the interrupt.

The read of the next pattern stays synchronous. However, it only starts once the write is finished and the EEPROM has completed its write cycle (a few ms of wait time are tracked). This avoids the long NACK loop.

## 4. Tools that may be useful

These are in our fork (https://github.com/robtron-byte/880-Firmware). You are welcome to use them:

- **`Makefile`**: builds on Linux/macOS without Atmel Studio (`make dfp` downloads the device pack once, then `make`). It fails if the flash exceeds 28,672 B or if less than 500 B of RAM is left for the stack.
- **`tools/hex2syx.py`**: converts the `.hex` into the 880 sysex update format.
  - Format per 128-byte page: `F0 00 79 08 00 7F 7E 00` + 256 nibbles + checksum (sum & 0xFF as 2 nibbles) + `F7`.
  - End message: `F0 00 79 08 00 7F 7F 00 F7`.
  - `--selftest` decodes the official `880-1.1.4-MK2.syx` and re-encodes it byte-identically.
- **Host unit tests** (Unity, `run_tests.sh`): built with the same type options as the firmware (`-fpack-struct -funsigned-char -funsigned-bitfields -fshort-enums`), so struct sizes and enum widths match the target.

Thanks for the great firmware!
