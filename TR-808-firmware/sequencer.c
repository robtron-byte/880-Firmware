/*
 * sequencer.c
 * Open808 firmware ATMEGA328PB
 * minisystem
 * system80.net
 */
#include <stdio.h>
#include <avr/io.h>
#include "string.h"
#include "sequencer.h"
#include "mode.h"
#include "leds.h"
#include "switches.h"
#include "hardware.h"
#include "clock.h"
#include "drums.h"
#include "adc.h"
#include "spi.h"
#include "midi.h"
#include "twi_eeprom.h"
#include "logic.h"
#include "xnormidi-develop/midi.h"
#include "xnormidi-develop/midi_device.h"
#include "xnormidi-develop/bytequeue/bytequeue.h"

struct sequencer sequencer;
struct recall recall;
volatile struct flag flag;
struct attr_block pattern_attrs;
//pattern_data next_pattern;
//uint8_t pre_scale_index = 1; //default is 4/4, so PRE_SCALE_3
uint8_t pre_scale[4] = {PRE_SCALE_4, PRE_SCALE_3, PRE_SCALE_2, PRE_SCALE_1};

void update_tempo(void) {
	static uint16_t new_tempo_adc = 0;
	static uint16_t current_tempo_adc = 0;
	int tempo_adc_change = 0;
	new_tempo_adc = read_tempo_pot();
	tempo_adc_change = new_tempo_adc - current_tempo_adc;
	current_tempo_adc = current_tempo_adc + (tempo_adc_change >>2); //low pass filter
	
	clock.rate = (ADC_MAX - current_tempo_adc) + TIMER_OFFSET; //offset to get desirable tempo range

	//if (clock.rate != clock.previous_rate) { //this step is maybe not necessary? Just always update OCR1A?
		
		update_clock_rate(clock.rate);
		
	//}
	
	//clock.previous_rate = clock.rate;
	
}

void show_version_steps(void) {
	uint8_t version_step = (sequencer.version/10)%10;
	if (version_step > 0) turn_on(version_step - 1);
}

void process_tick(void) {
	//TRIGGER_OUT |= (1<<TRIGGER_OUT_2);
		
	if (flag.shuffle_step) { 
		
		//ok, maybe use shuffle_amount to index an array of the best sounding shuffle ppqn counts
		if (++sequencer.shuffle_ppqn_count == (sequencer.shuffle_amount*sequencer.shuffle_multplier)) { //X2 to increase shuffle time. 1-5 ppqn @ new 96 ppqn internal resolution was a bit too subtle. X2 is compatible with all shuffle levels for all pre-scales whereas X3 was not
				
			//sequencer.shuffle_ppqn_count = 0;
			flag.next_step = 1;
			flag.shuffle_step = 0;
				
		}

	}
	
	uint8_t even_step = sequencer.current_step & 1;	//is current step even or odd?
	
	if (++clock.ppqn_counter == clock.divider) { //trigger next step
		//clock.tick_counter = 0;
		if (flag.pre_scale_change) {
			
			clock.divider = pre_scale[sequencer.pre_scale];       
		}
		if (flag.shuffle_change) {TCCR1B |= (1<<CS12);
			
			sequencer.shuffle_amount = sequencer.new_shuffle_amount;
			flag.shuffle_change = 0;
			
		}
		//handle shuffle
		if ((sequencer.shuffle_amount != 0) && !even_step && !flag.shuffle_change) { //confusing because step is incremented below, so it becomes even but is currently odd
			//set shuffle_step flag and then use shuffle ppqn counter to count amount of shuffle pqqn before setting next step flag
			sequencer.shuffle_ppqn_count = 0;
			flag.shuffle_step = 1;
			
		}
		
		if (!flag.shuffle_step) flag.next_step = 1;
		//if (sequencer.primed == 1) {
			//sequencer.current_measure_auto_fill = -1;
			//sequencer.track_measure = -1;
			//sequencer.primed = 0;
		//}	
		//sequencer.primed = 0;
		if (sequencer.current_step++ == sequencer.step_num[sequencer.part_playing] && sequencer.START) {
			if (sequencer.primed) { //this causes problems when first part has less than 16 steps. why? - FIXED!
				sequencer.primed = 0;
				//turn_on(IF_VAR_B_LED);
				sequencer.current_step = 0;
				//need to do something here to handle first part with less than 16 steps.
			} else {
				flag.new_measure = 1;
			}
			
		}
		
		clock.beat_counter++; //overflows every 4 beats 
		clock.ppqn_counter = 0;
	//50% step width, sort of - use for flashing step and variation LEDs to tempo
	} else if (((clock.ppqn_counter == clock.divider >> 1) && !even_step) || ((even_step && (clock.ppqn_counter == (clock.divider >> 1) + sequencer.shuffle_amount)))  ){
		//if it's an odd step then set half_step flag OR if it's an even step and the appropriate amount of shuffle has transpired then set half_step flag
		//this is gobbledygook is so that LEDs flash with right timing and period
		flag.half_step = 1;

	}
	
	if (clock.source == EXTERNAL) { 
		
		if (++clock.slave_ppqn_ticks == clock.sync_count) { //this works for 24 ppqn MIDI and DIN sync clock, but needs tweaking for external sync pulse that runs at 2 ppqn or 1 ppqn
			clock.slave_ppqn_ticks = 0; //reset
			flag.wait_for_master_tick = 1; //not currently used
			TCCR1B &= ~(1<<CS12); //turn off master timer
			//TCNT1 = 0;
		}
		
	}
	//TRIGGER_OUT &= ~(1<<TRIGGER_OUT_1);
		
}
	


