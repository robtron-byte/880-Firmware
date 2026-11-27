/*
 * ui.c
 * 880 Perform: key handling and LED rendering.
 *
 * Overview (see docs/BEDIENUNGSANLEITUNG.md):
 *   MODE                    EDIT <-> PERFORM (never changes the running pattern)
 *   I/F-VAR + MODE          EDIT: 1st <-> 2nd part (empty 2nd part is a copy of the 1st)
 *   SHIFT + MODE            PATTERN CLEAR (clear all: CLEAR held + TAP twice, version: SHIFT + TAP held)
 *   BASIC-VARIATION click   pattern layer on/off (steps 1-12 select the pattern)
 *   BASIC-VARIATION held    + step 1-12 selects the bank
 *   SHIFT + BASIC-VARIATION variation A -> AB -> B
 *   I/F-VAR + step 13-16    direction forward / reverse / ping-pong / random
 *   SHIFT + TAP             reload the pattern as it was when it was selected
 *   (pre-scale is fixed to PRE-SCALE 3; its LEDs count the passes 1-4)
 *   SHIFT + CLEAR           EDIT: copy variation A to B
 *   EDIT:    steps set/clear, held step + TAP = ratchet, held step + SHIFT = odds,
 *            TAP = tap write, CLEAR click = clear instrument, double click = clear pattern
 *   PERFORM: I/F-VAR click = instrument keys MUTE <-> SELECT, SHIFT click = step keys STEPS <-> LOOP,
 *            TAP = fill, I/F-VAR + TAP = record, I/F-VAR + inst 4-6 = toms, SHIFT + inst 4-8 = congas/CL/MA,
 *            pattern layer: hold + press = chain, 13-16 = mute scenes (SHIFT = store),
 *            SHIFT + source, SHIFT + target = copy, CLEAR + pattern twice = delete
 */
#include <avr/io.h>
#include <util/atomic.h>
#include <string.h>
#include "ui.h"
#include "hardware.h"
#include "sequencer.h"
#include "switches.h"
#include "leds.h"
#include "spi.h"
#include "drums.h"
#include "clock.h"
#include "mode.h"
#include "twi_eeprom.h"
#include "logic.h"

#define NONE 0xFF
#define CLICK_MAX_TICKS 60 //a press shorter than 600 ms (without other keys) is a click
#define DOUBLE_CLICK_TICKS 40 //CLEAR twice within 400 ms
#define CONFIRM_TICKS 200 //second press to confirm a delete within 2 s

volatile uint16_t ui_ticks = 0;
static uint16_t now_t; //ui_ticks read once (atomically) per ui_service

/* ---------- debounced key levels (sampled every 10 ms) ---------- */
static uint8_t deb[5], sample_prev[5], deb_prev[5];
static uint16_t last_sample_tick;

static uint8_t level(const uint8_t *bytes, uint8_t btn) {
	return (bytes[button[btn].spi_byte] >> button[btn].spi_bit) & 1;
}
static uint8_t raw_down(uint8_t btn) { return level(spi_previous_switch_data, btn); } //spi_previous_* holds the raw levels
static uint8_t deb_down(uint8_t btn) { return level(deb, btn); }
static uint8_t deb_released(uint8_t btn) { return level(deb_prev, btn) && !level(deb, btn); }
static uint16_t raw_steps(void) { return spi_previous_switch_data[4] | ((uint16_t)spi_previous_switch_data[3] << 8); }

static uint8_t press(uint8_t btn) { //consume a press event
	if (button[btn].state) {
		button[btn].state = 0;
		return 1;
	}
	return 0;
}

/* ---------- modifier keys: click detection ---------- */
struct modkey {
	uint8_t btn;
	uint8_t down:1;
	uint8_t used:1; //another key was involved: no click
	uint16_t t_down;
};
static struct modkey m_shift = {WRITE_SW}, m_alt = {IF_VAR_SW}, m_bvar = {BASIC_VAR_SW}, m_clear = {CLEAR_SW};
static struct modkey *const mods[4] = {&m_shift, &m_alt, &m_bvar, &m_clear};

/* ---------- UI state ---------- */
static uint8_t hs_step = NONE; //EDIT: first held step (shown on the PRE-SCALE LEDs)
static uint16_t hs_mask, hs_was_on, hs_seen; //all held steps, which of them were set before, debounced
static uint8_t hs_adjusted, hs_show_odds;
static uint16_t hs_t;

static void hs_reset(void) {
	hs_step = NONE;
	hs_mask = hs_was_on = hs_seen = 0;
}
static uint16_t last_clear_click = 0;
static uint8_t clear_flash = 0; //blink after a clear
static uint16_t clear_flash_t;
static uint8_t copy_src = NONE, copy_src_bank;
static uint8_t del_armed = NONE;
static uint16_t del_armed_t;
static uint16_t prev_loop_mask = 0;
static uint8_t tap_reload_armed, tap_fill; //PERFORM: SHIFT + TAP = reload on release unless something was recorded; fill set by this TAP press
static uint16_t tap_arm_t;
static uint8_t pad_inst[NUM_STEPS]; //PERFORM pads: voice of each pad (set on press, SHIFT = alternate voice)
/* scene keys 13-16 (PERFORM: pattern layer or I/F-VAR on B):
 * press = recall right away (an empty scene changes nothing). CLEAR + scene = delete it.
 * mutes changed while it is held = the mutes you hear are stored into it on release;
 * an empty scene held for 1 s stores the current mutes. No mutes at all = the scene is empty. */
static uint8_t scene_hold = NONE, scene_storing, scene_was_empty;
static uint16_t scene_hold_t, scene_base;
#define SCENE_HOLD_TICKS 100 //1 s

void ui_init(void) {
	//Timer4: 10 ms tick (16 MHz / 1024 / 156), CTC mode set up in setup_clock()
	OCR4A = 155;
	TIMSK4 |= (1<<OCIE4A);
	sequencer.inst_layer = INST_LAYER_MUTE;
	sequencer.step_layer = STEP_LAYER_LOOP;
	for (uint8_t k = 1; k < BASIC_RHYTHM; k++) pad_inst[k] = k - 1; //pads 2-12 = BD..CH
}

static uint8_t any_other_key_down(uint8_t self) {
	for (uint8_t i = 0; i < 5; i++) {
		uint8_t d = deb[i];
		if (i == button[self].spi_byte) d &= ~(1 << button[self].spi_bit);
		if (d) return 1;
	}
	return sequencer.TAP_HELD;
}

