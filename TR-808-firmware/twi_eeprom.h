/*
 * twi_eeprom.h
 *
 * Created: 2016-06-12 1:48:20 PM
 *  Author: jeff
 *
 * 880 Perform EEPROM map (24C512, 64 KB, 128 byte pages):
 *   0x0000 - 0xBFFF  12 banks x 16 patterns x 256 bytes (pattern_data, unchanged format)
 *   0xC000 + 13      recall data (same address as the original firmware)
 *   0xC100 - 0xC1FF  reload backup of the pattern that was selected last ("bank 12, pattern 1")
 *   0xC800 - 0xF7FF  pattern attributes (odds, ratchets, direction), 64 bytes per pattern
 *   0xF800 - 0xF83F  reload backup attributes
 *   0xF840 - 0xF87F  mute scenes
 * The area of the removed rhythm tracks (0xC001 - 0xC5FF) is otherwise unused.
 */


#ifndef TWI_EEPROM_H
#define TWI_EEPROM_H

#include "sequencer.h"
#define EEPROM_DEVICE_ID 0b1010000

#define PAGE_SIZE 128 //EEPROM page size (128 byte pages, 512 kilobit EEPROM)
#define PAGES_PER_PATTERN (uint16_t)((sizeof(pattern_data)/PAGE_SIZE) + 1)//need to know how many pages there are per pattern for addressing EEPROM memory
#define BANK_SIZE PAGES_PER_PATTERN*PAGE_SIZE*16 //16 patterns in a bank
#define NUM_PAGES_PATTERN (int)(sizeof(pattern_data) / PAGE_SIZE)
#define REMAINING_PATTERN (int)(sizeof(pattern_data) % PAGE_SIZE)

//same address as before (the old macro expanded to NUM_BANKS*BANK_SIZE + 1*NUM_TRACKS + 1)
#define RECALL_ADDR_OFFSET (NUM_BANKS*BANK_SIZE + 12 + 1)

//reload backup slot, addressed like a pattern in "bank 12" (after the last real bank)
#define BACKUP_BANK 12
#define BACKUP_PATTERN 1

#define ATTR_ADDR_BASE 0xC800
#define ATTR_SLOT_BACKUP (NUM_BANKS*PATTERNS_PER_BANK)
#define SCENE_ADDR 0xF840
#define NUM_SCENES 4

//Define format of eeprom pattern data block:
typedef struct {
	struct pattern variation_a;
	struct pattern variation_b;
	uint8_t pre_scale;
	uint8_t step_num[NUM_PARTS];
	uint8_t shuffle:3;

} pattern_data;

extern uint16_t mute_scene[NUM_SCENES];

void eeprom_init();
pattern_data read_pattern(uint16_t memory_address, uint8_t bank);
void start_write_current_pattern(uint8_t pattern_num, uint8_t pattern_bank);
void write_next_pattern_page();
void write_pattern_page(uint16_t memory_address, uint8_t bank, pattern_data *w_data, int page_number);
void eeprom_read_recall_data();
void eeprom_write_recall_data();
void handle_TWI_result(uint8_t return_code);

/* 880 Perform storage */
uint8_t attr_slot(uint8_t pattern, uint8_t bank);
void attr_read(uint8_t slot, struct attr_block *a); //blocking
void attr_write(uint8_t slot, const struct attr_block *a); //blocking
void attr_queue_current(void); //snapshot pattern_attrs of the current pattern for the background writer
uint8_t pattern_writer_busy(void);
void storage_service(void); //call every refresh: performs at most one background EEPROM operation
uint8_t storage_idle(void);
void storage_queue_backup(void); //copy the current (stored) pattern into the reload backup slot
uint8_t storage_reload(void); //copy the reload backup into RAM, returns 0 if there is none
uint8_t storage_queue_copy(uint8_t src_pattern, uint8_t src_bank, uint8_t dst_pattern, uint8_t dst_bank);
uint8_t storage_queue_clear(uint8_t dst_pattern, uint8_t dst_bank);
void scenes_read(void);
void scene_write(uint8_t scene);
void clear_all_patterns(void);
void default_pattern_data(pattern_data *p);

#endif
