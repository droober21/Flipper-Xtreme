#pragma once

#include <furry_hal_nfc.h>

#define RFAL_PICOPASS_UID_LEN 8
#define RFAL_PICOPASS_MAX_BLOCK_LEN 8

enum {
    RFAL_PICOPASS_CMD_ACTALL = 0x0A,
    RFAL_PICOPASS_CMD_IDENTIFY = 0x0C,
    RFAL_PICOPASS_CMD_SELECT = 0x81,
    RFAL_PICOPASS_CMD_READCHECK = 0x88,
    RFAL_PICOPASS_CMD_CHECK = 0x05,
    RFAL_PICOPASS_CMD_READ = 0x0C,
    RFAL_PICOPASS_CMD_WRITE = 0x87,
};

typedef struct {
    uint8_t CSN[RFAL_PICOPASS_UID_LEN]; // Anti-collision CSN
    uint8_t crc[2];
} rfalPicoPassIdentifyRes;

typedef struct {
    uint8_t CSN[RFAL_PICOPASS_UID_LEN]; // Real CSN
    uint8_t crc[2];
} rfalPicoPassSelectRes;

typedef struct {
    uint8_t CCNR[8];
} rfalPicoPassReadCheckRes;

typedef struct {
    uint8_t mac[4];
} rfalPicoPassCheckRes;

typedef struct {
    uint8_t data[RFAL_PICOPASS_MAX_BLOCK_LEN];
    uint8_t crc[2];
} rfalPicoPassReadBlockRes;

FurryHalNfcReturn rfalPicoPassPollerInitialize(void);
FurryHalNfcReturn rfalPicoPassPollerCheckPresence(void);
FurryHalNfcReturn rfalPicoPassPollerIdentify(rfalPicoPassIdentifyRes* idRes);
FurryHalNfcReturn rfalPicoPassPollerSelect(uint8_t* csn, rfalPicoPassSelectRes* selRes);
FurryHalNfcReturn rfalPicoPassPollerReadCheck(rfalPicoPassReadCheckRes* rcRes);
FurryHalNfcReturn rfalPicoPassPollerCheck(uint8_t* mac, rfalPicoPassCheckRes* chkRes);
FurryHalNfcReturn rfalPicoPassPollerReadBlock(uint8_t blockNum, rfalPicoPassReadBlockRes* readRes);
FurryHalNfcReturn rfalPicoPassPollerWriteBlock(uint8_t blockNum, uint8_t data[8], uint8_t mac[4]);