static void sample_keys(void) {
	uint16_t now = now_t;
	if (now == last_sample_tick) {
		memcpy(deb_prev, deb, 5); //no new sample: no edges
		return;
	}
	last_sample_tick = now;
	memcpy(deb_prev, deb, 5);
	for (uint8_t i = 0; i < 5; i++) {
		uint8_t s = spi_previous_switch_data[i];
		deb[i] = (s & sample_prev[i]) | (deb[i] & (s | sample_prev[i])); //2 sample debounce
		sample_prev[i] = s;
	}
}

/* any press event this refresh marks held modifiers as used */
static void track_modifiers(void) {
	uint8_t any_press = flag.tap;
	for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
		if (button[i].state) any_press = 1;
	}
	for (uint8_t i = 0; i < 4; i++) {
		struct modkey *m = mods[i];
		uint8_t d = deb_down(m->btn);
		if (d && !m->down) {
			m->down = 1;
			m->t_down = now_t;
			m->used = any_other_key_down(m->btn);
		} else if (d && any_press && !button[m->btn].state) {
			m->used = 1;
		}
	}
}

static uint8_t clicked(struct modkey *m) {
	if (m->down && deb_released(m->btn)) {
		m->down = 0;
		return !m->used && (uint16_t)(now_t - m->t_down) < CLICK_MAX_TICKS;
	}
	if (!deb_down(m->btn)) m->down = 0;
	return 0;
}

/* ---------- helpers ---------- */
static void flush_current(void);
static uint8_t perform(void) { return sequencer.mode == MANUAL_PLAY; }

static void mute_toggle(uint8_t d) {
	uint8_t m = !drum_hit[d].muted;
	drum_hit[d].muted = m;
	if (drum_hit[d].switch_bit != NO_SWITCH || d == CP) drum_hit[d + SW_DRUM_OFFSET].muted = m; //tom+conga, RS+CL, CP+MA
	for (uint8_t i = BD; i <= MA; i++) drum_hit[i].solo = 0;
}

static void unmute_all(void) {
	for (uint8_t i = BD; i <= AC; i++) {
		drum_hit[i].muted = 0;
		drum_hit[i].solo = 0;
	}
}

static void solo_toggle(uint8_t d) {
	if (drum_hit[d].solo) {
		unmute_all();
		return;
	}
	assign_solo(d);
	if (drum_hit[d].switch_bit != NO_SWITCH || d == CP) drum_hit[d + SW_DRUM_OFFSET].muted = 0;
}

static void select_accent(void) {
	sequencer.current_inst = AC;
	update_inst_led_mask();
}

static void mark_part_edited(uint8_t part) {
	flag.pattern_edit = 1;
	if (part == SECOND) sequencer.second_untouched = 0;
}

static void write_hit(uint8_t part, uint8_t step, uint8_t inst) {
	uint8_t v = sequencer.current_variation;
	if (inst == AC) {
		sequencer.pattern[v].accent[part] |= (1u << step);
	} else {
		sequencer.pattern[v].part[part][step] |= (1u << inst);
	}
	mark_part_edited(part);
	update_inst_led_mask();
}

/* where does a hit at this moment belong to? (rounded to the nearest 16th) */
static uint8_t locate_hit(uint8_t *part_out, uint8_t *step_out) {
	uint8_t pending = flag.next_step || flag.shuffle_step;
	uint8_t d = tap_decide(clock.ppqn_counter, clock.divider, pending);
	if (d == TAP_CURRENT_SOUNDED) {
		*part_out = sequencer.part_playing;
		*step_out = sequencer.play_step;
		return d;
	}
	//upcoming step
	uint8_t part = sequencer.part_playing;
	uint8_t last = sequencer.step_num[part];
	uint8_t pos, wrap;
	uint8_t pass = sequencer.pass_count;
	if (pending) {
		pos = sequencer.current_step;
		wrap = flag.new_measure || pos > last;
	} else {
		pos = sequencer.current_step + 1;
		wrap = pos > last;
	}
	if (wrap) {
		pos = 0;
		if (sequencer.step_num[SECOND] != NO_STEPS) {
			part ^= 1;
			if (part == FIRST) pass++;
		} else {
			pass++;
		}
		last = sequencer.step_num[part];
	}
	if (last > 15) last = 15;
	uint8_t step;
	if (sequencer.loop_mask) {
		step = loop_pick(sequencer.loop_mask, sequencer.loop_idx);
		if (step > last) step = last;
	} else {
		uint8_t dir = attr_direction(&pattern_attrs);
		step = dir_map(dir, pos, last, pass, lfsr_next(sequencer.rnd ? sequencer.rnd : 0x5A));
	}
	*part_out = part;
	*step_out = step;
	return d;
}

/* live hit in PERFORM (toms/congas, recording) */
/* PERFORM instrument keys select only where it is needed: I/F-VAR on A with the step keys on STEPS SETZEN.
 * Everywhere else (A + LOOP, B = pads) they mute. */
static uint8_t inst_keys_select(void) {
	return sequencer.inst_layer == INST_LAYER_MUTE && sequencer.step_layer == STEP_LAYER_STEPS && !sequencer.pattern_layer;
}

/* PERFORM: SHIFT + TAP held while running = everything played is recorded */
static uint8_t rec_hold(void) {
	return perform() && sequencer.START && sequencer.SHIFT && sequencer.TAP_HELD;
}

static void live_hit(uint8_t inst) {
	if (inst != AC) trigger_drum(inst, 0);
	if ((sequencer.recording || rec_hold()) && sequencer.START) {
		tap_reload_armed = 0; //recorded something: SHIFT + TAP is a recording, no reload
		uint8_t part, step;
		uint8_t d = locate_hit(&part, &step);
		write_hit(part, step, inst);
		if (d != TAP_CURRENT_SOUNDED && inst != AC) sequencer.skip_mask |= (1u << inst); //already heard now
	}
}

/* ---------- PERFORM, I/F-VAR on B: step keys 1-12 are pads, 13-16 mute scenes ----------
 * pad 1 = accent (held: repeats are accented), pads 2-12 = BD..CH, SHIFT + pad 4-8 = LC/MC/HC/CL/MA.
 * holding a pad plays the voice on every 16th in time (note repeat). */
