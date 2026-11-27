/* Host unit tests for logic.c (run with run_tests.sh) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "logic.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- tap / record quantisation ---- */
void test_tap_rounding_all_prescales(void) {
	const uint8_t dividers[4] = {12, 16, 24, 32};
	for (uint8_t d = 0; d < 4; d++) {
		uint8_t div = dividers[d];
		for (uint8_t t = 0; t < div; t++) {
			uint8_t r = tap_decide(t, div, 0);
			if (t * 2 < div) {
				TEST_ASSERT_EQUAL_UINT8(TAP_CURRENT_SOUNDED, r);
			} else {
				TEST_ASSERT_EQUAL_UINT8(TAP_NEXT, r);
			}
		}
		/* exactly half way rounds up to the next step */
		TEST_ASSERT_EQUAL_UINT8(TAP_NEXT, tap_decide(div / 2, div, 0));
		TEST_ASSERT_EQUAL_UINT8(TAP_CURRENT_SOUNDED, tap_decide(div / 2 - 1, div, 0));
	}
}

void test_tap_pending_step(void) {
	/* shuffle delay or step flag not yet processed: the step has not sounded */
	TEST_ASSERT_EQUAL_UINT8(TAP_CURRENT_PENDING, tap_decide(0, 24, 1));
	TEST_ASSERT_EQUAL_UINT8(TAP_CURRENT_PENDING, tap_decide(20, 24, 1));
}

/* ---- step loop ---- */
void test_loop_pick_order(void) {
	uint16_t mask = (1 << 4) | (1 << 5) | (1 << 7);
	TEST_ASSERT_EQUAL_UINT8(4, loop_pick(mask, 0));
	TEST_ASSERT_EQUAL_UINT8(5, loop_pick(mask, 1));
	TEST_ASSERT_EQUAL_UINT8(7, loop_pick(mask, 2));
	TEST_ASSERT_EQUAL_UINT8(4, loop_pick(mask, 3));
	TEST_ASSERT_EQUAL_UINT8(0xFF, loop_pick(0, 0));
	TEST_ASSERT_EQUAL_UINT8(15, loop_pick(0x8000, 9));
	TEST_ASSERT_EQUAL_UINT8(3, popcount16(mask));
}

/* ---- direction ---- */
void test_directions(void) {
	TEST_ASSERT_EQUAL_UINT8(3, dir_map(DIR_FORWARD, 3, 15, 0, 0));
	TEST_ASSERT_EQUAL_UINT8(12, dir_map(DIR_REVERSE, 3, 15, 0, 0));
	TEST_ASSERT_EQUAL_UINT8(0, dir_map(DIR_REVERSE, 11, 11, 0, 0));
	TEST_ASSERT_EQUAL_UINT8(3, dir_map(DIR_PINGPONG, 3, 15, 0, 0));
	TEST_ASSERT_EQUAL_UINT8(12, dir_map(DIR_PINGPONG, 3, 15, 1, 0));
	for (uint16_t r = 0; r < 256; r++) {
		TEST_ASSERT_TRUE(dir_map(DIR_RANDOM, 0, 11, 0, (uint8_t)r) <= 11);
	}
	/* position beyond last is clamped */
	TEST_ASSERT_EQUAL_UINT8(7, dir_map(DIR_FORWARD, 9, 7, 0, 0));
}

void test_lfsr_never_zero_and_cycles(void) {
	uint8_t s = 1, seen = 0;
	for (int i = 0; i < 255; i++) {
		s = lfsr_next(s);
		TEST_ASSERT_TRUE(s != 0);
		if (s == 1) seen++;
	}
	TEST_ASSERT_EQUAL_UINT8(1, seen);
}