uint8_t reload_pending = 0;
uint8_t reload_current = 0; //current pattern was overwritten by a copy: reload it at the next measure end, drop RAM edits
uint8_t fill_active = 0; //a fill or intro is playing instead of the basic pattern
uint8_t fill_from_pattern, fill_from_bank; //basic pattern to return to

static uint8_t is_fill_return(uint8_t p, uint8_t b) {
	return fill_active && p == fill_from_pattern && b == fill_from_bank;
}
uint8_t recall_dirty = 0;
void pattern_take(uint8_t pattern_num, uint8_t pattern_bank, uint8_t backup);
void apply_length_changes(void);

void process_start(void) {

		PORTC &= ~(1<<SYNC_LED_R);
		PORTE &= ~(1<<SYNC_LED_Y);
		clock.sync_led_mask = 0;
		sequencer.part_playing = FIRST;
		sequencer.pass_count = 0;
		sequencer.loop_idx = 0;
		sequencer.skip_mask = 0;
		if (sequencer.clock_mode == DIN_SYNC_MASTER) {
			clock.ppqn_counter = 0;
		} else {
			clock.ppqn_divider_tick = 0;
			//need to prime sequencer so that first step (downbeat) occurs on first incoming clock pulse
			sequencer.current_step = sequencer.step_num[FIRST];
			clock.ppqn_counter = clock.divider - 1;
			sequencer.primed = 1;
		}
		if (clock.source == EXTERNAL) {
			flag.slave_start = 1;
			clock.slave_ppqn_ticks = 0;
			PORTE |= (1 << SYNC_LED_Y); //orange sync light when slaved
			PORTC |= (1 << SYNC_LED_R);
			clock.tick_counter = 0;

		} else {

			flag.slave_start = 0;
			PORTE &= ~(1 << SYNC_LED_Y);
			PORTC |= (1 << SYNC_LED_R); //red for internal clock running
		}

		//reset variation on start
		flag.variation_change = 0;
		if (sequencer.variation_mode == VAR_A || sequencer.variation_mode == VAR_AB) {
			sequencer.current_variation = VAR_A; //start on variation A
		} else {
			sequencer.current_variation = VAR_B;
		}
		sequencer.basic_variation = sequencer.current_variation;

		if (sequencer.clock_mode == DIN_SYNC_MASTER || sequencer.clock_mode == DIN_SYNC_SLAVE) {
			//don't set flag.next_step here because need to send a couple of DIN Sync clock pulses before start
			flag.din_start = 1;
			clock.din_ppqn_pulses = 0;
		} else {
			if (sequencer.clock_mode == MIDI_MASTER) {
				midi_send_start(&midi_device);
				midi_send_clock(&midi_device);
			}
		}

		if (sequencer.mode == MANUAL_PLAY) {
			if (flag.intro) {
				flag.intro = 0;
				sequencer.current_variation = sequencer.intro_fill_var;
				flag.pattern_change = 1;
			}
			if (sequencer.fill_mode != MANUAL) { //need to prime fill count on first measure
				if (++sequencer.current_measure_auto_fill == sequencer.fill_mode) {
					flag.fill = 1;
				}
			}
		}

		spi_data[LATCH_3] |= CONGAS_OFF;

		//set trigger off timer
		TIMER0_OUTPUT_COMPARE = TIMER0_15_MS;

		//reset timers:
		TCNT1 = 0;
		TCNT3 = 0;
		TCCR1B |= (1<<CS12); //ensure timer is on
}