static uint8_t pads_active(void) {
	return perform() && sequencer.inst_layer == INST_LAYER_SELECT && !sequencer.pattern_layer;
}

static void pad_press(uint8_t k) {
	if (rec_hold()) tap_reload_armed = 0; //recording: SHIFT + TAP is no reload
	if (k == 0) return; //accent pad: only while held
	uint8_t d = k - 1;
	if (sequencer.SHIFT && !sequencer.TAP_HELD && k >= 3 && k <= 7) { //SHIFT + TAP = recording: no voice switch
		m_shift.used = 1;
		d += SW_DRUM_OFFSET;
	}
	pad_inst[k] = d;
	uint8_t vel = ((deb[4] | spi_previous_switch_data[4]) & 1) ? 127 : 0; //pad 1 held: accent
	if (!sequencer.START) {
		trigger_drum(d, vel);
		return;
	}
	uint8_t part, step;
	if (locate_hit(&part, &step) == TAP_CURRENT_SOUNDED) {
		trigger_drum(d, vel); //the step just played: sound now, repeats from the next step
		if (rec_hold() && step <= sequencer.step_num[part]) { //recording: onto that step
			write_hit(part, step, d);
			if (vel) write_hit(part, step, AC);
		}
	} else {
		pad_once_mask |= (1u << d); //belongs to the next step: plays there even if the pad is released before
	}
}

static void do_reload(void);
/* held pads -> voices repeated on every step */
static void update_pads(void) {
	uint16_t mask = 0;
	uint8_t acc = 0;
	pad_record = pads_active() && rec_hold();
	if (rec_hold() && tap_fill) { //TAP pressed before SHIFT: it is a recording, not a fill
		flag.fill = 0;
		tap_fill = 0;
	}
	if (!sequencer.TAP_HELD) {
		tap_fill = 0;
		//released without recording: reload (50 ms after the press at the earliest, so a bouncing contact is no release)
		if (tap_reload_armed && (uint16_t)(now_t - tap_arm_t) >= 5) {
			tap_reload_armed = 0;
			do_reload();
		}
	}
	if (pads_active() && sequencer.START) {
		uint16_t held = deb[4] | ((uint16_t)deb[3] << 8);
		for (uint8_t k = 1; k < BASIC_RHYTHM; k++) {
			if ((held >> k) & 1) mask |= (1u << pad_inst[k]);
		}
		acc = held & 1;
	}
	pad_repeat_mask = mask;
	pad_accent = acc;
	if (!sequencer.START) pad_once_mask = 0; //no late pad hit after a stop
}

/* TAP in EDIT: write the selected instrument to the nearest step where the playhead is
 * (in the part that is playing, whether the 1st or the 2nd part is being edited) */
static void tap_write(void) {
	uint8_t part, step;
	uint8_t d = locate_hit(&part, &step);
	if (sequencer.step_num[part] > 15 || step > sequencer.step_num[part]) return;
	write_hit(part, step, sequencer.current_inst);
	if (d == TAP_CURRENT_SOUNDED && sequencer.current_inst != AC) {
		trigger_drum(sequencer.current_inst, 0); //that step already played: let it sound now
	}
}

static void do_reload(void) {
	if (fill_active || !storage_backup_for(sequencer.current_pattern, sequencer.pattern_bank)) return; //nothing to restore
	if (sequencer.START) {
		reload_pending = 1;
	} else if (storage_reload()) {
		clock.divider = pre_scale[sequencer.pre_scale];
		flag.pre_scale_change = 0;
		start_write_current_pattern(sequencer.current_pattern, sequencer.pattern_bank);
		attr_queue_current();
		update_inst_led_mask();
	}
	clear_flash = 1;
	clear_flash_t = now_t;
}

/* ---------- key handlers ---------- */

/* 2nd part has content (exists and is not just about to be removed) */
static uint8_t second_in_use(void) {
	return sequencer.step_num[SECOND] != NO_STEPS && !flag.second_clear;
}

/* MODE: PERFORM -> 1st PART -> 2nd PART (only if it has content) -> PERFORM
 * SHIFT + MODE: 2nd PART on (an empty one is filled with the 1st part); in the 2nd PART: back to the 1st */
static void handle_mode_key(void) {
	if (!press(MODE_SW)) return;
	if (sequencer.ALT) m_alt.used = 1; //no I/F-VAR click on release
	if (sequencer.SHIFT) {
		m_shift.used = 1;
		set_mode(sequencer.mode == SECOND_PART ? FIRST_PART : SECOND_PART);
	} else if (sequencer.mode == FIRST_PART && second_in_use()) {
		set_mode(SECOND_PART);
	} else if (IS_EDIT(sequencer.mode)) {
		set_mode(MANUAL_PLAY);
	} else {
		set_mode(FIRST_PART);
	}
	hs_reset();
}

/* held step(s) + TAP (ratchet) / SHIFT (odds): all held steps that are set get the new value */
static void cycle_attr(uint8_t ratchet) {
	uint8_t part = sequencer.part_editing, v = sequencer.current_variation;
	if (sequencer.current_inst == AC || !hs_mask) return;
	uint16_t set = 0;
	for (uint8_t k = 0; k < NUM_STEPS; k++) {
		if ((hs_mask >> k) & 1 && (sequencer.pattern[v].part[part][k] >> sequencer.current_inst) & 1) set |= (1u << k);
	}
	if (!set) return;
	uint8_t ref = (hs_step != NONE && ((set >> hs_step) & 1)) ? hs_step : loop_pick(set, 0);
	uint16_t key = attr_key(part, v, ref, sequencer.current_inst);
	uint8_t new_odds = attr_get_odds(&pattern_attrs, key) % 4 + 1;
	uint8_t new_rat = attr_get_ratchet(&pattern_attrs, key) % 4 + 1;
	uint8_t need = 0; //new list entries: steps without odds/ratchet yet
	uint16_t marked = attr_inst_steps(&pattern_attrs, part, v, sequencer.current_inst);
	if ((ratchet ? new_rat : new_odds) > 1) need = popcount16(set & ~marked);
	if (attr_count(&pattern_attrs) + need > ATTR_MAX) { //list full: change nothing, flash
		clear_flash = 1;
		clear_flash_t = now_t;
		return;
	}
	uint8_t ok = 1;
	for (uint8_t k = 0; k < NUM_STEPS; k++) {
		if (!((set >> k) & 1)) continue;
		key = attr_key(part, v, k, sequencer.current_inst);
		uint8_t odds = attr_get_odds(&pattern_attrs, key);
		uint8_t rat = attr_get_ratchet(&pattern_attrs, key);
		if (ratchet) rat = new_rat; else odds = new_odds;
		if (!attr_set(&pattern_attrs, key, odds, rat)) ok = 0;
	}
	mark_part_edited(part);
	if (!ok) { //list full: flash
		clear_flash = 1;
		clear_flash_t = now_t;
	}
	hs_step = ref;
	hs_adjusted = 1;
	hs_show_odds = !ratchet;
}

