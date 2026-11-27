# 880 Perform – feature changes vs. 1.1.5

A one-hand live firmware for the 880 Desktop (MK2), based on 1.1.5. Bugs and build options: see [NOTES-FOR-UPSTREAM.md](NOTES-FOR-UPSTREAM.md).

## Removed
- Song mode (replaced by pattern chains)
- 32nd sub-steps (2nd part is always as long as the 1st)
- Roll mode (replaced by step loop and pad repeat)
- AUD live hits (replaced by live recording)
- Trigger inputs and assignable trigger outs (OUT 1 = Low Tom, OUT 2 = Low Conga, fixed)
- Pre-scale selection (fixed to PRE-SCALE 3)
- Shift lock, step fill, PATTERN CLEAR mode (clear all, version display)
- Modes loading patterns when switched

## Added
- **MODE** switches seamlessly: PERFORM → 1st → 2nd part (if used). SHIFT + MODE activates the 2nd part (auto-copied from the 1st).
- **Autosave** in the background at the end of the measure; **reload** to the state at selection (SHIFT + TAP).
- **TAP write** rounded to the nearest 16th, written where the playhead is.
- **Pattern layer** (BASIC-VAR click): patterns on steps 1–12, bank via BASIC-VAR held + step, pattern chain, copy, delete.
- **Odds and ratchets** per step and instrument, editable on several held steps at once and shown by LED patterns.
- **Playback direction** per pattern: forward, reverse, ping-pong, random.
- **CLEAR** click clears the instrument in the current part, double click clears the pattern. SHIFT + CLEAR copies variation A to B.
- **PERFORM**:
  - instant mutes and solo
  - TR-6S style step loop
  - step entry
  - **pads** with 16th note repeat
  - **mute scenes** (steps 13–16)
  - fill on TAP
  - **live recording** (toggle, or hold SHIFT + TAP)

Patterns keep the 1.1.5 format; the new data lives in separate EEPROM areas.