void process_stop(void) {
		PORTC &= ~(1<<SYNC_LED_R);
		PORTE &= ~(1<<SYNC_LED_Y);
		clock.sync_led_mask = 0;
		sequencer.part_playing = FIRST;
		sequencer.loop_mask = 0;
		sequencer.current_measure_auto_fill = 0; //reset current measure
		flag.fill = 0;
		flag.pattern_change = 0;
		inst_flash_mask = 0;
		//apply pending measure-end changes now
		reload_pending = 0;
		if (reload_current) { //copied content must be loaded, RAM edits are dropped
			reload_current = 0;
			flag.pattern_edit = 0;
			flag.length_change = 0;
			flag.second_clear = 0;
			if (sequencer.new_pattern == sequencer.current_pattern && sequencer.new_bank == sequencer.pattern_bank) {
				pattern_load(sequencer.current_pattern, sequencer.pattern_bank);
			}
		}
		if (flag.second_clear || flag.length_change) apply_length_changes();
		//return to the selected pattern (a fill or intro may be playing) and apply a pending pattern/bank change
		if (sequencer.mode == MANUAL_PLAY || sequencer.new_pattern != sequencer.current_pattern || sequencer.new_bank != sequencer.pattern_bank) {
			pattern_load(sequencer.new_pattern, sequencer.new_bank);
		} else if (flag.pattern_edit) {
			flag.pattern_edit = 0;
			start_write_current_pattern(sequencer.current_pattern, sequencer.pattern_bank);
			attr_queue_current();
		}
		if (clock.source == INTERNAL) {
			PORTD &= ~(1 << DIN_RUN_STOP);
			if (sequencer.clock_mode == MIDI_MASTER) {
				midi_send_stop(&midi_device);
			}
		}

		spi_data[LATCH_3] |= CONGAS_OFF;

		//set trigger off timer for incoming MIDI, currently only applies to MIDI_SLAVE sync mode
		TIMER0_OUTPUT_COMPARE = TIMER0_1_MS;
}

void update_fill(void) {
	if (flag.fill) { //problem with current_measure overfow here - only need to increment measure if fill_mode != 0
		sequencer.current_measure_auto_fill = 0;
		flag.fill = 0;
		flag.pre_scale_change = 1;
		if (!fill_active) {
			fill_from_pattern = sequencer.current_pattern;
			fill_from_bank = sequencer.pattern_bank;
		}
		read_next_pattern(sequencer.current_intro_fill, sequencer.pattern_bank);
		fill_active = 1;
		sequencer.part_playing = FIRST;
		sequencer.new_pattern = fill_from_pattern; //return here after the fill
		sequencer.new_bank = fill_from_bank;
		sequencer.current_pattern = sequencer.current_intro_fill;
		sequencer.basic_variation = sequencer.current_variation; //store current variation
		sequencer.current_variation = sequencer.intro_fill_var;
		flag.pattern_change = 1; //return to the basic rhythm after the fill
	} else {
		if (sequencer.fill_mode != MANUAL) {
			if (++sequencer.current_measure_auto_fill == sequencer.fill_mode) {
				flag.fill = 1;
			}
		}
	}
}