/* ---- chain ---- */
void test_chain(void) {
	TEST_ASSERT_EQUAL_UINT8(4, chain_next(3, 3, 6));
	TEST_ASSERT_EQUAL_UINT8(3, chain_next(6, 3, 6));
	TEST_ASSERT_EQUAL_UINT8(3, chain_next(0, 3, 6)); /* outside the range: jump to start */
	TEST_ASSERT_EQUAL_UINT8(5, chain_next(6, 6, 3)); /* descending */
	TEST_ASSERT_EQUAL_UINT8(6, chain_next(3, 6, 3));
	TEST_ASSERT_EQUAL_UINT8(2, chain_next(2, 2, 2));
}

/* ---- attributes ---- */
void test_attr_layout_is_64_bytes(void) {
	TEST_ASSERT_EQUAL_INT(ATTR_BYTES, sizeof(struct attr_block));
}

void test_attr_set_get_remove(void) {
	struct attr_block a;
	attr_clear(&a);
	uint16_t k = attr_key(1, 0, 7, 9);
	TEST_ASSERT_EQUAL_UINT8(1, attr_get_odds(&a, k));
	TEST_ASSERT_EQUAL_UINT8(1, attr_get_ratchet(&a, k));
	TEST_ASSERT_TRUE(attr_set(&a, k, 3, 4));
	TEST_ASSERT_EQUAL_UINT8(3, attr_get_odds(&a, k));
	TEST_ASSERT_EQUAL_UINT8(4, attr_get_ratchet(&a, k));
	TEST_ASSERT_EQUAL_UINT8(1, attr_count(&a));
	/* neighbouring cells untouched */
	TEST_ASSERT_EQUAL_UINT8(1, attr_get_ratchet(&a, attr_key(0, 0, 7, 9)));
	TEST_ASSERT_EQUAL_UINT8(1, attr_get_ratchet(&a, attr_key(1, 1, 7, 9)));
	TEST_ASSERT_EQUAL_UINT8(1, attr_get_ratchet(&a, attr_key(1, 0, 7, 8)));
	/* back to defaults removes the entry */
	TEST_ASSERT_TRUE(attr_set(&a, k, 1, 1));
	TEST_ASSERT_EQUAL_UINT8(0, attr_count(&a));
}

void test_attr_full_list(void) {
	struct attr_block a;
	attr_clear(&a);
	uint8_t added = 0;
	for (uint8_t inst = 0; inst < 16 && added < ATTR_MAX; inst++) {
		for (uint8_t step = 0; step < 16 && added < ATTR_MAX; step++) {
			TEST_ASSERT_TRUE(attr_set(&a, attr_key(0, 0, step, inst), 2, 1));
			added++;
		}
	}
	TEST_ASSERT_EQUAL_UINT8(ATTR_MAX, attr_count(&a));
	TEST_ASSERT_FALSE(attr_set(&a, attr_key(1, 1, 0, 0), 2, 2));
	/* changing an existing entry still works when full */
	TEST_ASSERT_TRUE(attr_set(&a, attr_key(0, 0, 0, 0), 4, 4));
	TEST_ASSERT_EQUAL_UINT8(4, attr_get_ratchet(&a, attr_key(0, 0, 0, 0)));
}

void test_attr_remove_inst_and_part(void) {
	struct attr_block a;
	attr_clear(&a);
	attr_set(&a, attr_key(0, 0, 1, 3), 2, 1);
	attr_set(&a, attr_key(1, 0, 2, 3), 2, 1);
	attr_set(&a, attr_key(0, 1, 3, 3), 2, 1); /* other variation */
	attr_set(&a, attr_key(0, 0, 4, 5), 2, 1); /* other instrument */
	attr_remove_inst(&a, 0, 3);
	TEST_ASSERT_EQUAL_UINT8(2, attr_count(&a));
	TEST_ASSERT_EQUAL_UINT8(2, attr_get_odds(&a, attr_key(0, 1, 3, 3)));
	TEST_ASSERT_EQUAL_UINT8(2, attr_get_odds(&a, attr_key(0, 0, 4, 5)));
	attr_set(&a, attr_key(1, 1, 9, 2), 1, 3);
	attr_remove_part(&a, 1);
	TEST_ASSERT_EQUAL_UINT8(2, attr_count(&a));
}

