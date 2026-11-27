/*
 * logic.c
 * Pure sequencer logic for the 880 Perform firmware (see logic.h).
 */
#include <string.h>
#include "logic.h"

uint8_t dir_map(uint8_t dir, uint8_t pos, uint8_t last, uint8_t pass, uint8_t rnd) {
	if (pos > last) pos = last;
	switch (dir) {
		case DIR_REVERSE:
			return last - pos;
		case DIR_PINGPONG:
			return (pass & 1) ? last - pos : pos;
		case DIR_RANDOM:
			return rnd % (uint8_t)(last + 1);
		default:
			return pos;
	}
}

uint8_t lfsr_next(uint8_t s) {
	uint8_t lsb = s & 1;
	s >>= 1;
	if (lsb) s ^= 0xB8;
	return s;
}

uint8_t popcount16(uint16_t mask) {
	uint8_t n = 0;
	while (mask) {
		mask &= mask - 1;
		n++;
	}
	return n;
}

uint8_t loop_pick(uint16_t held_mask, uint8_t idx) {
	uint8_t n = popcount16(held_mask);
	if (n == 0) return 0xFF;
	idx %= n;
	for (uint8_t step = 0; step < 16; step++) {
		if (held_mask & (1u << step)) {
			if (idx == 0) return step;
			idx--;
		}
	}
	return 0xFF;
}

uint8_t chain_next(uint8_t current, uint8_t start, uint8_t end) {
	uint8_t lo = start < end ? start : end;
	uint8_t hi = start < end ? end : start;
	if (current < lo || current > hi) return start;
	if (start <= end) {
		return (current >= end) ? start : current + 1;
	}
	/* descending chain */
	return (current <= end) ? start : current - 1;
}

uint8_t tap_decide(uint8_t ppqn_counter, uint8_t divider, uint8_t step_pending) {
	if (step_pending) return TAP_CURRENT_PENDING;
	/* first half of the step belongs to the step that just played */
	if ((uint16_t)ppqn_counter * 2 < divider) return TAP_CURRENT_SOUNDED;
	return TAP_NEXT;
}

/* ---------- attributes ---------- */

void attr_clear(struct attr_block *a) {
	memset(a, 0, sizeof(*a));
	a->magic = ATTR_MAGIC;
}

uint8_t attr_count(const struct attr_block *a) {
	return a->info & 0x1F;
}

static void attr_set_count(struct attr_block *a, uint8_t n) {
	a->info = (a->info & ~0x1F) | (n & 0x1F);
}

uint8_t attr_direction(const struct attr_block *a) {
	return (a->info >> 5) & 0x03;
}

void attr_set_direction(struct attr_block *a, uint8_t dir) {
	a->info = (a->info & 0x1F) | ((dir & 0x03) << 5);
}

uint8_t attr_validate(struct attr_block *a) {
	if (a->magic != ATTR_MAGIC || attr_count(a) > ATTR_MAX || (a->info & 0x80)) {
		attr_clear(a);
		return 0;
	}
	for (uint8_t i = 0; i < attr_count(a); i++) {
		if (a->entry[i] & 0xC000) {
			attr_clear(a);
			return 0;
		}
		for (uint8_t j = i + 1; j < attr_count(a); j++) { //every cell may appear only once
			if ((a->entry[i] & ATTR_KEY_MASK) == (a->entry[j] & ATTR_KEY_MASK)) {
				attr_clear(a);
				return 0;
			}
		}
	}
	return 1;
}

uint16_t attr_key(uint8_t part, uint8_t var, uint8_t step, uint8_t inst) {
	return (uint16_t)(step & 0x0F) | ((uint16_t)(part & 1) << 4) | ((uint16_t)(var & 1) << 5) | ((uint16_t)(inst & 0x0F) << 6);
}

static int8_t attr_find(const struct attr_block *a, uint16_t key) {
	for (uint8_t i = 0; i < attr_count(a); i++) {
		if ((a->entry[i] & ATTR_KEY_MASK) == key) return i;
	}
	return -1;
}

uint8_t attr_get_odds(const struct attr_block *a, uint16_t key) {
	int8_t i = attr_find(a, key);
	if (i < 0) return 1;
	return ((a->entry[i] >> 10) & 0x03) + 1;
}

uint8_t attr_get_ratchet(const struct attr_block *a, uint16_t key) {
	int8_t i = attr_find(a, key);
	if (i < 0) return 1;
	return ((a->entry[i] >> 12) & 0x03) + 1;
}

static void attr_delete_at(struct attr_block *a, uint8_t i) {
	uint8_t n = attr_count(a);
	for (; i + 1 < n; i++) a->entry[i] = a->entry[i + 1];
	a->entry[n - 1] = 0;
	attr_set_count(a, n - 1);
}