/* end of a full pattern pass (after the 2nd part, or after the 1st part when there is no 2nd part) */
static void update_pattern(void) {
	sequencer.pass_count++;
	toggle_variation();
	//pattern chain: move on to the next pattern of the range
	if (sequencer.chain_active && !flag.pattern_change && sequencer.mode == MANUAL_PLAY) {
		sequencer.new_pattern = chain_next(sequencer.current_pattern, sequencer.chain_start, sequencer.chain_end);
		if (sequencer.new_pattern != sequencer.current_pattern) flag.pattern_change = 1;
	}
	if (flag.pattern_change) {
		flag.pattern_change = 0;
		flag.pre_scale_change = 1; //need to handle any change in pre-scale
		uint8_t fill_return = is_fill_return(sequencer.new_pattern, sequencer.new_bank);
		pattern_take(sequencer.new_pattern, sequencer.new_bank, !fill_return);
		if (!fill_return) {
			//reset A/B variation when a pattern changes
			sequencer.current_variation = VAR_A;
			if (sequencer.variation_mode == VAR_B) sequencer.current_variation = VAR_B;
		} else {
			sequencer.current_variation = sequencer.basic_variation;
		}
		sequencer.part_playing = FIRST;
		sequencer.pass_count = 0;
	}
	if (sequencer.mode == MANUAL_PLAY) {
		update_fill();
	}
}

/* step length and 2nd part changes only happen at the end of a measure */
void apply_length_changes(void) {
	if (flag.second_clear) {
		flag.second_clear = 0;
		sequencer.step_num[SECOND] = NO_STEPS;
		for (uint8_t v = 0; v < 2; v++) {
			sequencer.pattern[v].accent[SECOND] = 0;
			memset(sequencer.pattern[v].part[SECOND], 0, sizeof(sequencer.pattern[v].part[SECOND]));
		}
		attr_remove_part(&pattern_attrs, SECOND);
		sequencer.second_untouched = 0;
		if (sequencer.part_playing == SECOND) sequencer.part_playing = FIRST;
		if (sequencer.mode == SECOND_PART) { //the edited part is gone: back to the 1st part
			sequencer.mode = FIRST_PART;
			sequencer.part_editing = FIRST;
		}
		flag.pattern_edit = 1;
	}
	if (flag.length_change) {
		flag.length_change = 0;
		sequencer.step_num[FIRST] = sequencer.step_num_new;
		if (sequencer.step_num[SECOND] != NO_STEPS) sequencer.step_num[SECOND] = sequencer.step_num_new;
		flag.pattern_edit = 1;
	}
	update_inst_led_mask();
}

void process_new_measure(void) {

	sequencer.current_step = 0;
	sequencer.loop_idx = 0;
	if (reload_current) { //a copy overwrote this pattern: load it now, RAM edits are dropped
		reload_current = 0;
		reload_pending = 0;
		flag.pattern_edit = 0;
		flag.length_change = 0;
		flag.second_clear = 0;
		uint8_t pending = flag.pattern_change; //keep a pattern/bank the user picked meanwhile
		uint8_t next_p = sequencer.new_pattern, next_b = sequencer.new_bank;
		pattern_take(sequencer.current_pattern, sequencer.pattern_bank, 1);
		flag.pattern_change = pending;
		sequencer.new_pattern = next_p;
		sequencer.new_bank = next_b;
		sequencer.part_playing = SECOND; //the toggle below restarts with the 1st part
		if (sequencer.step_num[SECOND] == NO_STEPS) sequencer.part_playing = FIRST;
		flag.pre_scale_change = 1;
	}
	if (reload_pending && storage_idle()) { //otherwise try again at the next measure (never block the downbeat)
		reload_pending = 0;
		if (storage_reload()) {
			flag.pattern_edit = 1; //store the restored pattern
			flag.length_change = 0;
			flag.second_clear = 0;
		}
	}
	if (flag.second_clear || flag.length_change) apply_length_changes();
	if (flag.pattern_edit == 1) {
		flag.pattern_edit = 0;
		start_write_current_pattern(sequencer.current_pattern, sequencer.pattern_bank); //save changed pattern at end of measure
		attr_queue_current();
	}

	if (sequencer.step_num[SECOND] != NO_STEPS) { //two parts: toggle part playing
		if (sequencer.part_playing == SECOND) {
			sequencer.part_playing = FIRST;
			update_pattern();
		} else {
			sequencer.part_playing = SECOND;
		}
	} else {
		sequencer.part_playing = FIRST;
		update_pattern();
	}

	if (IS_EDIT(sequencer.mode)) update_inst_led_mask();

	//handle pre-scale change
	if (flag.pre_scale_change) {
		flag.pre_scale_change = 0;
		sequencer.shuffle_amount = 0; //reset shuffle amount when pre-scale changes
		clock.divider = pre_scale[sequencer.pre_scale];
	}
}