static void handle_tap(void) {
	if (!flag.tap) return;
	flag.tap = 0;
	if (IS_EDIT(sequencer.mode) && hs_mask) { //held step(s) + TAP: ratchet
		cycle_attr(1);
		return;
	}
	if (sequencer.SHIFT && perform()) { //PERFORM: reload when TAP is released without playing anything
		m_shift.used = 1;
		tap_reload_armed = 1;
		tap_arm_t = now_t;
	} else if (sequencer.SHIFT) {
		m_shift.used = 1;
		do_reload();
	} else if (sequencer.ALT && perform()) {
		m_alt.used = 1;
		sequencer.recording ^= 1;
	} else if (IS_EDIT(sequencer.mode)) {
		if (sequencer.CLEAR) {
			m_clear.used = 1;
		} else if (sequencer.START) {
			tap_write();
		}
	} else if (perform()) {
		if (sequencer.START) {
			if (!flag.fill) tap_fill = 1; //this press asked for the fill (a recording may take it back)
			flag.fill = 1; //fill in the next measure
		} else { //stopped: toggle intro
			if (!flag.intro) {
				flush_current();
				fill_from_pattern = sequencer.current_pattern;
				fill_from_bank = sequencer.pattern_bank;
				read_next_pattern(sequencer.current_intro_fill, sequencer.pattern_bank);
				sequencer.new_pattern = fill_from_pattern;
				sequencer.new_bank = fill_from_bank;
				sequencer.current_pattern = sequencer.current_intro_fill;
				fill_active = 1;
				flag.intro = 1;
			} else {
				pattern_load(fill_from_pattern, fill_from_bank); //back to the basic rhythm (clears the intro)
			}
		}
	}
}

static void handle_variation_key(void) {
	if (!press(BASIC_VAR_SW)) return;
	if (sequencer.SHIFT) { //variation A -> AB -> B -> A
		m_shift.used = 1;
		m_bvar.used = 1;
		switch (sequencer.variation_mode) {
			case VAR_A: sequencer.variation_mode = VAR_AB; break;
			case VAR_AB: sequencer.variation_mode = VAR_B; break;
			default: sequencer.variation_mode = VAR_A; break;
		}
		if (sequencer.START) {
			flag.variation_change = 1;
		} else {
			sequencer.current_variation = (sequencer.variation_mode == VAR_B) ? VAR_B : VAR_A;
		}
		update_inst_led_mask();
	} else if (sequencer.ALT) { //intro/fill variation
		m_alt.used = 1;
		m_bvar.used = 1;
		sequencer.intro_fill_var ^= 1;
	}
}

static void handle_inst_keys(void) {
	for (uint8_t i = INST_AC_1_SW; i <= INST_CH_12_SW; i++) {
		if (!press(i)) continue;
		uint8_t k = i - INST_AC_1_SW; //0 = AC (key 1) ... 11 = CH (key 12)
		uint8_t d = k - 1; //drum index for k >= 1
		if (perform()) {
			if (sequencer.ALT && k >= 3 && k <= 5) { //keys 4-6: toms
				m_alt.used = 1;
				live_hit(d);
			} else if (sequencer.SHIFT && k >= 3 && k <= 7) { //keys 4-8: congas, claves, maracas
				m_shift.used = 1;
				live_hit(d + SW_DRUM_OFFSET);
			} else if (sequencer.CLEAR && k >= 1) {
				m_clear.used = 1;
				solo_toggle(d);
			} else if ((sequencer.recording || rec_hold()) && sequencer.START && scene_hold == NONE) {
				live_hit(k == 0 ? AC : d);
			} else if (!inst_keys_select() || scene_hold != NONE) { //scene held: always mute keys
				if (k == 0) {
					unmute_all();
				} else {
					mute_toggle(d);
				}
			} else {
				if (k == 0) {
					select_accent();
				} else {
					select_instrument(d);
				}
			}
		} else {
			if (sequencer.SHIFT && k >= 1) {
				m_shift.used = 1;
				mute_toggle(d);
			} else if (k == 0) {
				if (sequencer.CLEAR) {
					m_clear.used = 1;
					unmute_all(); //MUTE CLEAR
				} else {
					select_accent();
				}
			} else {
				if (sequencer.CLEAR) m_clear.used = 1;
				select_instrument(d); //handles CLEAR + instrument = solo
			}
		}
	}
}

static uint16_t mute_mask(void) {
	uint16_t mask = 0;
	for (uint8_t i = BD; i <= MA; i++) {
		if (drum_hit[i].muted) mask |= (1u << i);
	}
	return mask;
}

static void apply_mutes(uint16_t mask) {
	for (uint8_t i = BD; i <= MA; i++) {
		drum_hit[i].muted = (mask >> i) & 1;
		drum_hit[i].solo = 0;
	}
}

static void save_scene(uint8_t n) {
	uint16_t mask = mute_mask() & 0x7FFF; //MA is muted together with CP: bit 15 is derived on recall, so 0xFFFF stays "empty"
	mute_scene[n] = mask ? mask : 0xFFFF; //no mutes at all: the scene is empty again
	scene_write(n);
	clear_flash = 1;
	clear_flash_t = now_t;
}


static void recall_scene(uint8_t n);
/* scene key released: store or recall */
static void scene_finish(void) {
	if (scene_storing) save_scene(scene_hold - BASIC_RHYTHM);
	scene_hold = NONE;
}

static void scene_service(void) {
	if (scene_hold == NONE) return;
	uint16_t steps = deb[4] | ((uint16_t)deb[3] << 8);
	uint8_t held = ((steps >> scene_hold) & 1) || ((raw_steps() >> scene_hold) & 1);
	if (held) {
		if (!scene_storing && (mute_mask() != scene_base
			|| (scene_was_empty && (uint16_t)(now_t - scene_hold_t) >= SCENE_HOLD_TICKS))) {
			scene_storing = 1; //store on release (the step blinks fast)
		}
		return;
	}
	scene_finish();
}

