/**
 * @file furry_hal_crypto.h
 * Cryptography HAL API
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** FurryHalCryptoKey Type */
typedef enum {
    FurryHalCryptoKeyTypeMaster, /**< Master key */
    FurryHalCryptoKeyTypeSimple, /**< Simple enencrypted key */
    FurryHalCryptoKeyTypeEncrypted, /**< Encrypted with Master key */
} FurryHalCryptoKeyType;

/** FurryHalCryptoKey Size in bits */
typedef enum {
    FurryHalCryptoKeySize128,
    FurryHalCryptoKeySize256,
} FurryHalCryptoKeySize;

/** FurryHalCryptoKey */
typedef struct {
    FurryHalCryptoKeyType type;
    FurryHalCryptoKeySize size;
    uint8_t* data;
} FurryHalCryptoKey;

/** Initialize cryptography layer This includes AES engines, PKA and RNG
 */
void furry_hal_crypto_init();

bool furry_hal_crypto_verify_enclave(uint8_t* keys_nb, uint8_t* valid_keys_nb);

bool furry_hal_crypto_verify_key(uint8_t key_slot);

/** Store key in crypto storage
 *
 * @param      key   FurryHalCryptoKey to store. Only Master, Simple or
 *                   Encrypted
 * @param      slot  pinter to int where store slot number will be saved
 *
 * @return     true on success
 */
bool furry_hal_crypto_store_add_key(FurryHalCryptoKey* key, uint8_t* slot);

/** Init AES engine and load key from crypto store
 *
 * @param      slot  store slot number
 * @param[in]  iv    pointer to 16 bytes Initialization Vector data
 *
 * @return     true on success
 */
bool furry_hal_crypto_store_load_key(uint8_t slot, const uint8_t* iv);

/** Unload key engine and deinit AES engine
 *
 * @param      slot  store slot number
 *
 * @return     true on success
 */
bool furry_hal_crypto_store_unload_key(uint8_t slot);

/** Encrypt data
 *
 * @param      input   pointer to input data
 * @param      output  pointer to output data
 * @param      size    input/output buffer size in bytes
 *
 * @return     true on success
 */
bool furry_hal_crypto_encrypt(const uint8_t* input, uint8_t* output, size_t size);

/** Decrypt data
 *
 * @param      input   pointer to input data
 * @param      output  pointer to output data
 * @param      size    input/output buffer size in bytes
 *
 * @return     true on success
 */
bool furry_hal_crypto_decrypt(const uint8_t* input, uint8_t* output, size_t size);

#ifdef __cplusplus
}
#endif
