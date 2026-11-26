/*
 * sequencer.h
 * Open808 firmware ATMEGA328PB
 * minisystem
 * system80.net
 *
 * 880 Perform: song mode, 32nd sub steps, roll mode, live hits and trigger inputs removed.
 */
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include "mode.h"
#include "drums.h"
#include "logic.h"

//define part indices
#define FIRST 0
#define SECOND 1
#define NUM_PARTS 2

#define NUM_STEPS 16
#define NO_STEPS 16 //null state of part 2 step number: second part is off
#define NUM_BANKS 12 //12 banks of patterns
#define BASIC_RHYTHM 12 //number of basic rhythms
#define PATTERNS_PER_BANK 16

//define pre-scale ppqn dividers
#define NUM_PRE_SCALES 4
//pulses per qarter note for different pre-scales
//8/4/6/3 for 24 ppqn, 32/16/24/12 for 96 ppqn
#define PRE_SCALE_1 32
#define PRE_SCALE_2 16
#define	PRE_SCALE_3 24
#define	PRE_SCALE_4 12

#define SHUFFLE_MIN 0
#define SHUFFLE_MAX 6

//DIN reset enable (stopped, EDIT: SHIFT + TAP held + step 8)
#define DIN_RESET_STEP 7

//step layer used by the PERFORM step keys
#define STEP_LAYER_STEPS 0
#define STEP_LAYER_LOOP 1

//instrument layer used by the PERFORM instrument keys
#define INST_LAYER_MUTE 0
#define INST_LAYER_SELECT 1

enum variation_mode {
	VAR_A,
	VAR_B,
	VAR_AB
};

struct flag {
	uint8_t next_step:1;
	uint8_t half_step:1;
	uint8_t variation_change:1;
	uint8_t trig_finished:1;
	uint8_t pattern_edit:1; //flag if pattern is edited, need to write to eeprom at end of measure
	uint8_t pattern_change:1;
	uint8_t new_measure:1;
	uint8_t pre_scale_change:1;
	uint8_t tap:1;
	uint8_t intro:1; //flag for starting with selected intro pattern in manual play mode
	uint8_t fill:1;
	uint8_t slave_start:1;
	uint8_t slave_stop:1;
	uint8_t delay_slave_start:1; //flag for independent restart while clock slave
	uint8_t wait_for_master_tick:1;
	uint8_t shuffle_step:1; //flag to delay step when shuffle is active
	uint8_t shuffle_change:1; //flag to indicate shuffle amount has changed
	uint8_t blink:1;
	uint8_t din_start:1;
	uint8_t din_stop:1;
	uint8_t din_reset:1;
	uint8_t din_reset_previous:1;
	uint8_t length_change:1; //step length changed in EDIT, applied at the end of the measure
	uint8_t second_clear:1; //second part cleared, applied at the end of the measure
};

struct pattern { //current pattern will be loaded into ram from eeprom. changing pattern will write to eeprom and load next pattern
	uint16_t accent[NUM_PARTS]; // 2 parts of 16 steps of accent data, 2 parts
	uint16_t part[NUM_PARTS][NUM_STEPS]; //2 parts, 16 steps each. thanks to Omar
};

struct recall { //layout kept compatible with the original firmware
	uint8_t bank:4;
	uint8_t pattern:4;
	uint8_t midi_channel:4;
	uint8_t trigger_1; //unused (fixed trigger outs)
	uint8_t trigger_2; //unused
	// when we need to store more things, they should go AFTER this point
	uint8_t trigger_enable:1; //unused
	uint8_t clock_mode:3;
	uint8_t din_reset_enable:1;
};

extern struct recall recall;