static void recall_scene(uint8_t n);
static void scene_press(uint8_t k) {
	if (scene_hold != NONE && ((raw_steps() >> scene_hold) & 1)) return; //one scene at a time (only while it is really held)
	if (scene_hold != NONE) scene_finish(); //released in this tick: finish it first
	uint8_t n = k - BASIC_RHYTHM;
	if (sequencer.CLEAR) { //CLEAR + scene: delete it
		m_clear.used = 1;
		mute_scene[n] = 0xFFFF;
		scene_write(n);
		clear_flash = 1;
		clear_flash_t = now_t;
		return;
	}
	scene_hold = k;
	scene_hold_t = now_t;
	scene_storing = 0;
	scene_was_empty = mute_scene[n] == 0xFFFF;
	recall_scene(n); //recall right away (an empty scene changes nothing)
	scene_base = mute_mask();
}

static void recall_scene(uint8_t n) {
	uint16_t mask = mute_scene[n];
	if (mask == 0xFFFF) return; //never stored
	mask = (mask & 0x7FFF) | (((mask >> CP) & 1) << MA); //MA follows CP
	apply_mutes(mask);
}

/* make sure the stored version of the current pattern is up to date before it is copied */
static void flush_current(void) {
	if (fill_no_save) { //RAM holds overwritten fill content: never store it
		flag.pattern_edit = 0;
		return;
	}
	if (flag.pattern_edit) {
		flag.pattern_edit = 0;
		start_write_current_pattern(sequencer.current_pattern, sequencer.pattern_bank);
		attr_queue_current();
	}
}

static void pattern_layer_step(uint8_t k) {
	uint8_t bank = sequencer.new_bank;
	if (k < BASIC_RHYTHM) {
		if (perform() && sequencer.CLEAR) { //delete (press twice)
			m_clear.used = 1;
			if (del_armed == k && (uint16_t)(now_t - del_armed_t) < CONFIRM_TICKS) {
				del_armed = NONE;
				if (k == sequencer.current_pattern && bank == sequencer.pattern_bank && fill_active) {
					//playing fill/intro slot: clear it in the EEPROM, the old RAM content is never stored
					flag.pattern_edit = 0;
					storage_queue_clear(k, bank);
					if (sequencer.START) {
						fill_no_save = 1;
					} else {
						read_next_pattern(k, bank);
						if (sequencer.mode == SECOND_PART) second_part_autocopy();
					}
				} else if (k == sequencer.current_pattern && bank == sequencer.pattern_bank) {
					clear_current_pattern();
				} else {
					storage_queue_clear(k, bank);
				}
				clear_flash = 1;
				clear_flash_t = now_t;
			} else {
				del_armed = k;
				del_armed_t = now_t;
			}
		} else if (perform() && sequencer.SHIFT) { //copy: source, then target
			m_shift.used = 1;
			if (copy_src == NONE) {
				copy_src = k;
				copy_src_bank = bank;
			} else if (copy_src == k && copy_src_bank == bank) {
				copy_src = NONE;
			} else {
				if (copy_src == sequencer.current_pattern && copy_src_bank == sequencer.pattern_bank) flush_current();
				if (storage_queue_copy(copy_src, copy_src_bank, k, bank)) {
					if (k == sequencer.current_pattern && bank == sequencer.pattern_bank && fill_active) {
						//the playing fill/intro slot was overwritten: keep it a fill, never write the old content back
						flag.pattern_edit = 0;
						flag.length_change = 0;
						flag.second_clear = 0;
						if (sequencer.START) {
							fill_no_save = 1;
						} else {
							read_next_pattern(k, bank); //stopped intro: show the copied content, intro stays on
							if (sequencer.mode == SECOND_PART) second_part_autocopy();
						}
					} else if (k == sequencer.current_pattern && bank == sequencer.pattern_bank) {
						//the playing pattern was overwritten: take the new content, drop unsaved edits
						flag.pattern_edit = 0;
						flag.length_change = 0;
						flag.second_clear = 0;
						if (sequencer.START) {
							reload_current = 1; //loaded at the next measure end, edits until then are dropped
						} else {
							pattern_load(k, bank);
						}
					}
					clear_flash = 1;
					clear_flash_t = now_t;
				}
				copy_src = NONE;
			}
		} else {
			uint16_t held = raw_steps() & 0x0FFF & ~(1u << k);
			if (perform() && held) { //hold one pattern + press another: chain
				uint8_t start = loop_pick(held, 0);
				sequencer.chain_start = start;
				sequencer.chain_end = k;
				sequencer.chain_active = 1;
			} else {
				sequencer.chain_active = 0;
				pattern_request(k, bank);
			}
		}
	} else {
		if (perform() && sequencer.SHIFT) { //SHIFT + 13-16: fill-in for TAP
			m_shift.used = 1;
			sequencer.current_intro_fill = k;
		} else if (perform()) { //mute scenes: click = recall, hold = store (see scene_service)
			scene_press(k);
		} else { //EDIT: intro/fill patterns 13-16 can be selected for editing
			sequencer.current_intro_fill = k;
			sequencer.chain_active = 0;
			pattern_request(k, bank);
		}
	}
}

static void edit_step_press(uint8_t k) {
	uint8_t part = sequencer.part_editing, v = sequencer.current_variation;
	if (sequencer.step_num[part] > 15 || k > sequencer.step_num[part]) return;
	uint8_t on;
	if (sequencer.current_inst == AC) {
		on = (sequencer.pattern[v].accent[part] >> k) & 1;
	} else {
		on = (sequencer.pattern[v].part[part][k] >> sequencer.current_inst) & 1;
	}
	if (!hs_mask) { //new hold: start fresh
		hs_adjusted = 0;
		hs_show_odds = 0;
		hs_step = k;
	}
	uint16_t bit = 1u << k;
	hs_mask |= bit;
	hs_seen &= ~bit;
	if (on) hs_was_on |= bit; else hs_was_on &= ~bit;
	hs_t = now_t;
	if (!on) write_hit(part, k, sequencer.current_inst);
}

