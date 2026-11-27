/*
 * twi_eeprom.c
 *
 * Created: 2016-06-12 12:18:57 PM
 *  Author: jeff
 */ 
// hacked from: http://www.nerdkits.com/forum/thread/1423/

#include <avr/interrupt.h>
#include <stdbool.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <avr/wdt.h>
#include "sequencer.h"
#include "clock.h"
#include "ui.h"
#include <util/atomic.h>
#include "twi_eeprom.h"
#include "twi.h"
#include "hardware.h"


// prototype local functiosn
//typedef struct pattern_data pattern_data;
//struct pattern_data eeprom_pattern;

//PATTERN PATTERN;
// -----------------------------------------------------------------


// Define eeprom commands according to 24CXXX datasheet
typedef struct {
	uint8_t			high_byte;
	uint8_t			low_byte;
	pattern_data	pattern_data;
} WRITE_PATTERN;

typedef struct {
	uint8_t     high_byte;
	uint8_t     low_byte;
} SET_EEPROM_ADDRESS;


// Create structure pointers for the TWI/I2C buffer
WRITE_PATTERN            *p_write_pattern;
SET_EEPROM_ADDRESS      *p_set_eeprom_address;
pattern_data             *p_read_pattern;

// Create TWI/I2C buffer, size to largest command
char    TWI_buffer[sizeof(WRITE_PATTERN)];

void copy_active_pattern_data(pattern_data *out);

void eeprom_init(){
	// Specify startup parameters for the TWI/I2C driver
	TWI_init(   F_CPU,           // clock frequency
	800000L,                    // desired TWI/IC2 bitrate
	TWI_buffer,                 // pointer to comm buffer
	sizeof(TWI_buffer),         // size of comm buffer
	&handle_TWI_result          // pointer to callback function
	);
	
	// Enable interrupts
	//sei();
	
	// Set our structure pointers to the TWI/I2C buffer
	p_set_eeprom_address = (SET_EEPROM_ADDRESS *)TWI_buffer;
	
	p_write_pattern = (WRITE_PATTERN *)TWI_buffer;
	p_read_pattern = (pattern_data *)TWI_buffer;
	
	//p_write_recall = (WRITE_RECALL *)TWI_buffer;
	//p_read_recall_data = (recall *)TWI_buffer;
	
}

// This code copies the current pattern to write into global memory, and then does the copy one page at a time with calls to write_next_pattern_page, which happens in the 
// main sequencer.
pattern_data pattern_to_write;
#define PATTERN_PAGE_STOP_WRITING (NUM_PAGES_PATTERN + 1)
volatile int current_pattern_page = PATTERN_PAGE_STOP_WRITING;
int current_writing_page = -1;
uint8_t current_pattern_num;
uint8_t current_pattern_bank;

void write_next_pattern_page();

/* read a pattern into *out (by pointer: pattern_data is 140 bytes, keep it off the stack) */
void read_pattern_into(pattern_data *out, uint16_t memory_address, uint8_t bank){
	// the requested pattern may still be in the background writer: return that copy
	if (pattern_writer_busy() && bank == current_pattern_bank && memory_address == current_pattern_num*PAGES_PER_PATTERN*PAGE_SIZE) {
		if (out != &pattern_to_write) *out = pattern_to_write;
		return;
	}
	// send 'set memory address' command to eeprom and then read data
	while(TWI_busy);
	memory_address = memory_address + (bank*BANK_SIZE);
	p_set_eeprom_address->high_byte = (memory_address >> 8);
	p_set_eeprom_address->low_byte = memory_address;
	TWI_master_start_write_then_read(   EEPROM_DEVICE_ID,               // device address of eeprom chip
	sizeof(SET_EEPROM_ADDRESS),     // number of bytes to write
	sizeof(pattern_data)             // number of bytes to read
	);
	// nothing else to do - wait for the data
	while(TWI_busy);
	memcpy(out, TWI_buffer, sizeof(pattern_data));
}

static void pager_start(uint8_t pattern_num, uint8_t pattern_bank) {
	current_pattern_bank = pattern_bank;
	current_pattern_num = pattern_num;
	current_writing_page = -1;
	current_pattern_page = 0;
}