/* which step is played: step loop (PERFORM, LOOP layer) or the direction of the pattern */
static void compute_play_step(void) {
	uint8_t last = sequencer.step_num[sequencer.part_playing];
	if (last > 15) last = 15;
	uint8_t loop = 0xFF;
	if (sequencer.loop_mask) {
		loop = loop_pick(sequencer.loop_mask, sequencer.loop_idx++);
	}
	if (loop != 0xFF) {
		sequencer.play_step = (loop > last) ? last : loop;
		return;
	}
	uint8_t dir = attr_direction(&pattern_attrs);
	if (dir == DIR_RANDOM) sequencer.rnd = lfsr_next(sequencer.rnd ? sequencer.rnd : 0x5A);
	sequencer.play_step = dir_map(dir, sequencer.current_step, last, sequencer.pass_count, sequencer.rnd);
}

void process_step(void){
	if (flag.next_step) {
		flag.next_step = 0;
		if (sequencer.START) {
			if (flag.new_measure) {
				flag.new_measure = 0;
				process_new_measure();
				if (!sequencer.START) return;
			}
			compute_play_step();
			trigger_step(sequencer.part_playing);
		}
	} else if (flag.half_step) {
		inst_flash_mask = 0; //trigger LEDs off on the half step
		accent_flash = 0;
		spi_data[LATCH_5] &= ~(led[BASIC_VAR_A_LED].spi_bit | led[BASIC_VAR_B_LED].spi_bit); //this clears basic variation LEDs
		if (sequencer.START) {
			sequencer.var_led_mask = led[BASIC_VAR_A_LED].spi_bit;
			if (sequencer.current_variation == sequencer.variation_mode) {
				if (sequencer.current_variation == VAR_B && !flag.variation_change) sequencer.var_led_mask = led[BASIC_VAR_B_LED].spi_bit;
			} else {
				if (sequencer.current_variation == VAR_B) sequencer.var_led_mask = led[BASIC_VAR_B_LED].spi_bit;
			}
			if (clock.beat_counter <2) { //keeps basic-variation LEDs flashing in time with the clock
				if (sequencer.current_variation != sequencer.variation_mode) {
					if (sequencer.current_variation == VAR_A) {
						sequencer.var_led_mask |= led[BASIC_VAR_B_LED].spi_bit;
					} else {
						sequencer.var_led_mask |= led[BASIC_VAR_A_LED].spi_bit;
					}
				} else if (sequencer.current_variation == VAR_A && flag.variation_change) {
					sequencer.var_led_mask |= led[BASIC_VAR_B_LED].spi_bit;
				}
			}
		} else { //STOP LED behavior when sequencer is not running
			switch (sequencer.variation_mode) {
				case VAR_A: case VAR_AB:
				sequencer.var_led_mask = led[BASIC_VAR_A_LED].spi_bit;
				break;
				case VAR_B:
				sequencer.var_led_mask = led[BASIC_VAR_B_LED].spi_bit;
				break;
			}
			if ((clock.beat_counter <2) && (!sequencer.SHIFT) && (!sequencer.TAP_HELD) && (!sequencer.ALT)) {
				if (sequencer.variation_mode == VAR_AB) sequencer.var_led_mask |= led[BASIC_VAR_B_LED].spi_bit;	//flash VAR_B LED to indicate A/B mode
			}
		}
		flag.half_step = 0;
	} else if (clock.source == EXTERNAL && !sequencer.START) { //this handles variation LEDs when waiting for external clock signal
		spi_data[LATCH_5] &= ~(led[BASIC_VAR_A_LED].spi_bit | led[BASIC_VAR_B_LED].spi_bit); //this clears basic variation LEDs
		switch (sequencer.variation_mode) {
			case VAR_A:
				sequencer.var_led_mask = led[BASIC_VAR_A_LED].spi_bit;
				break;
			case VAR_B:
				sequencer.var_led_mask = led[BASIC_VAR_B_LED].spi_bit;
				break;
			case VAR_AB:
				sequencer.var_led_mask = led[BASIC_VAR_A_LED].spi_bit | led[BASIC_VAR_B_LED].spi_bit;
				break;
		}
	}
	spi_data[LATCH_5] |= sequencer.var_led_mask;
}

