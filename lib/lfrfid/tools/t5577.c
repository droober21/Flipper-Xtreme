#include "t5577.h"
#include <furry.h>
#include <furry_hal_rfid.h>

#define T5577_TIMING_WAIT_TIME 400
#define T5577_TIMING_START_GAP 30
#define T5577_TIMING_WRITE_GAP 18
#define T5577_TIMING_DATA_0 24
#define T5577_TIMING_DATA_1 56
#define T5577_TIMING_PROGRAM 700

#define T5577_OPCODE_PAGE_0 0b10
#define T5577_OPCODE_PAGE_1 0b11
#define T5577_OPCODE_RESET 0b00

static void t5577_start() {
    furry_hal_rfid_tim_read(125000, 0.5);
    furry_hal_rfid_pins_read();
    furry_hal_rfid_tim_read_start();

    // do not ground the antenna
    furry_hal_rfid_pin_pull_release();
}

static void t5577_stop() {
    furry_hal_rfid_tim_read_stop();
    furry_hal_rfid_tim_reset();
    furry_hal_rfid_pins_reset();
}

static void t5577_write_gap(uint32_t gap_time) {
    furry_hal_rfid_tim_read_stop();
    furry_delay_us(gap_time * 8);
    furry_hal_rfid_tim_read_start();
}

static void t5577_write_bit(bool value) {
    if(value) {
        furry_delay_us(T5577_TIMING_DATA_1 * 8);
    } else {
        furry_delay_us(T5577_TIMING_DATA_0 * 8);
    }
    t5577_write_gap(T5577_TIMING_WRITE_GAP);
}

static void t5577_write_opcode(uint8_t value) {
    t5577_write_bit((value >> 1) & 1);
    t5577_write_bit((value >> 0) & 1);
}

static void t5577_write_reset() {
    t5577_write_gap(T5577_TIMING_START_GAP);
    t5577_write_bit(1);
    t5577_write_bit(0);
}

static void t5577_write_block(uint8_t block, bool lock_bit, uint32_t data) {
    furry_delay_us(T5577_TIMING_WAIT_TIME * 8);

    // start gap
    t5577_write_gap(T5577_TIMING_START_GAP);

    // opcode for page 0
    t5577_write_opcode(T5577_OPCODE_PAGE_0);

    // lock bit
    t5577_write_bit(lock_bit);

    // data
    for(uint8_t i = 0; i < 32; i++) {
        t5577_write_bit((data >> (31 - i)) & 1);
    }

    // block address
    t5577_write_bit((block >> 2) & 1);
    t5577_write_bit((block >> 1) & 1);
    t5577_write_bit((block >> 0) & 1);

    furry_delay_us(T5577_TIMING_PROGRAM * 8);

    furry_delay_us(T5577_TIMING_WAIT_TIME * 8);
    t5577_write_reset();
}

void t5577_write(LFRFIDT5577* data) {
    t5577_start();
    FURRY_CRITICAL_ENTER();
    for(size_t i = 0; i < data->blocks_to_write; i++) {
        t5577_write_block(i, false, data->block[i]);
    }
    t5577_write_reset();
    FURRY_CRITICAL_EXIT();
    t5577_stop();
}