uint8_t pattern_writer_busy(void) {
	return current_pattern_page <= NUM_PAGES_PATTERN;
}

static void pager_flush(void) {
	while (pattern_writer_busy()) {
		write_next_pattern_page();
	}
}

void copy_active_pattern_data(pattern_data *out) {
	out->variation_a = sequencer.pattern[VAR_A];
	out->variation_b = sequencer.pattern[VAR_B];
	out->step_num[FIRST] = sequencer.step_num[FIRST];
	out->step_num[SECOND] = sequencer.step_num[SECOND];
	out->pre_scale = sequencer.pre_scale;
	out->shuffle = sequencer.shuffle_amount;
}

void start_write_current_pattern(uint8_t pattern_num, uint8_t pattern_bank) {
	// This is the check how many times writing is getting called. 
	/*static int called_times = 0;
	if (called_times == 10) {
		while(1);
	} else {
		called_times++;
	}
	*/
	
	//TRIGGER_OUT |= (1<<TRIGGER_OUT_2); //holy jeeze, I think I left this here just to measure pattern write times. good golly
	pager_flush(); //never overwrite the buffer while a previous pattern is still being written
	copy_active_pattern_data(&pattern_to_write);
	pager_start(pattern_num, pattern_bank);
	//TRIGGER_OUT &= TRIGGER_OFF;
}

void write_next_pattern_page() {
	if (current_pattern_page <= NUM_PAGES_PATTERN && current_pattern_page != current_writing_page) {
		current_writing_page = current_pattern_page;
		write_pattern_page(current_pattern_num*PAGES_PER_PATTERN*PAGE_SIZE, current_pattern_bank, &pattern_to_write, current_pattern_page);
	}
}


void write_pattern_page(uint16_t memory_address, uint8_t bank, pattern_data *w_data, int page_number){ //this writes whole pattern, including step number for each part and pre scale
	while(TWI_busy);
	memory_address = memory_address + (bank*BANK_SIZE);
	memory_address += page_number*PAGE_SIZE;
	int bytes_to_write = PAGE_SIZE;
	if (page_number == NUM_PAGES_PATTERN) {
		bytes_to_write = REMAINING_PATTERN;
	}
	
	p_write_pattern->high_byte = (memory_address >> 8);
	p_write_pattern->low_byte = memory_address;
	// this is directly putting bytes_to_write bytes of the input PATTERN w_data into the TWI_buffer *after* the address bytes (hence the +2)
	memcpy(TWI_buffer+2, (char *)w_data + page_number*PAGE_SIZE, bytes_to_write);
	TWI_master_start_write_pattern(     EEPROM_DEVICE_ID,       // device address of eeprom chip
	2 + bytes_to_write
	);
	//880 Perform: no waiting here. The transfer (and the NACK retries during the EEPROM write cycle)
	//runs in the TWI interrupt; every other TWI user waits for TWI_busy before touching the buffer.
}

void eeprom_read_recall_data() {
	// send 'set memory address' command to eeprom and then read data
	while(TWI_busy);
	uint16_t memory_address = RECALL_ADDR_OFFSET;
	p_set_eeprom_address->high_byte = (memory_address >> 8);
	p_set_eeprom_address->low_byte = memory_address;
	TWI_master_start_write_then_read(   EEPROM_DEVICE_ID,               // device address of eeprom chip
	sizeof(SET_EEPROM_ADDRESS),     // number of bytes to write
	sizeof(recall)             // number of bytes to read
	);
	
	// nothing else to do - wait for the data
	while(TWI_busy);
	
	struct recall* d = (struct recall*)p_read_pattern;

	sequencer.pattern_bank = sequencer.new_bank = d->bank;
	sequencer.midi_channel = d->midi_channel;
	sequencer.current_pattern = sequencer.new_pattern = d->pattern;
	sequencer.clock_mode = d->clock_mode;
	sequencer.din_reset_enable = d->din_reset_enable;
}