void test_attr_copy_part(void) {
	struct attr_block a;
	attr_clear(&a);
	attr_set(&a, attr_key(0, 0, 1, 3), 2, 3);
	attr_set(&a, attr_key(1, 1, 5, 4), 4, 1); /* will be replaced */
	attr_copy_part(&a, 0, 1);
	TEST_ASSERT_EQUAL_UINT8(2, attr_count(&a));
	TEST_ASSERT_EQUAL_UINT8(3, attr_get_ratchet(&a, attr_key(1, 0, 1, 3)));
	TEST_ASSERT_EQUAL_UINT8(1, attr_get_odds(&a, attr_key(1, 1, 5, 4)));
}

void test_attr_validate_and_direction(void) {
	struct attr_block a;
	memset(&a, 0xFF, sizeof(a)); /* erased EEPROM */
	TEST_ASSERT_FALSE(attr_validate(&a));
	TEST_ASSERT_EQUAL_UINT8(0, attr_count(&a));
	TEST_ASSERT_EQUAL_UINT8(DIR_FORWARD, attr_direction(&a));
	attr_set_direction(&a, DIR_RANDOM);
	attr_set(&a, attr_key(0, 0, 2, 2), 3, 2);
	TEST_ASSERT_EQUAL_UINT8(DIR_RANDOM, attr_direction(&a));
	TEST_ASSERT_EQUAL_UINT8(1, attr_count(&a));
	TEST_ASSERT_TRUE(attr_validate(&a));
	TEST_ASSERT_EQUAL_UINT8(DIR_RANDOM, attr_direction(&a));
	/* duplicate cells (corrupted block) are rejected */
	a.entry[1] = a.entry[0];
	a.info = (a.info & ~0x1F) | 2;
	TEST_ASSERT_FALSE(attr_validate(&a));
	TEST_ASSERT_EQUAL_UINT8(0, attr_count(&a));
}

void test_attr_copy_var(void) {
	struct attr_block a;
	attr_clear(&a);
	attr_set(&a, attr_key(0, 0, 1, 3), 2, 3);
	attr_set(&a, attr_key(1, 0, 4, 2), 1, 2);
	attr_set(&a, attr_key(0, 1, 9, 9), 4, 1); /* replaced */
	attr_copy_var(&a, 0, 1);
	TEST_ASSERT_EQUAL_UINT8(4, attr_count(&a));
	TEST_ASSERT_EQUAL_UINT8(3, attr_get_ratchet(&a, attr_key(0, 1, 1, 3)));
	TEST_ASSERT_EQUAL_UINT8(2, attr_get_ratchet(&a, attr_key(1, 1, 4, 2)));
	TEST_ASSERT_EQUAL_UINT8(1, attr_get_odds(&a, attr_key(0, 1, 9, 9)));
	TEST_ASSERT_EQUAL_UINT8(3, attr_get_ratchet(&a, attr_key(0, 0, 1, 3)));
}

