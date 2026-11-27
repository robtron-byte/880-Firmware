# 880 Perform – feature changes compared to 1.1.5

**880 Perform** is a performance firmware for the 880 Desktop (MK2), based on minisystem/880-firmware 1.1.5 (commit `bf810b4`). It is meant to be played live with one hand. This list covers the features that were removed and added. Build options and bugs found in 1.1.5 are described separately in [NOTES-FOR-UPSTREAM.md](NOTES-FOR-UPSTREAM.md).

Key names used below:
- **Instrument 1–12**: AC, BD … CH.
- **Step 1–16**: the step keys.
- **SHIFT**: WRITE/NEXT.
- **I/F-VAR held**: ALT.
- **BASIC-VAR**: BASIC-VARIATION.
- **Click**: press and release without touching another key.

## Removed

| Feature (1.1.5) | Replaced by |
|---|---|
| Song mode (PLAY/COMPOSE RHYTHM, INS/DEL, rhythm tracks) | Pattern chain (range) |
| 32nd sub-steps (2nd part with its own length) | 2nd part is always as long as the 1st part |
| Roll mode (SHIFT + step 9–14) | TR-6S style step loop and pad note repeat |
| AUD live hits | Live recording in PERFORM |
| Trigger inputs (TII) and assignable trigger outputs | TRIGGER OUT 1 = Low Tom, OUT 2 = Low Conga, fixed; mutes apply |
| Pre-scale selection (SHIFT + CLEAR) | Fixed to PRE-SCALE 3; the PRE-SCALE LEDs now count passes 1–4 |
| Shift lock (SHIFT + ACCENT) | – |
| Step fill (SHIFT + TAP), clear instrument with CLEAR + TAP | CLEAR click / double click (see below) |
| Pattern copy with CLEAR + step while stopped | Copy in the pattern layer |
| PATTERN CLEAR mode (clear all patterns, version display) | Single patterns are deleted in the pattern layer; DIN reset moved to EDIT |
| Modes loading patterns when switched (`previous_pattern`/`previous_bank`, MANUAL PLAY forcing pattern 12) | MODE never changes the running pattern |

## Added

### General
- **MODE** cycles PERFORM → 1st PART → 2nd PART → PERFORM.
  - The 2nd part is only included when it has content.
  - Pattern, timing and mutes keep running and nothing is loaded.
- **SHIFT + MODE** activates the 2nd part, or goes back from it to the 1st.
  - An empty 2nd part is filled with a copy of the 1st part.
  - If you leave it without changes, the copy is discarded again.
- **Autosave**: edits are saved at the end of the measure, in the background, without blocking the sequencer.
- **Reload** (SHIFT + TAP): resets the pattern to how it was when it was selected.
  - The backup is kept in the former song area of the EEPROM.
  - While running, the reload happens at the end of the measure.
- **TAP write is rounded to the nearest 16th** (shuffle included) and is evaluated right away.
  - It writes where the playhead is, in whichever part is playing.
  - If the step has already played, the sound is triggered immediately.

### Patterns and banks (EDIT and PERFORM)
- **BASIC-VAR click** toggles the **pattern layer**: step keys 1–12 select patterns. While running, the change happens at the end of the measure, and the pending pattern blinks. The BASIC-VAR LEDs show the layer (A = normal, B = pattern layer); the variation is shown while SHIFT is held.
- **BASIC-VAR held + step 1–12** selects the bank (one hand).
- **SHIFT + BASIC-VAR** cycles variation A/AB/B.
- **Pattern chain**: in the pattern layer, hold one pattern and press another. The range between them loops.
- **Copy a pattern**: SHIFT + source, then SHIFT + destination. Switching banks in between is allowed.
- **Delete a pattern**: CLEAR + the pattern key, pressed twice.
- **Playback direction** per pattern: I/F-VAR + step 13–16 for forward, reverse, ping-pong or random.
- In EDIT, the step of the fill pattern being edited (13–16) blinks.

### Odds and ratchets (EDIT)
- **Per step and instrument**:
  - Hold a set step + TAP: ratchet 1–4×.
  - Hold a set step + SHIFT: odds, i.e. play every 1st/2nd/3rd/4th pass.
  - Several held steps get the same value.
- Holding a step shows its odds and ratchet on the PRE-SCALE LEDs.
- Steps with odds or ratchets flicker once per second in the step row: 1 dip = odds, 2 = ratchet, 3 = both.
- Storage:
  - A sparse list of up to 31 entries per pattern, kept with the playback direction in a separate EEPROM area (64 B per pattern).
  - The pattern format is unchanged, so 1.1.5 ignores the extra data.

### EDIT
- **CLEAR click** clears the selected instrument in the edited part. **CLEAR double click** clears the whole pattern.
- **SHIFT + CLEAR** copies variation A to B, both parts, including odds and ratchets.
- DIN reset on/off: SHIFT + step 8 while stopped.

### PERFORM
- **I/F-VAR click** switches the step keys between two layers:
  - **A**: LOOP or STEPS (toggled with a SHIFT click).
  - **B**: pads and mute scenes.
- Instrument keys:
  - They **mute** immediately; key 1 clears all mutes.
  - They select the instrument only in layer A with STEPS.
  - CLEAR + instrument = solo.
  - I/F-VAR + key 4–6 plays the toms; SHIFT + key 4–8 plays congas, claves and maracas.
- **Step loop**, TR-6S style: hold a step and it repeats in 16ths. Several held steps play in order. Releasing continues seamlessly.
- **STEPS**: step keys set and clear steps of the selected instrument. Saved automatically.
- **Pads** (layer B):
  - Steps 2–12 play BD … CH. SHIFT + steps 4–8 play the alternate voices.
  - Holding a pad plays a **16th note repeat** in time.
  - Holding step 1 accents the hits.
  - Pads also play muted voices.
- **Mute scenes**: steps 13–16, in layer B or in the pattern layer.
  - Press: recall.
  - Hold an empty scene for 1 s: store the current mutes.
  - Hold a stored scene and change mutes: edit it.
  - CLEAR + scene: delete it.
  - In layer B, stored scenes are lit and empty ones flash every 2 s.
- **Fill**:
  - TAP plays the fill in the next measure.
  - SHIFT + step 13–16 selects the fill pattern, also while running.
  - While stopped, TAP toggles the intro.
- **Live recording**:
  - I/F-VAR + TAP toggles recording on and off.
  - Holding **SHIFT + TAP** records everything played while held: pads, rolls and instrument keys.
  - Hits are rounded to the nearest 16th.
  - SHIFT + TAP without playing anything is a reload.

### Display (mode LEDs)
- 1st/2nd PART = EDIT
- MANUAL PLAY = PERFORM
- PLAY = pattern layer
- COMPOSE = recording

## Storage and compatibility
- Patterns stay in the 1.1.5 format.
- New EEPROM areas:
  - Odds/ratchets/direction from 0xC800 (64 B per pattern).
  - Reload backup at 0xC100, in the former song area.
  - Mute scenes at 0xF840.
- The recall data stays at the 1.1.5 address.
- Going back to 1.1.5:
  - Songs are gone; their area holds the reload backup.
  - Patterns saved by 880 Perform use pre-scale 3 and have a 2nd part as long as the 1st.