void eeprom_write_recall_data() {
	struct recall recall_data = {0};
	recall_data.bank = sequencer.pattern_bank; 
	recall_data.midi_channel = sequencer.midi_channel;
	recall_data.pattern = sequencer.current_pattern;	
	recall_data.clock_mode = sequencer.clock_mode;
	recall_data.din_reset_enable = sequencer.din_reset_enable;
	// if you want to store more data, put it here
	
	while(TWI_busy);
	// assert(sizeof(rhythm_track_data) <= PAGE_SIZE);
	uint16_t memory_address = RECALL_ADDR_OFFSET;
	//write 128 byte pages of data
	p_write_pattern->high_byte = (memory_address >> 8);
	p_write_pattern->low_byte = memory_address;
	
	memcpy(TWI_buffer+2, (char *)&recall_data, sizeof(recall_data));
	TWI_master_start_write(     EEPROM_DEVICE_ID,       // device address of eeprom chip
	2 + sizeof(recall_data)
	);
	while(TWI_busy);//added by omar to protect twi buffer
	
}

// optional callback function for TWI/I2C driver
// We only have special handling for pattern writing
void handle_TWI_result(uint8_t return_code){
	//flag.twi_init_error = 0;
	if (TWI_operation == TWI_OP_WRITE_PATTERN) {
		if(return_code==TWI_success){
			if (current_pattern_page == current_writing_page) {
				current_pattern_page++;
			}
			//flag.twi_init_error = 1;
			//turn_on(IF_VAR_B_LED);
		}
		// a NACK (EEPROM still busy with its internal write cycle) is retried by the
		// TWI interrupt automatically, so failures are not treated as the end of the transfer

	}
}


/* ======================================================================
 * 880 Perform storage: attributes, reload backup, copy/clear jobs, scenes
 * ====================================================================== */

uint16_t mute_scene[NUM_SCENES];

static struct attr_block attr_to_write; //snapshot for the background writer, also used as job buffer
static uint8_t attr_write_slot;
static uint8_t attr_write_pending = 0;

enum job_type { JOB_NONE, JOB_COPY, JOB_CLEAR };
static struct {
	uint8_t type;
	uint8_t stage;
	uint8_t src_pattern, src_bank, dst_pattern, dst_bank;
} job;

uint8_t attr_slot(uint8_t pattern, uint8_t bank) {
	if (bank == BACKUP_BANK) return ATTR_SLOT_BACKUP;
	return bank * PATTERNS_PER_BANK + pattern;
}

static uint16_t attr_addr(uint8_t slot) {
	return ATTR_ADDR_BASE + (uint16_t)slot * ATTR_BYTES;
}

static void eeprom_read_block_blocking(uint16_t addr, void *dst, uint8_t n) {
	while(TWI_busy);
	p_set_eeprom_address->high_byte = addr >> 8;
	p_set_eeprom_address->low_byte = addr;
	TWI_master_start_write_then_read(EEPROM_DEVICE_ID, sizeof(SET_EEPROM_ADDRESS), n);
	while(TWI_busy);
	memcpy(dst, TWI_buffer, n);
}

//n must not cross a 128 byte EEPROM page
static void eeprom_write_block_blocking(uint16_t addr, const void *src, uint8_t n) {
	while(TWI_busy);
	p_write_pattern->high_byte = addr >> 8;
	p_write_pattern->low_byte = addr;
	memcpy(TWI_buffer + 2, src, n);
	TWI_master_start_write(EEPROM_DEVICE_ID, 2 + n);
	while(TWI_busy);
}

void attr_read(uint8_t slot, struct attr_block *a) {
	if (attr_write_pending && slot == attr_write_slot) {
		*a = attr_to_write; //newest data has not been written yet
		return;
	}
	eeprom_read_block_blocking(attr_addr(slot), a, ATTR_BYTES);
	attr_validate(a);
}

void attr_write(uint8_t slot, const struct attr_block *a) {
	eeprom_write_block_blocking(attr_addr(slot), a, ATTR_BYTES);
}

/* EEPROM write cycle: a read right after a write would wait up to 5 ms (NACK retries).
 * Background reads are only started 20 ms after the last background write. */
static uint16_t last_write_t;
static uint16_t ticks_now(void) {
	uint16_t t;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = ui_ticks; }
	return t;
}
static void mark_write(void) { last_write_t = ticks_now(); }
static uint8_t read_settled(void) { return (uint16_t)(ticks_now() - last_write_t) >= 2; }

