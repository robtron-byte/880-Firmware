/*
 * mode.h
 * Open808 firmware ATMEGA328PB
 * minisystem
 * system80.net
 */
#ifndef MODE_H
#define MODE_H

#define MODE_LED_MASK 0b11000000 //mask for latch 4
#define FILL_MODE_LATCH_4_LED_MASK 0b00111111
#define FILL_MODE_LATCH_2_LED_MASK 0b11110000
#define NUM_FILL_MODES 6
#define NUM_CLOCK_MODES 5
#define MANUAL 0

/* 880 Perform: MODE toggles EDIT <-> PERFORM. PATTERN CLEAR only with SHIFT + MODE.
 * Values are kept so the mode LEDs (latch 4 bits 0..5) still line up. */
enum global_mode {
	PATTERN_CLEAR,
	FIRST_PART,   //EDIT, 1st part
	SECOND_PART,  //EDIT, 2nd part
	MANUAL_PLAY   //PERFORM
};

#define IS_EDIT(m) ((m) == FIRST_PART || (m) == SECOND_PART)

enum clock_mode {
	MIDI_MASTER,
	MIDI_SLAVE,
	DIN_SYNC_MASTER,
	DIN_SYNC_SLAVE,
	PULSE_SYNC_SLAVE
};

extern uint8_t fill_index;

void set_mode(uint8_t new_mode);
void check_fill_switch(void);
void update_fill_leds(uint8_t fill_index);
void set_clock_mode (uint8_t sync_index);
void update_clock_mode_leds(uint8_t sync_index);

#endif
