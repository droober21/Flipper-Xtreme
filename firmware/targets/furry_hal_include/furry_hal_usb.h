#pragma once

#include "usb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FurryHalUsbInterface FurryHalUsbInterface;

struct FurryHalUsbInterface {
    void (*init)(usbd_device* dev, FurryHalUsbInterface* intf, void* ctx);
    void (*deinit)(usbd_device* dev);
    void (*wakeup)(usbd_device* dev);
    void (*suspend)(usbd_device* dev);

    struct usb_device_descriptor* dev_descr;

    void* str_manuf_descr;
    void* str_prod_descr;
    void* str_serial_descr;

    void* cfg_descr;
};

/** USB device interface modes */
extern FurryHalUsbInterface usb_cdc_single;
extern FurryHalUsbInterface usb_cdc_dual;
extern FurryHalUsbInterface usb_hid;
extern FurryHalUsbInterface usb_hid_u2f;

typedef enum {
    FurryHalUsbStateEventReset,
    FurryHalUsbStateEventWakeup,
    FurryHalUsbStateEventSuspend,
    FurryHalUsbStateEventDescriptorRequest,
} FurryHalUsbStateEvent;

typedef void (*FurryHalUsbStateCallback)(FurryHalUsbStateEvent state, void* context);

/** USB device low-level initialization
 */
void furry_hal_usb_init();

/** Set USB device configuration
 *
 * @param      mode new USB device mode
 * @param      ctx context passed to device mode init function
 * @return     true - mode switch started, false - mode switch is locked
 */
bool furry_hal_usb_set_config(FurryHalUsbInterface* new_if, void* ctx);

/** Get USB device configuration
 *
 * @return    current USB device mode
 */
FurryHalUsbInterface* furry_hal_usb_get_config();

/** Lock USB device mode switch
 */
void furry_hal_usb_lock();

/** Unlock USB device mode switch
 */
void furry_hal_usb_unlock();

/** Check if USB device mode switch locked
 * 
 * @return    lock state
 */
bool furry_hal_usb_is_locked();

/** Disable USB device
 */
void furry_hal_usb_disable();

/** Enable USB device
 */
void furry_hal_usb_enable();

/** Set USB state callback
 */
void furry_hal_usb_set_state_callback(FurryHalUsbStateCallback cb, void* ctx);

/** Restart USB device
 */
void furry_hal_usb_reinit();

#ifdef __cplusplus
}
#endif