/* start a write and return; completion (and retries) happen in the TWI interrupt */
static void attr_write_async(uint8_t slot, const struct attr_block *a) {
	while(TWI_busy);
	uint16_t addr = attr_addr(slot);
	p_write_pattern->high_byte = addr >> 8;
	p_write_pattern->low_byte = addr;
	memcpy(TWI_buffer + 2, a, ATTR_BYTES);
	TWI_master_start_write(EEPROM_DEVICE_ID, 2 + ATTR_BYTES);
}

void attr_queue_current(void) {
	if (attr_write_pending && attr_write_slot != attr_slot(sequencer.current_pattern, sequencer.pattern_bank)) {
		attr_write(attr_write_slot, &attr_to_write); //a different pattern is still queued: write it first
	}
	attr_to_write = pattern_attrs;
	attr_write_slot = attr_slot(sequencer.current_pattern, sequencer.pattern_bank);
	attr_write_pending = 1;
}

void default_pattern_data(pattern_data *p) {
	memset(p, 0, sizeof(*p));
	p->step_num[FIRST] = 15;
	p->step_num[SECOND] = NO_STEPS;
	p->pre_scale = 1; //PRE_SCALE_3
}

/* the reload backup is only used when it belongs to the pattern that is current */
static uint8_t backup_valid = 0, backup_wanted = 0, backup_pattern, backup_bank;

/* is there a backup (made or being made) of this pattern? */
uint8_t storage_backup_for(uint8_t pattern, uint8_t bank) {
	return (backup_valid || backup_wanted) && backup_pattern == pattern && backup_bank == bank;
}

uint8_t storage_idle(void) {
	return !pattern_writer_busy() && !attr_write_pending && job.type == JOB_NONE && !backup_wanted;
}

static uint8_t queue_job(uint8_t type, uint8_t sp, uint8_t sb, uint8_t dp, uint8_t db) {
	if (job.type != JOB_NONE) return 0;
	job.type = type;
	job.stage = 0;
	job.src_pattern = sp;
	job.src_bank = sb;
	job.dst_pattern = dp;
	job.dst_bank = db;
	return 1;
}

uint8_t storage_queue_copy(uint8_t src_pattern, uint8_t src_bank, uint8_t dst_pattern, uint8_t dst_bank) {
	return queue_job(JOB_COPY, src_pattern, src_bank, dst_pattern, dst_bank);
}

uint8_t storage_queue_clear(uint8_t dst_pattern, uint8_t dst_bank) {
	return queue_job(JOB_CLEAR, 0, 0, dst_pattern, dst_bank);
}


void storage_queue_backup(void) {
	backup_valid = 0;
	backup_wanted = 1;
	backup_pattern = sequencer.current_pattern;
	backup_bank = sequencer.pattern_bank;
}

static void start_backup(void) {
	if (job.type != JOB_NONE) return; //retried when the user copy/clear job is done
	//the stored version must be current: unsaved changes would end up in the backup otherwise
	if (backup_pattern != sequencer.current_pattern || backup_bank != sequencer.pattern_bank) {
		backup_wanted = 0; //pattern changed again before the backup could be made
		return;
	}
	queue_job(JOB_COPY, backup_pattern, backup_bank, BACKUP_PATTERN, BACKUP_BANK);
	backup_wanted = 0;
}

/* one EEPROM operation per call so the sequencer never stalls for long */
void storage_service(void) {
	if (TWI_busy) return; //previous transfer still running: never wait in the main loop
	if (pattern_writer_busy()) {
		write_next_pattern_page();
		mark_write();
		return;
	}
	if (attr_write_pending) {
		mark_write();
		attr_write_async(attr_write_slot, &attr_to_write);
		attr_write_pending = 0;
		return;
	}
	if (job.type == JOB_NONE) {
		if (backup_wanted) start_backup();
		return;
	}
	switch (job.stage) {
		case 0: //pattern data into the pager buffer
			if (job.type == JOB_COPY) {
				if (!read_settled()) return;
				read_pattern_into(&pattern_to_write, job.src_pattern*PAGES_PER_PATTERN*PAGE_SIZE, job.src_bank);
			} else {
				default_pattern_data(&pattern_to_write);
			}
			pager_start(job.dst_pattern, job.dst_bank);
			job.stage = 1;
			break;
		case 1: //pager finished (checked above): attributes
			if (job.type == JOB_COPY) {
				if (!read_settled()) return;
				eeprom_read_block_blocking(attr_addr(attr_slot(job.src_pattern, job.src_bank)), &attr_to_write, ATTR_BYTES);
				attr_validate(&attr_to_write);
			} else {
				attr_clear(&attr_to_write);
			}
			attr_write_async(attr_slot(job.dst_pattern, job.dst_bank), &attr_to_write);
			mark_write();
			if (job.dst_bank == BACKUP_BANK) {
				backup_valid = (job.src_pattern == backup_pattern && job.src_bank == backup_bank);
			} else if (job.dst_pattern == backup_pattern && job.dst_bank == backup_bank) {
				backup_valid = 0; //the backed up pattern was overwritten by a copy/clear
			}
			job.type = JOB_NONE;
			break;
	}
}

