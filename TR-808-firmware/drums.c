/*
 * drums.c
 *Open808 firmware ATMEGA328PB
 *minisystem
 *system80.net
 */
#include <stdio.h>
#include <avr/io.h>
#include "drums.h"
#include "leds.h"
#include "spi.h"
#include "sequencer.h"
#include "switches.h"
#include "hardware.h"
#include "clock.h"
#include "logic.h"

uint8_t current_drum_hit = 0;
enum drum midi_note_queue[16] = {255};
//volatile uint8_t trigger_finished = 1;

struct drum_hit drum_hit[17] = { //progmem? 
	
	{0,8, 1<<BD_TRIG,NO_SWITCH, 0, BD_2_LED},
	{1,8, 1<<SD_TRIG,NO_SWITCH, 0, SD_3_LED},
	{2,8, 1<<LT_TRIG, 1<<LT_LC_SW, 1, LT_4_LED},
	{3,8, 1<<MT_TRIG, 1<<MT_MC_SW, 1, MT_5_LED},
	{4,8, 1<<HT_TRIG, 1<<HT_HC_SW, 1, HT_6_LED},
	{5,8, 1<<RS_TRIG, 1<<RS_CL_SW, 0, RS_7_LED},
	{6,8, 1<<CP_TRIG,NO_SWITCH, 0, CP_8_LED},
	{7,6, 1<<CB_TRIG,NO_SWITCH, 0, CB_9_LED},
	{8,6, 1<<CY_TRIG,NO_SWITCH, 0, CY_10_LED},
	{9,6, 1<<OH_TRIG,NO_SWITCH, 0, OH_11_LED},
	{10,6, 1<<CH_TRIG,NO_SWITCH, 0, CH_12_LED},
	{11,8, 1<<LT_TRIG, 1<<LT_LC_SW, 0, LC_LED},
	{12,8, 1<<MT_TRIG, 1<<MT_MC_SW, 0, MC_LED},
	{13,8, 1<<HT_TRIG, 1<<HT_HC_SW, 0, HC_LED},
	{14,8, 1<<RS_TRIG, 1<<RS_CL_SW, 1, CL_LED},
	{15,7, 1<<MA_TRIG,NO_SWITCH, 0, MA_LED},
	{16,8, 1<<ACCENT,NO_SWITCH, 0, ACCENT_1_LED}, //this last accent element is a bit of a hack - not currently used to access accent, but useful to turn on accent LED when accent it triggered by step sequencer (see interrupt.c)

	
			
};


/* instruments whose trigger LED should flash (bit per enum drum, AC = bit 16 kept separately) */
uint16_t inst_flash_mask = 0;
uint8_t accent_flash = 0;

/* fixed trigger outputs: OUT 1 = Low Tom, OUT 2 = Low Conga (muted voices stay silent) */
#define TRIGGER_OUT_1_DRUM LT
#define TRIGGER_OUT_2_DRUM LC

static void set_voice(uint8_t i) {
	spi_data[drum_hit[i].spi_byte_num] |= drum_hit[i].trig_bit;
	if (drum_hit[i].switch_bit != NO_SWITCH) { //set tom/conga or rimshot/clave switch
		spi_data[LATCH_3] ^= (-(drum_hit[i].switch_value) ^ spi_data[LATCH_3]) & drum_hit[i].switch_bit;
	}
	if (i == TRIGGER_OUT_1_DRUM) TRIGGER_OUT |= (1<<TRIGGER_OUT_1);
	if (i == TRIGGER_OUT_2_DRUM) TRIGGER_OUT |= (1<<TRIGGER_OUT_2);
	inst_flash_mask |= (1u << i);
}

static void start_trigger_pulse(void) {
	TIMSK0 |= (1<<OCIE0A); //enable output compare match A (turns trigger outs off)
	TCCR0B |= (1<<CS00) | (1<<CS02); //set Timer0 clock divide to /1024
	PORTD |= (1<<TRIG);
}