uint8_t attr_set(struct attr_block *a, uint16_t key, uint8_t odds, uint8_t ratchet) {
	if (odds < 1) odds = 1;
	if (odds > 4) odds = 4;
	if (ratchet < 1) ratchet = 1;
	if (ratchet > 4) ratchet = 4;
	int8_t i = attr_find(a, key);
	if (odds == 1 && ratchet == 1) {
		if (i >= 0) attr_delete_at(a, i);
		return 1;
	}
	uint16_t value = key | ((uint16_t)(odds - 1) << 10) | ((uint16_t)(ratchet - 1) << 12);
	if (i >= 0) {
		a->entry[i] = value;
		return 1;
	}
	uint8_t n = attr_count(a);
	if (n >= ATTR_MAX) return 0;
	a->entry[n] = value;
	attr_set_count(a, n + 1);
	return 1;
}

void attr_remove_inst(struct attr_block *a, uint8_t var, uint8_t inst) {
	uint8_t i = 0;
	while (i < attr_count(a)) {
		uint16_t e = a->entry[i];
		if (((e >> 5) & 1) == (var & 1) && ((e >> 6) & 0x0F) == inst) {
			attr_delete_at(a, i);
		} else {
			i++;
		}
	}
}

void attr_remove_part(struct attr_block *a, uint8_t part) {
	uint8_t i = 0;
	while (i < attr_count(a)) {
		if (((a->entry[i] >> 4) & 1) == (part & 1)) {
			attr_delete_at(a, i);
		} else {
			i++;
		}
	}
}

void attr_copy_part(struct attr_block *a, uint8_t src, uint8_t dst) {
	if ((src & 1) == (dst & 1)) return;
	attr_remove_part(a, dst);
	uint8_t n = attr_count(a);
	for (uint8_t i = 0; i < n; i++) {
		uint16_t e = a->entry[i];
		if (((e >> 4) & 1) == (src & 1)) {
			uint16_t copy = (e & ~(1u << 4)) | ((uint16_t)(dst & 1) << 4);
			uint8_t c = attr_count(a);
			if (c >= ATTR_MAX) return;
			a->entry[c] = copy;
			attr_set_count(a, c + 1);
		}
	}
}

void attr_copy_var(struct attr_block *a, uint8_t src, uint8_t dst) {
	if ((src & 1) == (dst & 1)) return;
	uint8_t i = 0;
	while (i < attr_count(a)) { //remove the destination entries
		if (((a->entry[i] >> 5) & 1) == (dst & 1)) {
			attr_delete_at(a, i);
		} else {
			i++;
		}
	}
	uint8_t n = attr_count(a);
	for (i = 0; i < n; i++) {
		uint16_t e = a->entry[i];
		if (((e >> 5) & 1) == (src & 1)) {
			uint8_t c = attr_count(a);
			if (c >= ATTR_MAX) return;
			a->entry[c] = (e & ~(1u << 5)) | ((uint16_t)(dst & 1) << 5);
			attr_set_count(a, c + 1);
		}
	}
}

uint16_t attr_inst_steps(const struct attr_block *a, uint8_t part, uint8_t var, uint8_t inst) {
	uint16_t m = 0;
	uint16_t want = attr_key(part, var, 0, inst); //step bits 0
	for (uint8_t i = 0; i < attr_count(a); i++) {
		uint16_t e = a->entry[i];
		if ((e & ATTR_KEY_MASK & ~0x0F) == want) m |= (1u << (e & 0x0F));
	}
	return m;
}

void attr_step_masks(const struct attr_block *a, uint8_t part, uint8_t var, uint8_t step, uint8_t pass,
	uint16_t *odds_skip, uint16_t ratchet[3]) {
	*odds_skip = 0;
	ratchet[0] = ratchet[1] = ratchet[2] = 0;
	uint16_t want = attr_key(part, var, step, 0); //instrument bits 0
	for (uint8_t i = 0; i < attr_count(a); i++) {
		uint16_t e = a->entry[i];
		if ((e & 0x3F) != want) continue; //step, part and variation must match
		uint8_t inst = (e >> 6) & 0x0F;
		uint8_t odds = ((e >> 10) & 0x03) + 1;
		uint8_t rat = ((e >> 12) & 0x03) + 1;
		if (!odds_play(odds, pass)) *odds_skip |= (1u << inst);
		if (rat > 1) ratchet[rat - 2] |= (1u << inst);
	}
}

uint8_t odds_play(uint8_t odds, uint8_t pass) {
	if (odds <= 1) return 1;
	return (pass % odds) == 0;
}

uint8_t ratchet_tick(uint8_t divider, uint8_t n, uint8_t k) {
	if (n < 2) return 0xFF;
	return (uint8_t)(((uint16_t)divider * k) / n);
}
