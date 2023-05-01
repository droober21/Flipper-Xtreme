#include "i2csender.h"

void i2c_send(i2cSender* i2c_sender) {
    furry_hal_i2c_acquire(I2C_BUS);
    uint8_t adress = i2c_sender->scanner->addresses[i2c_sender->address_idx] << 1;
    i2c_sender->error = furry_hal_i2c_trx(
        I2C_BUS,
        adress,
        &i2c_sender->value,
        sizeof(i2c_sender->value),
        i2c_sender->recv,
        sizeof(i2c_sender->recv),
        I2C_TIMEOUT);
    furry_hal_i2c_release(I2C_BUS);
    i2c_sender->must_send = false;
    i2c_sender->sended = true;
}

i2cSender* i2c_sender_alloc() {
    i2cSender* i2c_sender = malloc(sizeof(i2cSender));
    i2c_sender->must_send = false;
    i2c_sender->sended = false;
    return i2c_sender;
}

void i2c_sender_free(i2cSender* i2c_sender) {
    furry_assert(i2c_sender);
    free(i2c_sender);
}