void trigger_drum(uint8_t note, uint8_t velocity) {
		clear_all_trigs();
		while(flag.trig_finished == 0);	//need to wait until trigger interrupt is complete before triggering new drum sound
		current_drum_hit = note;
		set_voice(note);
		if (velocity > 64) {
			spi_data[LATCH_8] |= (1<<ACCENT);
			accent_flash = 1;
		}
		start_trigger_pulse();
		flag.trig_finished = 0;
}

void clear_all_trigs(void) {
	spi_data[LATCH_8] = 0;
	spi_data[LATCH_6] &= 0b11110000; //make these masks constants
	spi_data[LATCH_7] &= 0b11011111;
	TRIGGER_OUT &= TRIGGER_OFF;
}

/* ---- ratchets ---- */
static uint16_t ratchet_inst[3]; //instruments with 2, 3 and 4 hits on the current step
static uint8_t ratchet_done; //bit per (n,k) pair already fired
static uint8_t ratchet_start; //ppqn tick at which the step was triggered (shuffle)
static uint8_t ratchet_accent;

static uint8_t ratchet_bit(uint8_t n, uint8_t k) {
	//n=2:k1 -> bit0, n=3:k1,k2 -> bit1,2, n=4:k1..k3 -> bit3..5
	return (n == 2) ? 0 : (n == 3) ? k : (k + 2);
}

void service_ratchets(void) {
	if (!sequencer.START || !(ratchet_inst[0] | ratchet_inst[1] | ratchet_inst[2])) return;
	uint8_t tick = clock.ppqn_counter;
	if (tick < ratchet_start || flag.next_step) { //next step already started: stop
		ratchet_inst[0] = ratchet_inst[1] = ratchet_inst[2] = 0;
		return;
	}
	uint8_t span = clock.divider - ratchet_start;
	uint16_t fire = 0;
	for (uint8_t n = 2; n <= 4; n++) {
		if (!ratchet_inst[n - 2]) continue;
		for (uint8_t k = 1; k < n; k++) {
			uint8_t b = 1 << ratchet_bit(n, k);
			if (!(ratchet_done & b) && tick >= ratchet_start + ratchet_tick(span, n, k)) {
				ratchet_done |= b;
				fire |= ratchet_inst[n - 2];
			}
		}
	}
	if (!fire) return;
	clear_all_trigs();
	for (uint8_t i = BD; i <= MA; i++) {
		if (fire & (1u << i)) set_voice(i);
	}
	if (ratchet_accent) spi_data[LATCH_8] |= 1<<ACCENT;
	start_trigger_pulse();
}

void trigger_step(uint8_t part_playing) { //trigger all drums on the play step
	uint8_t step = sequencer.play_step;
	uint8_t var = sequencer.current_variation;
	uint16_t data = sequencer.pattern[var].part[part_playing][step];
	clear_all_trigs();
	ratchet_inst[0] = ratchet_inst[1] = ratchet_inst[2] = 0;
	ratchet_done = 0;
	ratchet_start = clock.ppqn_counter;
	TIMSK0 |= (1<<OCIE0A); //enable output compare match A
	TCCR0B |= (1<<CS00) | (1<<CS02); //set Timer0 clock divide to /1024
	uint8_t any = 0;
	for (uint8_t i = BD; i <= MA; i++) {
		if (!((data >> i) & 1)) continue;
		if (sequencer.skip_mask & (1u << i)) continue; //already played live while recording
		if (drum_hit[i].muted) continue;
		uint16_t key = attr_key(part_playing, var, step, i);
		if (!odds_play(attr_get_odds(&pattern_attrs, key), sequencer.pass_count)) continue;
		uint8_t r = attr_get_ratchet(&pattern_attrs, key);
		if (r > 1) ratchet_inst[r - 2] |= (1u << i);
		set_voice(i);
		any = 1;
	}
	sequencer.skip_mask = 0;
	ratchet_accent = 0;
	if ((sequencer.pattern[var].accent[part_playing] >> step) & 1) {
		spi_data[LATCH_8] |= 1<<ACCENT;
		accent_flash = 1;
		ratchet_accent = 1;
		any = 1;
	}
	if (any) PORTD |= (1<<TRIG);
}