void test_attr_fast_paths(void) {
	struct attr_block a;
	attr_clear(&a);
	attr_set(&a, attr_key(0, 1, 3, 5), 2, 1);
	attr_set(&a, attr_key(0, 1, 9, 5), 1, 3);
	attr_set(&a, attr_key(0, 1, 3, 7), 1, 4);
	attr_set(&a, attr_key(1, 1, 3, 5), 3, 2); /* other part */
	attr_set(&a, attr_key(0, 0, 3, 5), 4, 2); /* other variation */
	TEST_ASSERT_EQUAL_HEX16((1 << 3) | (1 << 9), attr_inst_steps(&a, 0, 1, 5));
	TEST_ASSERT_EQUAL_HEX16(1 << 3, attr_inst_steps(&a, 0, 1, 7));
	uint16_t om, rm;
	attr_inst_masks(&a, 0, 1, 5, &om, &rm);
	TEST_ASSERT_EQUAL_HEX16(1 << 3, om);
	TEST_ASSERT_EQUAL_HEX16(1 << 9, rm);
	uint16_t skip, rat[3];
	attr_step_masks(&a, 0, 1, 3, 1, &skip, rat); /* pass 1: odds 2 skips */
	TEST_ASSERT_EQUAL_HEX16(1 << 5, skip);
	TEST_ASSERT_EQUAL_HEX16(0, rat[0]);
	TEST_ASSERT_EQUAL_HEX16(0, rat[1]);
	TEST_ASSERT_EQUAL_HEX16(1 << 7, rat[2]);
	attr_step_masks(&a, 0, 1, 3, 2, &skip, rat);
	TEST_ASSERT_EQUAL_HEX16(0, skip);
	/* the fast paths agree with the single lookups for every cell */
	for (uint8_t part = 0; part < 2; part++) for (uint8_t var = 0; var < 2; var++) for (uint8_t step = 0; step < 16; step++) {
		for (uint8_t pass = 0; pass < 4; pass++) {
			attr_step_masks(&a, part, var, step, pass, &skip, rat);
			for (uint8_t inst = 0; inst < 16; inst++) {
				uint16_t k = attr_key(part, var, step, inst);
				TEST_ASSERT_EQUAL_UINT8(!odds_play(attr_get_odds(&a, k), pass), (skip >> inst) & 1);
				uint8_t r = attr_get_ratchet(&a, k);
				for (uint8_t n = 2; n <= 4; n++) TEST_ASSERT_EQUAL_UINT8(r == n, (rat[n - 2] >> inst) & 1);
			}
		}
	}
}

/* ---- odds / ratchets ---- */
void test_odds(void) {
	for (uint8_t p = 0; p < 12; p++) {
		TEST_ASSERT_TRUE(odds_play(1, p));
		TEST_ASSERT_EQUAL_UINT8((p % 2) == 0, odds_play(2, p));
		TEST_ASSERT_EQUAL_UINT8((p % 3) == 0, odds_play(3, p));
		TEST_ASSERT_EQUAL_UINT8((p % 4) == 0, odds_play(4, p));
	}
}

void test_ratchet_ticks(void) {
	TEST_ASSERT_EQUAL_UINT8(12, ratchet_tick(24, 2, 1));
	TEST_ASSERT_EQUAL_UINT8(8, ratchet_tick(24, 3, 1));
	TEST_ASSERT_EQUAL_UINT8(16, ratchet_tick(24, 3, 2));
	TEST_ASSERT_EQUAL_UINT8(6, ratchet_tick(24, 4, 1));
	TEST_ASSERT_EQUAL_UINT8(5, ratchet_tick(16, 3, 1)); /* rounded down */
	for (uint8_t n = 2; n <= 4; n++) {
		for (uint8_t k = 1; k < n; k++) {
			TEST_ASSERT_TRUE(ratchet_tick(12, n, k) < 12);
			TEST_ASSERT_TRUE(ratchet_tick(12, n, k) > ratchet_tick(12, n, k - 1) || k == 1);
		}
	}
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_tap_rounding_all_prescales);
	RUN_TEST(test_tap_pending_step);
	RUN_TEST(test_loop_pick_order);
	RUN_TEST(test_directions);
	RUN_TEST(test_lfsr_never_zero_and_cycles);
	RUN_TEST(test_chain);
	RUN_TEST(test_attr_layout_is_64_bytes);
	RUN_TEST(test_attr_set_get_remove);
	RUN_TEST(test_attr_full_list);
	RUN_TEST(test_attr_remove_inst_and_part);
	RUN_TEST(test_attr_copy_part);
	RUN_TEST(test_attr_validate_and_direction);
	RUN_TEST(test_attr_copy_var);
	RUN_TEST(test_attr_fast_paths);
	RUN_TEST(test_odds);
	RUN_TEST(test_ratchet_ticks);
	return UNITY_END();
}
