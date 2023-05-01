#pragma once

#include "cc1101_regs.h"

#include <stdbool.h>
#include <stdint.h>
#include <furry_hal_spi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Low level API */

/** Strobe command to the device
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 * @param      strobe  - command to execute
 *
 * @return     device status
 */
CC1101Status cc1101_strobe(FurryHalSpiBusHandle* handle, uint8_t strobe);

/** Write device register
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 * @param      reg     - register
 * @param      data    - data to write
 *
 * @return     device status
 */
CC1101Status cc1101_write_reg(FurryHalSpiBusHandle* handle, uint8_t reg, uint8_t data);

/** Read device register
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 * @param      reg     - register
 * @param[out] data    - pointer to data
 *
 * @return     device status
 */
CC1101Status cc1101_read_reg(FurryHalSpiBusHandle* handle, uint8_t reg, uint8_t* data);

/* High level API */

/** Reset
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 */
void cc1101_reset(FurryHalSpiBusHandle* handle);

/** Get status
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 *
 * @return     CC1101Status structure
 */
CC1101Status cc1101_get_status(FurryHalSpiBusHandle* handle);

/** Enable shutdown mode
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 */
void cc1101_shutdown(FurryHalSpiBusHandle* handle);

/** Get Partnumber
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 *
 * @return     part number id
 */
uint8_t cc1101_get_partnumber(FurryHalSpiBusHandle* handle);

/** Get Version
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 *
 * @return     version
 */
uint8_t cc1101_get_version(FurryHalSpiBusHandle* handle);

/** Get raw RSSI value
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 *
 * @return     rssi value
 */
uint8_t cc1101_get_rssi(FurryHalSpiBusHandle* handle);

/** Calibrate oscillator
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 */
void cc1101_calibrate(FurryHalSpiBusHandle* handle);

/** Switch to idle
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 */
void cc1101_switch_to_idle(FurryHalSpiBusHandle* handle);

/** Switch to RX
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 */
void cc1101_switch_to_rx(FurryHalSpiBusHandle* handle);

/** Switch to TX
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 */
void cc1101_switch_to_tx(FurryHalSpiBusHandle* handle);

/** Flush RX FIFO
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 */
void cc1101_flush_rx(FurryHalSpiBusHandle* handle);

/** Flush TX FIFO
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 */
void cc1101_flush_tx(FurryHalSpiBusHandle* handle);

/** Set Frequency
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 * @param      value   - frequency in herz
 *
 * @return     real frequency that were synthesized
 */
uint32_t cc1101_set_frequency(FurryHalSpiBusHandle* handle, uint32_t value);

/** Set Intermediate Frequency
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 * @param      value   - frequency in herz
 *
 * @return     real inermediate frequency that were synthesized
 */
uint32_t cc1101_set_intermediate_frequency(FurryHalSpiBusHandle* handle, uint32_t value);

/** Set Power Amplifier level table, ramp
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 * @param      value   - array of power level values
 */
void cc1101_set_pa_table(FurryHalSpiBusHandle* handle, const uint8_t value[8]);

/** Write FIFO
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 * @param      data    pointer to byte array
 * @param      size    write bytes count
 *
 * @return     size, written bytes count
 */
uint8_t cc1101_write_fifo(FurryHalSpiBusHandle* handle, const uint8_t* data, uint8_t size);

/** Read FIFO
 *
 * @param      handle  - pointer to FurryHalSpiHandle
 * @param      data    pointer to byte array
 * @param      size    bytes to read from fifo
 *
 * @return     size, read bytes count
 */
uint8_t cc1101_read_fifo(FurryHalSpiBusHandle* handle, uint8_t* data, uint8_t* size);

#ifdef __cplusplus
}
#endif