static void edit_step_release(void) {
	if (!hs_mask) return;
	uint16_t steps_deb = deb[4] | ((uint16_t)deb[3] << 8);
	uint16_t steps_raw = raw_steps();
	uint8_t part = sequencer.part_editing, v = sequencer.current_variation;
	for (uint8_t k = 0; k < NUM_STEPS; k++) {
		uint16_t bit = 1u << k;
		if (!(hs_mask & bit)) continue;
		if (steps_deb & bit) { //still held
			hs_seen |= bit;
			continue;
		}
		//not released yet if the debounced level has not even seen the press (very short tap: allow 50 ms)
		if (!(hs_seen & bit) && ((uint16_t)(now_t - hs_t) < 5 || (steps_raw & bit))) continue;
		hs_mask &= ~bit;
		if ((hs_was_on & bit) && !hs_adjusted) { //a set step pressed and released without TAP/SHIFT: clear it
			if (sequencer.current_inst == AC) {
				sequencer.pattern[v].accent[part] &= ~bit;
			} else {
				sequencer.pattern[v].part[part][k] &= ~(1u << sequencer.current_inst);
				attr_set(&pattern_attrs, attr_key(part, v, k, sequencer.current_inst), 1, 1);
			}
			mark_part_edited(part);
			update_inst_led_mask();
		}
		if (hs_step == k) hs_step = hs_mask ? loop_pick(hs_mask, 0) : NONE;
	}
	if (!hs_mask) hs_reset();
}

/* PERFORM: step keys set steps in the STEPS layer (I/F-VAR on A) */
static uint8_t perform_steps(void) {
	return sequencer.step_layer == STEP_LAYER_STEPS;
}

static void handle_step_keys(void) {
	for (uint8_t k = 0; k < NUM_STEPS; k++) {
		if (!press(k)) continue;
		if (raw_down(BASIC_VAR_SW)) { //bank
			m_bvar.used = 1;
			if (k < NUM_BANKS) pattern_request(sequencer.new_pattern, k);
			continue;
		}
		if (sequencer.ALT) {
			m_alt.used = 1;
			if (k >= BASIC_RHYTHM) { //direction
				attr_set_direction(&pattern_attrs, k - BASIC_RHYTHM);
				flag.pattern_edit = 1;
			} else if (sequencer.SHIFT && !sequencer.START && IS_EDIT(sequencer.mode)) { //MIDI channel
				m_shift.used = 1;
				sequencer.midi_channel = k;
				recall_dirty = 1;
			}
			continue;
		}
		if (sequencer.pattern_layer) {
			pattern_layer_step(k);
			continue;
		}
		if (IS_EDIT(sequencer.mode)) {
			if (sequencer.START) {
				if (sequencer.SHIFT) {
					m_shift.used = 1;
					update_shuffle(k);
				} else if (sequencer.CLEAR) {
					m_clear.used = 1;
					if (sequencer.part_editing == SECOND) {
						if (k == 0) flag.second_clear = 1; //CLEAR + step 1 in the 2nd part removes it
					} else {
						sequencer.step_num_new = k;
						if (k != sequencer.step_num[FIRST]) {
							flag.length_change = 1;
							flag.pattern_edit = 1;
						}
					}
				} else {
					edit_step_press(k);
				}
			} else {
				if (sequencer.SHIFT) {
					m_shift.used = 1;
					if (k == DIN_RESET_STEP) { //stopped: SHIFT + step 8 = DIN reset on/off
						sequencer.din_reset_enable ^= 1;
						recall_dirty = 1;
					} else if (sequencer.clock_mode == DIN_SYNC_SLAVE && k < NUM_SYNC_COUNTS) {
						clock.sync_count_index = k;
					}
				} else if (sequencer.CLEAR) {
					m_clear.used = 1;
					if (sequencer.part_editing == SECOND) {
						if (k == 0) flag.second_clear = 1;
						apply_length_changes();
					} else {
						sequencer.step_num_new = k;
						flag.length_change = 1;
						apply_length_changes();
						flag.pattern_edit = 1;
					}
				} else {
					if (k >= BASIC_RHYTHM) sequencer.current_intro_fill = k;
					pattern_request(k, sequencer.new_bank);
				}
			}
		} else if (pads_active()) { //PERFORM, I/F-VAR on B: pads and mute scenes
			if (k < BASIC_RHYTHM) {
				pad_press(k);
			} else if (sequencer.SHIFT) { //SHIFT + 13-16: fill-in for TAP
				m_shift.used = 1;
				sequencer.current_intro_fill = k;
			} else {
				scene_press(k);
			}
		} else { //PERFORM
			if (sequencer.SHIFT) {
				m_shift.used = 1;
				update_shuffle(k);
			} else if (!sequencer.START) {
				if (k < BASIC_RHYTHM) {
					pattern_request(k, sequencer.new_bank);
				} else {
					sequencer.current_intro_fill = k;
				}
			} else if (perform_steps()) {
				uint8_t part = sequencer.part_playing, v = sequencer.current_variation;
				if (k <= sequencer.step_num[part]) {
					if (sequencer.current_inst == AC) {
						sequencer.pattern[v].accent[part] ^= (1u << k);
					} else {
						sequencer.pattern[v].part[part][k] ^= (1u << sequencer.current_inst);
						if (!((sequencer.pattern[v].part[part][k] >> sequencer.current_inst) & 1)) {
							attr_set(&pattern_attrs, attr_key(part, v, k, sequencer.current_inst), 1, 1);
						}
					}
					mark_part_edited(part);
				}
			}
			//LOOP layer: handled by the held step levels
		}
	}
}

/* SHIFT + CLEAR (EDIT): copy variation A to B */
static void handle_shift_clear(void) {
	if (!button[CLEAR_SW].state || !sequencer.SHIFT) return;
	button[CLEAR_SW].state = 0;
	m_shift.used = 1;
	m_clear.used = 1;
	if (!IS_EDIT(sequencer.mode)) return;
	copy_variation_a_to_b();
	clear_flash = 1;
	clear_flash_t = now_t;
}

static void handle_clicks(void) {
	if (clicked(&m_shift)) {
		if (perform() && !sequencer.pattern_layer) {
			sequencer.step_layer ^= 1;
			sequencer.loop_mask = 0;
		}
	}
	if (clicked(&m_alt)) {
		if (perform()) sequencer.inst_layer ^= 1;
	}
	if (clicked(&m_bvar)) {
		sequencer.pattern_layer ^= 1;
		copy_src = NONE;
		del_armed = NONE;
	}
	if (clicked(&m_clear)) {
		if (IS_EDIT(sequencer.mode)) {
			if ((uint16_t)(now_t - last_clear_click) < DOUBLE_CLICK_TICKS) {
				clear_current_pattern();
				last_clear_click = now_t - DOUBLE_CLICK_TICKS; //no triple click
			} else {
				clear_current_instrument();
				last_clear_click = now_t;
			}
			clear_flash = 1;
			clear_flash_t = now_t;
		}
	}
	if (del_armed != NONE && (uint16_t)(now_t - del_armed_t) >= CONFIRM_TICKS) del_armed = NONE;
}

