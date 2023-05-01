#include "rfid_writer.h"
#include <furry_hal.h>

void writer_start() {
    furry_hal_rfid_tim_read(125000, 0.5);
    furry_hal_rfid_pins_read();
    furry_hal_rfid_tim_read_start();

    // do not ground the antenna
    furry_hal_rfid_pin_pull_release();
}

void writer_stop() {
    furry_hal_rfid_tim_read_stop();
    furry_hal_rfid_tim_reset();
    furry_hal_rfid_pins_reset();
}

void write_gap(uint32_t gap_time) {
    furry_hal_rfid_tim_read_stop();
    furry_delay_us(gap_time * 8);
    furry_hal_rfid_tim_read_start();
}

void write_bit(T55xxTiming* t55xxtiming, bool value) {
    if(value) {
        furry_delay_us(t55xxtiming->data_1 * 8);
    } else {
        furry_delay_us(t55xxtiming->data_0 * 8);
    }
    write_gap(t55xxtiming->write_gap);
}

void write_block(
    T55xxTiming* t55xxtiming,
    uint8_t page,
    uint8_t block,
    bool lock_bit,
    uint32_t data,
    bool password_enable,
    uint32_t password) {
    furry_delay_us(t55xxtiming->wait_time * 8);

    //client: https://github.com/Proxmark/proxmark3/blob/6116334485ca77343eda51c557cdc81032afcf38/client/cmdlft55xx.c#L944
    //hardware: https://github.com/Proxmark/proxmark3/blob/555fa197730c061bbf0ab01334e99bc47fb3dc06/armsrc/lfops.c#L1465
    //hardware: https://github.com/Proxmark/proxmark3/blob/555fa197730c061bbf0ab01334e99bc47fb3dc06/armsrc/lfops.c#L1396

    // start gap
    write_gap(t55xxtiming->start_gap);

    // opcode
    switch(page) {
    case 0:
        write_bit(t55xxtiming, 1);
        write_bit(t55xxtiming, 0);
        break;
    case 1:
        write_bit(t55xxtiming, 1);
        write_bit(t55xxtiming, 1);
        break;
    default:
        furry_check(false);
        break;
    }

    // password
    if(password_enable) {
        for(uint8_t i = 0; i < 32; i++) {
            write_bit(t55xxtiming, (password >> (31 - i)) & 1);
        }
    }

    // lock bit
    write_bit(t55xxtiming, lock_bit);

    // data
    for(uint8_t i = 0; i < 32; i++) {
        write_bit(t55xxtiming, (data >> (31 - i)) & 1);
    }

    // block address
    write_bit(t55xxtiming, (block >> 2) & 1);
    write_bit(t55xxtiming, (block >> 1) & 1);
    write_bit(t55xxtiming, (block >> 0) & 1);

    furry_delay_us(t55xxtiming->program * 8);

    furry_delay_us(t55xxtiming->wait_time * 8);
    write_reset(t55xxtiming);
}

void write_reset(T55xxTiming* t55xxtiming) {
    write_gap(t55xxtiming->start_gap);
    write_bit(t55xxtiming, 1);
    write_bit(t55xxtiming, 0);
}