struct sequencer {
	enum global_mode mode;
	enum clock_mode clock_mode;
	uint8_t fill_mode:4;
	uint8_t SHIFT:1; //is SHIFT key being held?
	uint8_t START:1; //is sequencer running or not?
	uint8_t CLEAR:1; //is the clear button being held?
	uint8_t TAP_HELD:1; //is the TAP button being held?
	uint8_t ALT:1; //alternative function mode (I/F-VAR held)
	uint8_t primed:1;
	uint8_t shuffle_amount:3;
	uint8_t shuffle_multplier:3; //1 for MIDI and DIN SYNC slave modes, 4 for master modes @ 96 ppqn
	uint8_t new_shuffle_amount:3;
	uint8_t shuffle_ppqn_count:4;//may need to up bit depth for 96ppqn
	struct pattern pattern[2]; //Variation A:0, Variation B: 1
	uint8_t pattern_bank:4; //bank of the pattern in RAM
	uint8_t new_bank:4; //bank of the next pattern (switches together with the pattern)
	uint16_t led_mask;
	uint8_t basic_variation:1; //variation A or variation B
	uint8_t current_variation:1; //variation currently playing
	uint8_t intro_fill_var:1; //intro/fill variation
	enum variation_mode variation_mode; //0 = A, 1 = B, 2 = toggle AB
	uint8_t step_num[NUM_PARTS];
	uint8_t step_num_new:5; //holder to change step number at end of measure
	uint8_t current_step:4; //running position within the part (0..step_num), 4 bits like the original (wraps after 15)
	uint8_t play_step:4; //step that is actually played (direction / step loop applied)
	uint8_t part_playing:1; //0 or 1 first part or second part - will toggle
	uint8_t part_editing:1; //part currently being edited. Determined by mode
	uint8_t pre_scale:2;
	uint8_t current_pattern:4;
	uint8_t new_pattern:4;//pattern that plays after the current measure
	uint8_t current_intro_fill:4;
	uint8_t current_measure_auto_fill:6; //counter used for counting measures for AUTO FILL INs
	enum drum current_inst; //this is index of drum_hit struct
	uint8_t var_led_mask;
	uint8_t midi_channel:4;
	uint8_t version; //firmware version
	uint8_t din_reset_enable:1;
	/* 880 Perform */
	uint8_t pass_count; //full pattern passes, used for odds and ping-pong
	uint8_t rnd; //random direction state
	uint8_t chain_start:4;
	uint8_t chain_end:4;
	uint8_t chain_active:1;
	uint8_t recording:1; //live recording in PERFORM
	uint8_t second_untouched:1; //2nd part was auto-copied and not edited yet
	uint8_t step_layer:1; //PERFORM step keys: STEPS or LOOP
	uint8_t inst_layer:1; //PERFORM instrument keys: MUTE or SELECT
	uint8_t pattern_layer:1; //step keys select patterns
	uint8_t loop_idx;
	uint16_t loop_mask; //held step keys in LOOP layer
	uint16_t skip_mask; //instruments not to trigger on the next step (already sounded live while recording)
};

extern struct sequencer sequencer;
extern volatile struct flag flag;
extern struct attr_block pattern_attrs; //odds/ratchets/direction of the pattern in RAM
extern uint8_t pre_scale[4];
extern uint8_t reload_pending; //SHIFT + TAP while running: reload at the end of the measure
extern uint8_t recall_dirty; //last pattern/bank must be stored in the recall data

void update_tempo(void);
void process_tick(void);
void process_step(void);
void process_start(void);
void process_stop(void);
void process_new_measure(void);
void update_fill(void);
void update_shuffle(uint8_t shuffle_amount);
void toggle_variation(void);
void read_next_pattern(uint8_t pattern_num, uint8_t pattern_bank);
void pattern_load(uint8_t pattern_num, uint8_t pattern_bank); //load now (stopped) and make it current
void pattern_request(uint8_t pattern_num, uint8_t pattern_bank); //at the end of the measure (running) or now (stopped)
void show_version_steps(void);
void service_ratchets(void);
void second_part_autocopy(void);
void second_part_leave(void);
void clear_current_pattern(void);
void clear_current_instrument(void);
void apply_length_changes(void);
void copy_variation_a_to_b(void);

#endif
