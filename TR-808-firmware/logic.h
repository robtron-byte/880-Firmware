/*
 * logic.h
 * Pure sequencer logic for the 880 Perform firmware.
 * No hardware access - compiled into the firmware and into the host unit tests.
 */
#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>

/* ---------- playback direction ---------- */
enum direction {
	DIR_FORWARD,
	DIR_REVERSE,
	DIR_PINGPONG, /* forward on even passes, backward on odd passes */
	DIR_RANDOM
};

/* Map the running position (0..last) to the step that is played. */
uint8_t dir_map(uint8_t dir, uint8_t pos, uint8_t last, uint8_t pass, uint8_t rnd);

/* 8 bit Galois LFSR, never returns 0 when seeded with non-zero value */
uint8_t lfsr_next(uint8_t s);

/* ---------- step loop (TR-6S style) ---------- */
/* number of set bits in a 16 bit mask */
uint8_t popcount16(uint16_t mask);
/* idx-th (modulo count) held step in ascending order, 0xFF if mask empty */
uint8_t loop_pick(uint16_t held_mask, uint8_t idx);

/* ---------- pattern chain ---------- */
/* next pattern of a range chain start..end (start may be > end: wraps the other way round) */
uint8_t chain_next(uint8_t current, uint8_t start, uint8_t end);

/* ---------- tap write / live recording quantisation ---------- */
enum tap_target {
	TAP_CURRENT_SOUNDED,  /* round to the step that already played: write + trigger now */
	TAP_CURRENT_PENDING,  /* step boundary passed but step not triggered yet: just write */
	TAP_NEXT              /* round to the upcoming step: just write */
};
uint8_t tap_decide(uint8_t ppqn_counter, uint8_t divider, uint8_t step_pending);

/* ---------- per step attributes: odds and ratchets ----------
 * Sparse list stored per pattern (64 bytes in EEPROM):
 *   byte 0: ATTR_MAGIC
 *   byte 1: bits 0-4 count, bits 5-6 direction
 *   bytes 2..63: up to ATTR_MAX entries, 2 bytes each (little endian uint16):
 *     bits 0-3 step, bit 4 part, bit 5 variation, bits 6-9 instrument (BD..MA),
 *     bits 10-11 odds-1 (play every 1st..4th pass), bits 12-13 ratchet-1 (1..4 hits)
 */
#define ATTR_MAGIC 0xA5
#define ATTR_MAX 31
#define ATTR_BYTES 64

struct attr_block {
	uint8_t magic;
	uint8_t info;
	uint16_t entry[ATTR_MAX];
};

#define ATTR_KEY_MASK 0x03FF

void attr_clear(struct attr_block *a);
/* validate a block read from EEPROM, reset it if it is garbage/erased. returns 1 if it was valid */
uint8_t attr_validate(struct attr_block *a);
uint8_t attr_count(const struct attr_block *a);
uint8_t attr_direction(const struct attr_block *a);
void attr_set_direction(struct attr_block *a, uint8_t dir);
uint16_t attr_key(uint8_t part, uint8_t var, uint8_t step, uint8_t inst);
/* odds (1..4) and ratchet (1..4) for a cell, 1/1 if no entry */
uint8_t attr_get_odds(const struct attr_block *a, uint16_t key);
uint8_t attr_get_ratchet(const struct attr_block *a, uint16_t key);
/* set values (1..4). entry removed when both are 1. returns 0 if the list is full */
uint8_t attr_set(struct attr_block *a, uint16_t key, uint8_t odds, uint8_t ratchet);
/* remove every entry of an instrument in one variation (both parts) */
void attr_remove_inst(struct attr_block *a, uint8_t var, uint8_t inst);
/* remove every entry of a part (both variations) */
void attr_remove_part(struct attr_block *a, uint8_t part);
/* copy all entries of variation `src` to variation `dst` (dst entries are replaced) */
void attr_copy_var(struct attr_block *a, uint8_t src, uint8_t dst);
/* copy all entries of part `src` to part `dst` (dst entries are replaced) */
void attr_copy_part(struct attr_block *a, uint8_t src, uint8_t dst);

/* does a step with the given odds play on this pass? */
uint8_t odds_play(uint8_t odds, uint8_t pass);
/* ppqn tick (0..divider-1) of the k-th ratchet hit (k = 1..n-1) */
uint8_t ratchet_tick(uint8_t divider, uint8_t n, uint8_t k);

#endif