static void update_loop(void) {
	uint16_t mask = 0;
	if (perform() && sequencer.START && !perform_steps() && !pads_active() && !sequencer.pattern_layer
		&& !raw_down(BASIC_VAR_SW) && !sequencer.ALT && !sequencer.SHIFT) {
		mask = deb[4] | ((uint16_t)deb[3] << 8);
	}
	if (mask && !prev_loop_mask) sequencer.loop_idx = 0;
	prev_loop_mask = mask;
	sequencer.loop_mask = mask;
}

static void service_storage(void) {
	if (!sequencer.START && flag.pattern_edit && storage_idle()) { //edits while stopped are stored right away
		flag.pattern_edit = 0;
		start_write_current_pattern(sequencer.current_pattern, sequencer.pattern_bank);
		attr_queue_current();
	}
	storage_service();
	if (recall_dirty && !sequencer.START && storage_idle()) {
		recall_dirty = 0;
		eeprom_write_recall_data();
	}
}

/* ---------- LEDs ---------- */

static uint16_t inst_mask_for(uint8_t part) {
	uint16_t m = 0;
	uint8_t v = sequencer.current_variation;
	uint8_t last = sequencer.step_num[part];
	if (last > 15) last = 15;
	for (uint8_t i = 0; i <= last; i++) {
		uint8_t on = (sequencer.current_inst == AC) ? ((sequencer.pattern[v].accent[part] >> i) & 1)
			: ((sequencer.pattern[v].part[part][i] >> sequencer.current_inst) & 1);
		if (on) m |= (1u << i);
	}
	return m;
}


/* instrument steps; steps with odds/ratchets flicker once per second (fixed rhythm, not the tempo):
 *   odds only: 1 short dip, ratchet only: 2 dips, odds and ratchet: 3 dips (80 ms each, 80 ms apart),
 * the rest of the second the step is lit, so the marks stay calm while the pattern runs. */
static uint16_t marked_mask_for(uint8_t part) {
	uint16_t m = inst_mask_for(part);
	if (sequencer.current_inst == AC) return m;
	uint8_t p = now_t % 100;
	if (p >= 40 || (p % 16) >= 8) return m; //outside the dips
	uint16_t odds_m, rat_m;
	attr_inst_masks(&pattern_attrs, part, sequencer.current_variation, sequencer.current_inst, &odds_m, &rat_m);
	uint16_t both = odds_m & rat_m;
	uint16_t dark;
	if (p < 8) dark = odds_m | rat_m;          //1st dip: everything marked
	else if (p < 24) dark = rat_m;             //2nd dip: ratchet (alone or with odds)
	else dark = both;                          //3rd dip: odds and ratchet
	return m & ~dark;
}

static void render_steps(uint8_t blink, uint8_t fast) {
	uint16_t m = 0;
	uint8_t run_on = sequencer.START && (clock.ppqn_counter < (clock.divider >> 1));
	uint16_t run = run_on ? (1u << sequencer.play_step) : 0;
	if (raw_down(BASIC_VAR_SW)) {
		m = 1u << sequencer.new_bank;
	} else if (sequencer.ALT && sequencer.SHIFT && !sequencer.START && IS_EDIT(sequencer.mode)) {
		m = 1u << sequencer.midi_channel;
	} else if (sequencer.ALT) {
		m = 1u << (BASIC_RHYTHM + attr_direction(&pattern_attrs));
	} else if (hs_step != NONE) {
		m = inst_mask_for(sequencer.part_editing);
		if (fast) m ^= hs_mask; //all held steps blink
	} else if (sequencer.SHIFT && !(sequencer.pattern_layer && perform()) && !pads_active()) {
		if (!sequencer.START && sequencer.clock_mode == DIN_SYNC_SLAVE) {
			m = 1u << clock.sync_count_index;
		} else {
			m = 1u << sequencer.new_shuffle_amount;
		}
		if (!sequencer.START && IS_EDIT(sequencer.mode) && sequencer.din_reset_enable) m |= 1u << DIN_RESET_STEP;
	} else if (pads_active()) {
		//pads: held pads and voices that just played; scenes: stored = on, empty = short flash every 2 s
		m = (deb[4] | ((uint16_t)deb[3] << 8)) & 0x0FFF;
		m |= (inst_flash_mask & 0x07FF) << 1; //BD..CH -> pads 2-12
		m |= ((inst_flash_mask >> SW_DRUM_OFFSET) & 0x7C) << 1; //LC..MA -> pads 4-8
		if (sequencer.SHIFT) { //SHIFT + 13-16 chooses the fill: show it
			m |= 1u << sequencer.current_intro_fill;
		} else {
			for (uint8_t n = 0; n < 4; n++) {
				if (mute_scene[n] != 0xFFFF || (now_t % 200) < 8) m |= 1u << (BASIC_RHYTHM + n);
			}
		}
		if (scene_hold != NONE && scene_storing) {
			if (fast) m |= (1u << scene_hold); else m &= ~(1u << scene_hold);
		}
	} else if (sequencer.pattern_layer || !sequencer.START) {
		//pattern display
		if (sequencer.chain_active && perform()) {
			uint8_t lo = sequencer.chain_start < sequencer.chain_end ? sequencer.chain_start : sequencer.chain_end;
			uint8_t hi = sequencer.chain_start < sequencer.chain_end ? sequencer.chain_end : sequencer.chain_start;
			for (uint8_t i = lo; i <= hi; i++) m |= (1u << i);
			if (!blink) m &= ~(1u << sequencer.current_pattern);
		} else {
			m = 1u << sequencer.current_pattern;
			if (sequencer.new_pattern != sequencer.current_pattern || sequencer.new_bank != sequencer.pattern_bank) {
				if (blink) m |= 1u << sequencer.new_pattern; //pending pattern blinks
			}
		}
		if (!sequencer.START && !sequencer.pattern_layer && !blink && IS_EDIT(sequencer.mode)) m &= ~(1u << sequencer.current_pattern);
		if (perform() && (!sequencer.pattern_layer || sequencer.SHIFT)) m |= (1u << sequencer.current_intro_fill); //pattern layer: SHIFT shows the fill
		if (!perform() && sequencer.pattern_layer) m |= (1u << sequencer.current_intro_fill);
		if (copy_src != NONE && copy_src_bank == sequencer.new_bank) {
			if (fast) m |= (1u << copy_src); else m &= ~(1u << copy_src);
		}
		if (del_armed != NONE) {
			if (fast) m |= (1u << del_armed); else m &= ~(1u << del_armed);
		}
		if (scene_hold != NONE && scene_storing) {
			if (fast) m |= (1u << scene_hold); else m &= ~(1u << scene_hold);
		}
	} else if (IS_EDIT(sequencer.mode)) {
		m = marked_mask_for(sequencer.part_editing);
		if (sequencer.part_playing == sequencer.part_editing) m ^= run;
		if (sequencer.current_pattern >= BASIC_RHYTHM && blink) m ^= (1u << sequencer.current_pattern); //editing fill 13-16: its step blinks
	} else if (perform_steps()) {
		m = marked_mask_for(sequencer.part_playing) ^ run;
	} else {
		m = run | sequencer.loop_mask;
	}
	if (clear_flash && fast) m = 0;
	spi_data[LATCH_1] = m;
	spi_data[LATCH_0] = m >> 8;
}