uint8_t storage_reload(void) {
	if (backup_pattern != sequencer.current_pattern || backup_bank != sequencer.pattern_bank) return 0; //e.g. a fill is playing
	while (!storage_idle() || backup_wanted) { //make sure the backup is complete
		storage_service();
		if (backup_pattern != sequencer.current_pattern || backup_bank != sequencer.pattern_bank) return 0;
	}
	if (!backup_valid) return 0;
	pattern_data *pp = &pattern_to_write; //storage is idle here: the writer buffer is free
	read_pattern_into(pp, BACKUP_PATTERN*PAGES_PER_PATTERN*PAGE_SIZE, BACKUP_BANK);
	#define p (*pp)
	if (p.step_num[FIRST] > 15) return 0; //never written (erased EEPROM)
	sequencer.pattern[VAR_A] = p.variation_a;
	sequencer.pattern[VAR_B] = p.variation_b;
	sequencer.step_num[FIRST] = p.step_num[FIRST];
	sequencer.step_num[SECOND] = (p.step_num[SECOND] == NO_STEPS) ? NO_STEPS : p.step_num[FIRST];
	sequencer.step_num_new = sequencer.step_num[FIRST];
	if (p.shuffle < SHUFFLE_MAX) {
		sequencer.new_shuffle_amount = p.shuffle;
		flag.shuffle_change = 1;
	}
	#undef p
	attr_read(ATTR_SLOT_BACKUP, &pattern_attrs);
	return 1;
}

void scenes_read(void) {
	eeprom_read_block_blocking(SCENE_ADDR, mute_scene, sizeof(mute_scene));
}

void scene_write(uint8_t scene) {
	(void)scene;
	eeprom_write_block_blocking(SCENE_ADDR, mute_scene, sizeof(mute_scene));
}

// This function will clear every pattern by blocking while it writes to each page. It should
// only be run in special situations (like resetting the system).
void clear_all_patterns(void) {
	wdt_disable(); //disable watchdog timer when clearing all patters as it takes longer than 1 second
	default_pattern_data(&pattern_to_write);
	struct attr_block empty;
	attr_clear(&empty);
	for (uint8_t bank = 0; bank < NUM_BANKS; bank++) {
		for (uint8_t pattern = 0; pattern < PATTERNS_PER_BANK; pattern++) {
			for (int page_num = 0; page_num <= NUM_PAGES_PATTERN; ++page_num) {
				write_pattern_page(pattern*PAGES_PER_PATTERN*PAGE_SIZE, bank, &pattern_to_write, page_num);
			}
			attr_write(attr_slot(pattern, bank), &empty);
		}
	}
	attr_write(ATTR_SLOT_BACKUP, &empty);
	sequencer.midi_channel = sequencer.current_pattern = sequencer.new_pattern = 0;
	sequencer.pattern_bank = sequencer.new_bank = 0;
	sequencer.clock_mode = MIDI_MASTER;
	eeprom_write_recall_data(); //initialize recall data in eeprom
	wdt_enable(WDTO_1S);
}

/* finish a queued copy/clear into this pattern before it is loaded */
void storage_finish_for(uint8_t pattern, uint8_t bank) {
	if (job.type != JOB_NONE && job.dst_pattern == pattern && job.dst_bank == bank) {
		while (job.type != JOB_NONE) storage_service();
	}
}
