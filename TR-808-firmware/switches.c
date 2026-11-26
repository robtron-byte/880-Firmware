/*
 * switches.c
 * Open808 firmware ATMEGA328PB
 * minisystem
 * system80.net
 */

#include <stdio.h>
#include <avr/io.h>
#include <string.h>
#include <avr/wdt.h>
#include "hardware.h"
#include "leds.h"
#include "switches.h"
#include "spi.h"
#include "sequencer.h"
#include "drums.h"
#include "clock.h"
#include "midi.h"
#include "select_instrument_with_soloing.h"
#include "twi_eeprom.h"
#include "xnormidi-develop/midi.h"
#include "xnormidi-develop/midi_device.h"
#include "xnormidi-develop/bytequeue/bytequeue.h"


struct button button[NUM_BUTTONS] = {
	
{	0	,	4	,0,0,},
{	1	,	4	,0,0,},
{	2	,	4	,0,0,},
{	3	,	4	,0,0,},
{	4	,	4	,0,0,},
{	5	,	4	,0,0,},
{	6	,	4	,0,0,},
{	7	,	4	,0,0,},
{	0	,	3	,0,0,},
{	1	,	3	,0,0,},
{	2	,	3	,0,0,},
{	3	,	3	,0,0,},
{	4	,	3	,0,0,},
{	5	,	3	,0,0,},
{	6	,	3	,0,0,},
{	7	,	3	,0,0,},
{	5	,	2	,0,0,},
{	6	,	2	,0,0,},
{	7	,	2	,0,0,},
{	0	,	1	,0,0,},
{	1	,	1	,0,0,},
{	2	,	1	,0,0,},
{	3	,	1	,0,0,},
{	4	,	1	,0,0,},
{	5	,	1	,0,0,},
{	6	,	1	,0,0,},
{	7	,	1	,0,0,},
{	0	,	0	,0,0,},
{	1	,	0	,0,0,},
{	2	,	0	,0,0,},
{	0	,	2	,0,0,},
{	1	,	2	,0,0,},
{	2	,	2	,0,0,},
{	3	,	2	,0,0,},
{	4	,	2	,0,0,}	
	
	
	};
	
uint8_t current_start_stop_tap_state = 0;
uint8_t previous_start_stop_tap_state = 0;	
	
void parse_switch_data(void) {
	
	for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
		
		//button[i].current_state = (switch_states[button[i].spi_byte] & button[i].spi_bit) & 1; //need to fix this
		uint8_t current_state = (spi_current_switch_data[button[i].spi_byte] >> button[i].spi_bit) &1 ;
		button[i].state |= current_state; //new press (rising edge); handlers clear it
		
	}
	
	
}	

void check_start_stop_tap(void) {
	//if (sequencer.mode == PATTERN_CLEAR) return; //do nothing
	current_start_stop_tap_state = PINB;
	
	sequencer.TAP_HELD = (current_start_stop_tap_state >> TAP) & 1;
	
	current_start_stop_tap_state ^= previous_start_stop_tap_state;
	previous_start_stop_tap_state ^= current_start_stop_tap_state;
	current_start_stop_tap_state &= previous_start_stop_tap_state;
	
	//if ((sequencer.START && (current_start_stop_tap_state >> TAP) &1)) {
			//
		//current_start_stop_tap_state ^= (1<<TAP); //toggle tap switch bit
		//flag.tap = 1;
			//
	//}
	
	if ((current_start_stop_tap_state >> TAP) & 1) {
		
		current_start_stop_tap_state ^= (1<<TAP); //toggle tap switch bit
		flag.tap = 1;
		
	}
	
		if ((clock.source == EXTERNAL) && (sequencer.clock_mode != PULSE_SYNC_SLAVE)) { //what about stopping when MIDI slave? this would be the place to do it, yeah?
			 if (flag.din_stop == 1) {
				 
				 flag.din_stop = 0;
				 sequencer.START = 0;
				 process_stop(); //need to process stop outside of DIN SYNC slave ISR, yeah?
			 }
			 if (current_start_stop_tap_state >> START_STOP) {
				 if (sequencer.SHIFT) {
					 //a shift + START/STOP should allow start/stopping in MIDI and DIN sync slave modes
					 flag.slave_stop = 1;
			 
					 //while (clock.tick_counter != 0);
					 //OK, so knowing clock.ppqn counter value here and maybe clock.tick_counter might be helpful when starting as slave?
				 }	else {
					 flag.slave_stop = 0;
					 return; //get out of here because when using external clock you don't need to process start/stop button activity - ACTUALLY, you really should! - in fact, need to handle start/stop when synced to external pulse volca style
			 
				 }
			 }
		}
	
	uint8_t start_state = sequencer.START;
	sequencer.START ^= current_start_stop_tap_state >> START_STOP;
	
	if (sequencer.START && (start_state == 0)) { //initialize sequencer when start is detected
		if (flag.slave_stop) {
			//while (clock.tick_counter != 0); //need to wait to start on quarter note. This works, BUT need to be very careful that flag.slave stop is reset else where otherwise timeout and reset will occur
			// oh, GOTCHA! this busy/wait will be interrupted by hardware clocks, which results in very tight, repeatedably slave re-starts, BUT BUT BUT, MIDI clock does not generate an interrupt. So
			//can MIDI clock force an interrupt?
			//get rid of while busy/wiat here and instead set flag to call process_start in the from within process_external_clock event
			flag.delay_slave_start = 1;
			flag.slave_stop = 0;
		} else {
			process_start();
		}
		
			
	}
	
	if ((sequencer.START == 0) && (start_state == 1)) {//when stop is first pressed need to handle lingering instrument LEDs 
		
		process_stop();
		
	} 
	

	
}