static void render_insts(void) {
	turn_off_all_inst_leds();
	turn_off(LC_LED); turn_off(MC_LED); turn_off(HC_LED); turn_off(CL_LED); turn_off(MA_LED);
	uint8_t show_mutes_now = (perform() && ((!inst_keys_select() && !sequencer.recording) || scene_hold != NONE)) || (!perform() && sequencer.SHIFT);
	if (show_mutes_now) {
		for (uint8_t i = BD; i <= MA; i++) {
			if (drum_hit[i].muted) turn_on(drum_hit[i].led_index);
		}
		return;
	}
	if (!(perform() && sequencer.recording)) {
		if (sequencer.current_inst == AC) {
			turn_on(ACCENT_1_LED);
		} else {
			turn_on(drum_hit[sequencer.current_inst].led_index);
		}
	}
	for (uint8_t i = BD; i <= MA; i++) {
		if (inst_flash_mask & (1u << i)) turn_on(drum_hit[i].led_index);
	}
	if (accent_flash) turn_on(ACCENT_1_LED);
}

static void render_mode_leds(uint8_t blink, uint8_t fast) {
	uint8_t m = 0;
	switch (sequencer.mode) {
		case FIRST_PART: m = 1 << 1; break;
		case SECOND_PART: m = 1 << 2; break;
		default: m = 1 << 3; break;
	}
	if (sequencer.pattern_layer) m |= 1 << 4; //PLAY LED: pattern layer
	if (sequencer.recording && perform() && (blink || !sequencer.START)) m |= 1 << 5; //COMPOSE LED: recording
	spi_data[LATCH_4] = (spi_data[LATCH_4] & MODE_LED_MASK) | m;

	//I/F-VAR LEDs: PERFORM = instrument layer (A = MUTE, B = SELECT), EDIT = intro/fill variation
	uint8_t b = perform() ? (sequencer.inst_layer == INST_LAYER_SELECT) : sequencer.intro_fill_var;
	if (b) { turn_off(IF_VAR_A_LED); turn_on(IF_VAR_B_LED); } else { turn_on(IF_VAR_A_LED); turn_off(IF_VAR_B_LED); }

	//part LEDs
	uint8_t part = sequencer.START ? sequencer.part_playing : (perform() ? FIRST : sequencer.part_editing);
	if (part == SECOND) { turn_off(FIRST_PART_LED); turn_on(SECOND_PART_LED); } else { turn_on(FIRST_PART_LED); turn_off(SECOND_PART_LED); }

	//pre-scale LEDs (bits 2-5 of latch 5): pre-scale, odds/ratchet value, version
	spi_data[LATCH_5] &= PRE_SCALE_LED_MASK;
	if (hs_step != NONE && sequencer.current_inst != AC) {
		uint16_t key = attr_key(sequencer.part_editing, sequencer.current_variation, hs_step, sequencer.current_inst);
		//just held: show ratchet (steady) and odds (blinking) alternately; after TAP/SHIFT: the value that was changed
		uint8_t show_odds = hs_adjusted ? hs_show_odds : (((uint16_t)(now_t - hs_t) / 50) & 1); //ratchet first
		uint8_t n = show_odds ? attr_get_odds(&pattern_attrs, key) : attr_get_ratchet(&pattern_attrs, key);
		if (!(show_odds && !fast)) spi_data[LATCH_5] |= 1 << (6 - n); //odds blink, ratchet steady
	} else if (sequencer.START) { //pass counter 1-4 (odds: every 2nd/3rd/4th pass)
		spi_data[LATCH_5] |= 1 << (6 - ((sequencer.pass_count & 3) + 1));
	}
}

static void render(void) {
	uint8_t blink = ((now_t / 25) & 1) == 0; //250 ms
	uint8_t fast = ((now_t / 8) & 1) == 0; //80 ms
	if (clear_flash && (uint16_t)(now_t - clear_flash_t) > 30) clear_flash = 0;
	if (perform()) sequencer.part_editing = sequencer.part_playing;
	render_steps(blink, fast);
	render_insts();
	render_mode_leds(blink, fast);
}

void ui_service(void) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		now_t = ui_ticks;
	}
	sample_keys();
	track_modifiers();
	handle_shift_clear();
	handle_mode_key();
	handle_variation_key();
	handle_tap();
	handle_inst_keys();
	handle_step_keys();
	if (IS_EDIT(sequencer.mode)) {
		edit_step_release();
	} else {
		hs_reset();
	}
	scene_service();
	update_pads();
	handle_clicks();
	if (press(IF_VAR_SW)) {} //handled as modifier / click
	if (press(WRITE_SW)) {
		if (IS_EDIT(sequencer.mode) && hs_mask) { //held step(s) + SHIFT: odds
			m_shift.used = 1;
			cycle_attr(0);
		}
	}
	press(CLEAR_SW);
	press(BASIC_VAR_B_SW);
	update_loop();
	service_storage();
	render();
}