void update_shuffle(uint8_t shuffle_amount) {
	if (shuffle_amount < SHUFFLE_MAX) { //ensure button press is within control range of shuffle selection
		//if shuffle changes need to change shuffle after step changes, otherwise next step flag may not be set on time
		sequencer.new_shuffle_amount = shuffle_amount; //shuffle ranges from 0-5
		flag.shuffle_change = 1;
		flag.pattern_edit = 1; //shuffle is stored with the pattern
	}
}

void toggle_variation(void) {
	if (flag.variation_change == 1) {
		flag.variation_change = 0;
		switch (sequencer.variation_mode) {
			case VAR_A: case VAR_AB:
				sequencer.current_variation = VAR_A;
			break;
			case VAR_B:
				sequencer.current_variation = VAR_B;
			break;
		}
	} else if (sequencer.variation_mode == VAR_AB) {
		sequencer.current_variation ^= 1<<0; //toggle state
	}
}

void read_next_pattern(uint8_t pattern_num, uint8_t pattern_bank) {
	static pattern_data next_pattern; //static: 140 bytes, not on the stack
	storage_finish_for(pattern_num, pattern_bank);
	read_pattern_into(&next_pattern, pattern_num*PAGES_PER_PATTERN*PAGE_SIZE, pattern_bank);
	if (next_pattern.step_num[FIRST] > 15) { //erased EEPROM
		default_pattern_data(&next_pattern);
	}
	sequencer.pattern[VAR_A] = next_pattern.variation_a;
	sequencer.pattern[VAR_B] = next_pattern.variation_b;
	sequencer.step_num[FIRST] = next_pattern.step_num[FIRST];
	//2nd part is always as long as the 1st part (older patterns may store a different length)
	sequencer.step_num[SECOND] = (next_pattern.step_num[SECOND] == NO_STEPS) ? NO_STEPS : next_pattern.step_num[FIRST];
	sequencer.pre_scale = 1; //880 Perform: fixed resolution (PRE-SCALE 3, 16ths in 4/4)
	clock.divider = pre_scale[sequencer.pre_scale];
	if (next_pattern.shuffle >= SHUFFLE_MAX) next_pattern.shuffle = 0; //pre-shuffle storage patterns
	sequencer.new_shuffle_amount = next_pattern.shuffle;
	flag.shuffle_change = 1;
	attr_read(attr_slot(pattern_num, pattern_bank), &pattern_attrs);
	sequencer.step_num_new = sequencer.step_num[FIRST];
	sequencer.second_untouched = 0;
	flag.length_change = 0;
	flag.second_clear = 0;
	update_inst_led_mask();
}

/* switch the RAM pattern (used at the end of a measure and when stopped) */
void pattern_take(uint8_t pattern_num, uint8_t pattern_bank, uint8_t backup) {
	uint8_t changed = (pattern_num != sequencer.current_pattern) || (pattern_bank != sequencer.pattern_bank);
	read_next_pattern(pattern_num, pattern_bank);
	sequencer.current_pattern = sequencer.new_pattern = pattern_num;
	sequencer.pattern_bank = sequencer.new_bank = pattern_bank;
	fill_active = 0;
	flag.intro = 0;
	if (sequencer.mode == SECOND_PART) second_part_autocopy(); //still editing the 2nd part
	if (changed) reload_pending = 0;
	if (backup && (changed || !storage_backup_for(pattern_num, pattern_bank))) {
		storage_queue_backup(); //for reload
		if (changed) recall_dirty = 1;
	}
}