void assign_mutes(uint8_t drum_index) {
	
	if (drum_hit[drum_index].switch_bit != NO_SWITCH || (drum_index == CP)) { //need to handle toggling between instrument
		//maybe evaluate the two drum states as 00, 01, 10, 11 and then use switch case
		uint8_t mute_state = (drum_hit[drum_index].muted) | (drum_hit[drum_index + SW_DRUM_OFFSET].muted << 1);
		switch (mute_state) {
					
			case 0:
			drum_hit[drum_index].muted = 1;
			drum_hit[drum_index + SW_DRUM_OFFSET].muted = 0;
			break;
					
			case 1:
			drum_hit[drum_index].muted = 0;
			drum_hit[drum_index + SW_DRUM_OFFSET].muted = 1;
			break;
					
			case 2:
			drum_hit[drum_index].muted = 1;
			drum_hit[drum_index + SW_DRUM_OFFSET].muted = 1;
			break;
					
			case 3:
			drum_hit[drum_index].muted = 0;
			drum_hit[drum_index + SW_DRUM_OFFSET].muted = 0;
			break;
					
		}
				
	} else {
				
		drum_hit[drum_index].muted ^= 1<<0; //toggle drum mute
				
	}	
}

void clear_mutes(void) {
	
	for (uint8_t i = BD; i <= MA; i++) {
		
		drum_hit[i].muted = 0;
		
	}
	
}

void assign_solo(uint8_t drum_index) {

	for (uint8_t i = BD; i <= MA; i++) {
		
		drum_hit[i].muted = 1;
		
		// TODO(omar): I think we need this too, as only 1 solo at a time
		drum_hit[i].solo = 0;
	}	
	
	drum_hit[drum_index].solo = 1;
	drum_hit[drum_index].muted = 0;
	
}

void select_instrument(uint8_t drum_index) {
	struct SoloState state;
						
	state.currentInstrument = sequencer.current_inst;
	state.isSolo = false;
	state.secondaryInstrument = EMPTY;
						
	for (uint8_t j = 0; j < NUM_INST; ++j) {
		if (drum_hit[j].solo) {
			state.currentInstrument = j;
			state.isSolo = true;
			state.secondaryInstrument = sequencer.current_inst;
			break;
		}
	}
						
	struct SoloState new_state;
	new_state = handleInstrumentTransition(state, drum_index, sequencer.CLEAR);
	turn_off_all_inst_leds();
	if (new_state.isSolo) {
		assign_solo(new_state.currentInstrument);
		sequencer.current_inst = new_state.secondaryInstrument != EMPTY ? new_state.secondaryInstrument : new_state.currentInstrument;
	} else {
		sequencer.current_inst = new_state.currentInstrument;
		if (drum_hit[sequencer.current_inst].solo) {
			clear_mutes();
			for (uint8_t k = 0; k < NUM_INST; ++k) {drum_hit[k].solo = false;}
		}
	}
	update_inst_led_mask();
}

/* copy one part to the other (both variations). Sets the length, does not mark the pattern as edited. */
void copy_part(uint8_t part) {
	uint8_t source = (part == SECOND) ? SECOND : FIRST;
	uint8_t destination = source ^ 1;
	for (uint8_t v = 0; v < 2; v++) {
		sequencer.pattern[v].accent[destination] = sequencer.pattern[v].accent[source];
		memcpy(sequencer.pattern[v].part[destination], sequencer.pattern[v].part[source], sizeof(sequencer.pattern[v].part[source]));
	}
	sequencer.step_num[destination] = sequencer.step_num[source];
}