/* load immediately (sequencer stopped): unsaved changes of the current pattern are written first */
void pattern_load(uint8_t pattern_num, uint8_t pattern_bank) {
	if (flag.pattern_edit) {
		flag.pattern_edit = 0;
		start_write_current_pattern(sequencer.current_pattern, sequencer.pattern_bank);
		attr_queue_current();
	}
	pattern_take(pattern_num, pattern_bank, !is_fill_return(pattern_num, pattern_bank));
	sequencer.part_playing = FIRST;
	sequencer.current_step = 0;
	clock.ppqn_counter = 0; //there's a glitch when switching to new patterns that can somehow cause overflow and next_step and half_step flags aren't set
	clock.beat_counter = 0;
	flag.pattern_change = 0;
}

/* pattern/bank selection by the user: running -> at the end of the pattern, stopped -> now */
void pattern_request(uint8_t pattern_num, uint8_t pattern_bank) {
	if (pattern_num >= PATTERNS_PER_BANK || pattern_bank >= NUM_BANKS) return;
	if (sequencer.START) {
		sequencer.new_pattern = pattern_num;
		sequencer.new_bank = pattern_bank;
		flag.pattern_change = (pattern_num != sequencer.current_pattern) || (pattern_bank != sequencer.pattern_bank);
	} else {
		pattern_load(pattern_num, pattern_bank);
	}
}

/* ---- 2nd part ---- */
void second_part_autocopy(void) {
	if (flag.second_clear && sequencer.second_untouched) { //copy still there, removal was only scheduled
		flag.second_clear = 0;
		return;
	}
	if (sequencer.step_num[SECOND] != NO_STEPS || flag.second_clear) return;
	copy_part(FIRST); //copies both variations and sets the 2nd part length
	attr_copy_part(&pattern_attrs, FIRST, SECOND);
	sequencer.second_untouched = 1;
}

void second_part_leave(void) {
	if (sequencer.second_untouched) {
		flag.second_clear = 1; //nothing was changed: remove the copy again at the end of the measure
		if (!sequencer.START) apply_length_changes();
	}
}

/* SHIFT + CLEAR in EDIT: variation A -> B (both parts, odds/ratchets included) */
void copy_variation_a_to_b(void) {
	sequencer.pattern[VAR_B] = sequencer.pattern[VAR_A];
	attr_copy_var(&pattern_attrs, VAR_A, VAR_B);
	flag.pattern_edit = 1;
	if (sequencer.part_editing == SECOND) sequencer.second_untouched = 0;
	update_inst_led_mask();
}

/* ---- CLEAR in EDIT ---- */
void clear_current_instrument(void) {
	uint8_t v = sequencer.current_variation;
	for (uint8_t part = 0; part < NUM_PARTS; part++) {
		if (sequencer.current_inst == AC) {
			sequencer.pattern[v].accent[part] = 0;
		} else {
			for (uint8_t i = 0; i < NUM_STEPS; i++) {
				sequencer.pattern[v].part[part][i] &= ~(1u << sequencer.current_inst);
			}
		}
	}
	if (sequencer.current_inst != AC) attr_remove_inst(&pattern_attrs, v, sequencer.current_inst);
	flag.pattern_edit = 1;
	update_inst_led_mask();
}

void clear_current_pattern(void) {
	for (uint8_t v = 0; v < 2; v++) {
		sequencer.pattern[v].accent[FIRST] = 0;
		sequencer.pattern[v].accent[SECOND] = 0;
		memset(sequencer.pattern[v].part, 0, sizeof(sequencer.pattern[v].part));
	}
	uint8_t dir = attr_direction(&pattern_attrs);
	attr_clear(&pattern_attrs);
	attr_set_direction(&pattern_attrs, dir);
	sequencer.step_num_new = 15;
	flag.length_change = 1;
	if (sequencer.step_num[SECOND] != NO_STEPS) flag.second_clear = 1;
	if (sequencer.mode == SECOND_PART) sequencer.second_untouched = 0;
	if (sequencer.pre_scale != 1) {
		sequencer.pre_scale = 1; //default PRE_SCALE_3
		flag.pre_scale_change = 1;
	}
	if (!sequencer.START) {
		apply_length_changes();
		clock.divider = pre_scale[sequencer.pre_scale];
		flag.pre_scale_change = 0;
	}
	flag.pattern_edit = 1;
	update_inst_led_mask();
}
