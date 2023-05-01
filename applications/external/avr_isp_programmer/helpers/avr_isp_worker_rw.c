#include "avr_isp_worker_rw.h"
#include <furry_hal_pwm.h>
#include "avr_isp_types.h"
#include "avr_isp.h"
#include "../lib/driver/avr_isp_prog_cmd.h"
#include "../lib/driver/avr_isp_chip_arr.h"

#include "flipper_i32hex_file.h"
#include <flipper_format/flipper_format.h>

#include <furry.h>

#define TAG "AvrIspWorkerRW"

#define NAME_PATERN_FLASH_FILE "flash.hex"
#define NAME_PATERN_EEPROM_FILE "eeprom.hex"

struct AvrIspWorkerRW {
    AvrIsp* avr_isp;
    FurryThread* thread;
    volatile bool worker_running;

    uint32_t chip_arr_ind;
    bool chip_detect;
    uint8_t lfuse;
    uint8_t hfuse;
    uint8_t efuse;
    uint8_t lock;
    float progress_flash;
    float progress_eeprom;
    const char* file_path;
    const char* file_name;
    AvrIspSignature signature;
    AvrIspWorkerRWCallback callback;
    void* context;

    AvrIspWorkerRWStatusCallback callback_status;
    void* context_status;
};

typedef enum {
    AvrIspWorkerRWEvtStop = (1 << 0),

    AvrIspWorkerRWEvtReading = (1 << 1),
    AvrIspWorkerRWEvtVerification = (1 << 2),
    AvrIspWorkerRWEvtWriting = (1 << 3),
    AvrIspWorkerRWEvtWritingFuse = (1 << 4),

} AvrIspWorkerRWEvt;
#define AVR_ISP_WORKER_ALL_EVENTS                                                              \
    (AvrIspWorkerRWEvtWritingFuse | AvrIspWorkerRWEvtWriting | AvrIspWorkerRWEvtVerification | \
     AvrIspWorkerRWEvtReading | AvrIspWorkerRWEvtStop)

/** Worker thread
 * 
 * @param context 
 * @return exit code 
 */
static int32_t avr_isp_worker_rw_thread(void* context) {
    AvrIspWorkerRW* instance = context;

    /* start PWM on &gpio_ext_pa4 */
    furry_hal_pwm_start(FurryHalPwmOutputIdLptim2PA4, 4000000, 50);

    FURRY_LOG_D(TAG, "Start");

    while(1) {
        uint32_t events =
            furry_thread_flags_wait(AVR_ISP_WORKER_ALL_EVENTS, FurryFlagWaitAny, FurryWaitForever);

        if(events & AvrIspWorkerRWEvtStop) {
            break;
        }

        if(events & AvrIspWorkerRWEvtWritingFuse) {
            if(avr_isp_worker_rw_write_fuse(instance, instance->file_path, instance->file_name)) {
                if(instance->callback_status)
                    instance->callback_status(
                        instance->context_status, AvrIspWorkerRWStatusEndWritingFuse);
            } else {
                if(instance->callback_status)
                    instance->callback_status(
                        instance->context_status, AvrIspWorkerRWStatusErrorWritingFuse);
            }
        }

        if(events & AvrIspWorkerRWEvtWriting) {
            if(avr_isp_worker_rw_write_dump(instance, instance->file_path, instance->file_name)) {
                if(instance->callback_status)
                    instance->callback_status(
                        instance->context_status, AvrIspWorkerRWStatusEndWriting);
            } else {
                if(instance->callback_status)
                    instance->callback_status(
                        instance->context_status, AvrIspWorkerRWStatusErrorWriting);
            }
        }

        if(events & AvrIspWorkerRWEvtVerification) {
            if(avr_isp_worker_rw_verification(instance, instance->file_path, instance->file_name)) {
                if(instance->callback_status)
                    instance->callback_status(
                        instance->context_status, AvrIspWorkerRWStatusEndVerification);
            } else {
                if(instance->callback_status)
                    instance->callback_status(
                        instance->context_status, AvrIspWorkerRWStatusErrorVerification);
            }
        }

        if(events & AvrIspWorkerRWEvtReading) {
            if(avr_isp_worker_rw_read_dump(instance, instance->file_path, instance->file_name)) {
                if(instance->callback_status)
                    instance->callback_status(
                        instance->context_status, AvrIspWorkerRWStatusEndReading);
            } else {
                if(instance->callback_status)
                    instance->callback_status(
                        instance->context_status, AvrIspWorkerRWStatusErrorReading);
            }
        }
    }
    FURRY_LOG_D(TAG, "Stop");

    furry_hal_pwm_stop(FurryHalPwmOutputIdLptim2PA4);

    return 0;
}

bool avr_isp_worker_rw_detect_chip(AvrIspWorkerRW* instance) {
    furry_assert(instance);

    FURRY_LOG_D(TAG, "Detecting AVR chip");

    instance->chip_detect = false;
    instance->chip_arr_ind = avr_isp_chip_arr_size + 1;

    /* start PWM on &gpio_ext_pa4 */
    furry_hal_pwm_start(FurryHalPwmOutputIdLptim2PA4, 4000000, 50);

    do {
        if(!avr_isp_auto_set_spi_speed_start_pmode(instance->avr_isp)) {
            FURRY_LOG_E(TAG, "Well, I managed to enter the mod program");
            break;
        }
        instance->signature = avr_isp_read_signature(instance->avr_isp);

        if(instance->signature.vendor != 0x1E) {
            //No detect chip
        } else {
            for(uint32_t ind = 0; ind < avr_isp_chip_arr_size; ind++) {
                if(avr_isp_chip_arr[ind].avrarch != F_AVR8) continue;
                if(avr_isp_chip_arr[ind].sigs[1] == instance->signature.part_family) {
                    if(avr_isp_chip_arr[ind].sigs[2] == instance->signature.part_number) {
                        FURRY_LOG_D(TAG, "Detect AVR chip = \"%s\"", avr_isp_chip_arr[ind].name);
                        FURRY_LOG_D(
                            TAG,
                            "Signature = 0x%02X 0x%02X 0x%02X",
                            instance->signature.vendor,
                            instance->signature.part_family,
                            instance->signature.part_number);

                        switch(avr_isp_chip_arr[ind].nfuses) {
                        case 1:
                            instance->lfuse = avr_isp_read_fuse_low(instance->avr_isp);
                            FURRY_LOG_D(TAG, "Lfuse = %02X", instance->lfuse);
                            break;
                        case 2:
                            instance->lfuse = avr_isp_read_fuse_low(instance->avr_isp);
                            instance->hfuse = avr_isp_read_fuse_high(instance->avr_isp);
                            FURRY_LOG_D(
                                TAG, "Lfuse = %02X Hfuse = %02X", instance->lfuse, instance->hfuse);
                            break;
                        case 3:
                            instance->lfuse = avr_isp_read_fuse_low(instance->avr_isp);
                            instance->hfuse = avr_isp_read_fuse_high(instance->avr_isp);
                            instance->efuse = avr_isp_read_fuse_extended(instance->avr_isp);
                            FURRY_LOG_D(
                                TAG,
                                "Lfuse = %02X Hfuse = %02X Efuse = %02X",
                                instance->lfuse,
                                instance->hfuse,
                                instance->efuse);
                            break;
                        default:
                            break;
                        }
                        if(avr_isp_chip_arr[ind].nlocks == 1) {
                            instance->lock = avr_isp_read_lock_byte(instance->avr_isp);
                            FURRY_LOG_D(TAG, "Lock = %02X", instance->lock);
                        }
                        instance->chip_detect = true;
                        instance->chip_arr_ind = ind;
                        break;
                    }
                }
            }
        }
        avr_isp_end_pmode(instance->avr_isp);

    } while(0);

    furry_hal_pwm_stop(FurryHalPwmOutputIdLptim2PA4);

    if(instance->callback) {
        if(instance->chip_arr_ind > avr_isp_chip_arr_size) {
            instance->callback(instance->context, "No detect", instance->chip_detect, 0);
        } else if(instance->chip_arr_ind < avr_isp_chip_arr_size) {
            instance->callback(
                instance->context,
                avr_isp_chip_arr[instance->chip_arr_ind].name,
                instance->chip_detect,
                avr_isp_chip_arr[instance->chip_arr_ind].flashsize);
        } else {
            instance->callback(instance->context, "Unknown", instance->chip_detect, 0);
        }
    }

    return instance->chip_detect;
}

AvrIspWorkerRW* avr_isp_worker_rw_alloc(void* context) {
    furry_assert(context);
    UNUSED(context);

    AvrIspWorkerRW* instance = malloc(sizeof(AvrIspWorkerRW));
    instance->avr_isp = avr_isp_alloc();

    instance->thread =
        furry_thread_alloc_ex("AvrIspWorkerRW", 4096, avr_isp_worker_rw_thread, instance);

    instance->chip_detect = false;
    instance->lfuse = 0;
    instance->hfuse = 0;
    instance->efuse = 0;
    instance->lock = 0;
    instance->progress_flash = 0.0f;
    instance->progress_eeprom = 0.0f;

    return instance;
}

void avr_isp_worker_rw_free(AvrIspWorkerRW* instance) {
    furry_assert(instance);

    avr_isp_free(instance->avr_isp);

    furry_check(!instance->worker_running);
    furry_thread_free(instance->thread);

    free(instance);
}

void avr_isp_worker_rw_start(AvrIspWorkerRW* instance) {
    furry_assert(instance);
    furry_assert(!instance->worker_running);

    instance->worker_running = true;

    furry_thread_start(instance->thread);
}

void avr_isp_worker_rw_stop(AvrIspWorkerRW* instance) {
    furry_assert(instance);
    furry_assert(instance->worker_running);

    instance->worker_running = false;
    furry_thread_flags_set(furry_thread_get_id(instance->thread), AvrIspWorkerRWEvtStop);

    furry_thread_join(instance->thread);
}

bool avr_isp_worker_rw_is_running(AvrIspWorkerRW* instance) {
    furry_assert(instance);

    return instance->worker_running;
}

void avr_isp_worker_rw_set_callback(
    AvrIspWorkerRW* instance,
    AvrIspWorkerRWCallback callback,
    void* context) {
    furry_assert(instance);

    instance->callback = callback;
    instance->context = context;
}

void avr_isp_worker_rw_set_callback_status(
    AvrIspWorkerRW* instance,
    AvrIspWorkerRWStatusCallback callback_status,
    void* context_status) {
    furry_assert(instance);

    instance->callback_status = callback_status;
    instance->context_status = context_status;
}

float avr_isp_worker_rw_get_progress_flash(AvrIspWorkerRW* instance) {
    furry_assert(instance);

    return instance->progress_flash;
}

float avr_isp_worker_rw_get_progress_eeprom(AvrIspWorkerRW* instance) {
    furry_assert(instance);

    return instance->progress_eeprom;
}

static void avr_isp_worker_rw_get_dump_flash(AvrIspWorkerRW* instance, const char* file_path) {
    furry_assert(instance);
    furry_check(instance->avr_isp);

    FURRY_LOG_D(TAG, "Dump FLASH %s", file_path);

    FlipperI32HexFile* flipper_hex_flash = flipper_i32hex_file_open_write(
        file_path, avr_isp_chip_arr[instance->chip_arr_ind].flashoffset);

    uint8_t data[272] = {0};
    bool send_extended_addr = ((avr_isp_chip_arr[instance->chip_arr_ind].flashsize / 2) > 0x10000);
    uint8_t extended_addr = 0;

    for(int32_t i = avr_isp_chip_arr[instance->chip_arr_ind].flashoffset;
        i < avr_isp_chip_arr[instance->chip_arr_ind].flashsize / 2;
        i += avr_isp_chip_arr[instance->chip_arr_ind].pagesize / 2) {
        if(send_extended_addr) {
            if(extended_addr <= ((i >> 16) & 0xFF)) {
                avr_isp_write_extended_addr(instance->avr_isp, extended_addr);
                extended_addr = ((i >> 16) & 0xFF) + 1;
            }
        }
        avr_isp_read_page(
            instance->avr_isp,
            STK_SET_FLASH_TYPE,
            (uint16_t)i,
            avr_isp_chip_arr[instance->chip_arr_ind].pagesize,
            data,
            sizeof(data));
        flipper_i32hex_file_bin_to_i32hex_set_data(
            flipper_hex_flash, data, avr_isp_chip_arr[instance->chip_arr_ind].pagesize);
        //FURRY_LOG_D(TAG, "%s", flipper_i32hex_file_get_string(flipper_hex_flash));
        instance->progress_flash =
            (float)(i) / ((float)avr_isp_chip_arr[instance->chip_arr_ind].flashsize / 2.0f);
    }
    flipper_i32hex_file_bin_to_i32hex_set_end_line(flipper_hex_flash);
    //FURRY_LOG_D(TAG, "%s", flipper_i32hex_file_get_string(flipper_hex_flash));
    flipper_i32hex_file_close(flipper_hex_flash);
    instance->progress_flash = 1.0f;
}

static void avr_isp_worker_rw_get_dump_eeprom(AvrIspWorkerRW* instance, const char* file_path) {
    furry_assert(instance);
    furry_check(instance->avr_isp);

    FURRY_LOG_D(TAG, "Dump EEPROM %s", file_path);

    FlipperI32HexFile* flipper_hex_eeprom = flipper_i32hex_file_open_write(
        file_path, avr_isp_chip_arr[instance->chip_arr_ind].eepromoffset);

    int32_t size_data = 32;
    uint8_t data[256] = {0};

    if(size_data > avr_isp_chip_arr[instance->chip_arr_ind].eepromsize)
        size_data = avr_isp_chip_arr[instance->chip_arr_ind].eepromsize;

    for(int32_t i = avr_isp_chip_arr[instance->chip_arr_ind].eepromoffset;
        i < avr_isp_chip_arr[instance->chip_arr_ind].eepromsize;
        i += size_data) {
        avr_isp_read_page(
            instance->avr_isp, STK_SET_EEPROM_TYPE, (uint16_t)i, size_data, data, sizeof(data));
        flipper_i32hex_file_bin_to_i32hex_set_data(flipper_hex_eeprom, data, size_data);
        FURRY_LOG_D(TAG, "%s", flipper_i32hex_file_get_string(flipper_hex_eeprom));
        instance->progress_eeprom =
            (float)(i) / ((float)avr_isp_chip_arr[instance->chip_arr_ind].eepromsize);
    }
    flipper_i32hex_file_bin_to_i32hex_set_end_line(flipper_hex_eeprom);
    FURRY_LOG_D(TAG, "%s", flipper_i32hex_file_get_string(flipper_hex_eeprom));
    flipper_i32hex_file_close(flipper_hex_eeprom);
    instance->progress_eeprom = 1.0f;
}

bool avr_isp_worker_rw_read_dump(
    AvrIspWorkerRW* instance,
    const char* file_path,
    const char* file_name) {
    furry_assert(instance);
    furry_assert(file_path);
    furry_assert(file_name);

    FURRY_LOG_D(TAG, "Read dump chip");

    instance->progress_flash = 0.0f;
    instance->progress_eeprom = 0.0f;
    bool ret = false;
    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);
    FurryString* file_path_name = furry_string_alloc();

    if(!avr_isp_worker_rw_detect_chip(instance)) {
        FURRY_LOG_E(TAG, "No detect AVR chip");
    } else {
        do {
            furry_string_printf(
                file_path_name, "%s/%s%s", file_path, file_name, AVR_ISP_APP_EXTENSION);
            if(!flipper_format_file_open_always(
                   flipper_format, furry_string_get_cstr(file_path_name))) {
                FURRY_LOG_E(TAG, "flipper_format_file_open_always");
                break;
            }
            if(!flipper_format_write_header_cstr(
                   flipper_format, AVR_ISP_APP_FILE_TYPE, AVR_ISP_APP_FILE_VERSION)) {
                FURRY_LOG_E(TAG, "flipper_format_write_header_cstr");
                break;
            }
            if(!flipper_format_write_string_cstr(
                   flipper_format, "Chip name", avr_isp_chip_arr[instance->chip_arr_ind].name)) {
                FURRY_LOG_E(TAG, "Chip name");
                break;
            }
            if(!flipper_format_write_hex(
                   flipper_format,
                   "Signature",
                   (uint8_t*)&instance->signature,
                   sizeof(AvrIspSignature))) {
                FURRY_LOG_E(TAG, "Unable to add Signature");
                break;
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 0) {
                if(!flipper_format_write_hex(flipper_format, "Lfuse", &instance->lfuse, 1)) {
                    FURRY_LOG_E(TAG, "Unable to add Lfuse");
                    break;
                }
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 1) {
                if(!flipper_format_write_hex(flipper_format, "Hfuse", &instance->hfuse, 1)) {
                    FURRY_LOG_E(TAG, "Unable to add Hfuse");
                    break;
                }
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 2) {
                if(!flipper_format_write_hex(flipper_format, "Efuse", &instance->efuse, 1)) {
                    FURRY_LOG_E(TAG, "Unable to add Efuse");
                    break;
                }
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nlocks == 1) {
                if(!flipper_format_write_hex(flipper_format, "Lock", &instance->lock, 1)) {
                    FURRY_LOG_E(TAG, "Unable to add Lock");
                    break;
                }
            }
            furry_string_printf(file_path_name, "%s_%s", file_name, NAME_PATERN_FLASH_FILE);
            if(!flipper_format_write_string_cstr(
                   flipper_format, "Dump_flash", furry_string_get_cstr(file_path_name))) {
                FURRY_LOG_E(TAG, "Unable to add Dump_flash");
                break;
            }

            if(avr_isp_chip_arr[instance->chip_arr_ind].eepromsize > 0) {
                furry_string_printf(file_path_name, "%s_%s", file_name, NAME_PATERN_EEPROM_FILE);
                if(avr_isp_chip_arr[instance->chip_arr_ind].eepromsize > 0) {
                    if(!flipper_format_write_string_cstr(
                           flipper_format, "Dump_eeprom", furry_string_get_cstr(file_path_name))) {
                        FURRY_LOG_E(TAG, "Unable to add Dump_eeprom");
                        break;
                    }
                }
            }
            ret = true;
        } while(false);
    }

    flipper_format_free(flipper_format);
    furry_record_close(RECORD_STORAGE);

    if(ret) {
        if(avr_isp_auto_set_spi_speed_start_pmode(instance->avr_isp)) {
            //Dump flash
            furry_string_printf(
                file_path_name, "%s/%s_%s", file_path, file_name, NAME_PATERN_FLASH_FILE);
            avr_isp_worker_rw_get_dump_flash(instance, furry_string_get_cstr(file_path_name));
            //Dump eeprom
            if(avr_isp_chip_arr[instance->chip_arr_ind].eepromsize > 0) {
                furry_string_printf(
                    file_path_name, "%s/%s_%s", file_path, file_name, NAME_PATERN_EEPROM_FILE);
                avr_isp_worker_rw_get_dump_eeprom(instance, furry_string_get_cstr(file_path_name));
            }

            avr_isp_end_pmode(instance->avr_isp);
        }
    }

    furry_string_free(file_path_name);

    return true;
}

void avr_isp_worker_rw_read_dump_start(
    AvrIspWorkerRW* instance,
    const char* file_path,
    const char* file_name) {
    furry_assert(instance);

    instance->file_path = file_path;
    instance->file_name = file_name;
    furry_thread_flags_set(furry_thread_get_id(instance->thread), AvrIspWorkerRWEvtReading);
}

static bool avr_isp_worker_rw_verification_flash(AvrIspWorkerRW* instance, const char* file_path) {
    furry_assert(instance);
    furry_assert(file_path);

    FURRY_LOG_D(TAG, "Verification flash %s", file_path);

    instance->progress_flash = 0.0;
    bool ret = true;

    FlipperI32HexFile* flipper_hex_flash = flipper_i32hex_file_open_read(file_path);

    uint8_t data_read_flash[272] = {0};
    uint8_t data_read_hex[272] = {0};

    uint32_t addr = avr_isp_chip_arr[instance->chip_arr_ind].flashoffset;
    bool send_extended_addr = ((avr_isp_chip_arr[instance->chip_arr_ind].flashsize / 2) > 0x10000);
    uint8_t extended_addr = 0;

    FlipperI32HexFileRet flipper_hex_ret = flipper_i32hex_file_i32hex_to_bin_get_data(
        flipper_hex_flash, data_read_hex, sizeof(data_read_hex));

    while(((flipper_hex_ret.status == FlipperI32HexFileStatusData) ||
           (flipper_hex_ret.status == FlipperI32HexFileStatusUdateAddr)) &&
          ret) {
        switch(flipper_hex_ret.status) {
        case FlipperI32HexFileStatusData:

            if(send_extended_addr) {
                if(extended_addr <= ((addr >> 16) & 0xFF)) {
                    avr_isp_write_extended_addr(instance->avr_isp, extended_addr);
                    extended_addr = ((addr >> 16) & 0xFF) + 1;
                }
            }

            avr_isp_read_page(
                instance->avr_isp,
                STK_SET_FLASH_TYPE,
                (uint16_t)addr,
                flipper_hex_ret.data_size,
                data_read_flash,
                sizeof(data_read_flash));

            if(memcmp(data_read_hex, data_read_flash, flipper_hex_ret.data_size) != 0) {
                ret = false;

                FURRY_LOG_E(TAG, "Verification flash error");
                FURRY_LOG_E(TAG, "Addr: 0x%04lX", addr);
                for(uint32_t i = 0; i < flipper_hex_ret.data_size; i++) {
                    FURRY_LOG_RAW_E("%02X ", data_read_hex[i]);
                }
                FURRY_LOG_RAW_E("\r\n");
                for(uint32_t i = 0; i < flipper_hex_ret.data_size; i++) {
                    FURRY_LOG_RAW_E("%02X ", data_read_flash[i]);
                }
                FURRY_LOG_RAW_E("\r\n");
            }

            addr += flipper_hex_ret.data_size / 2;
            instance->progress_flash =
                (float)(addr) / ((float)avr_isp_chip_arr[instance->chip_arr_ind].flashsize / 2.0f);
            break;

        case FlipperI32HexFileStatusUdateAddr:
            addr = (data_read_hex[0] << 24 | data_read_hex[1] << 16) / 2;
            break;

        default:
            furry_crash(TAG " Incorrect status.");
            break;
        }

        flipper_hex_ret = flipper_i32hex_file_i32hex_to_bin_get_data(
            flipper_hex_flash, data_read_hex, sizeof(data_read_hex));
    }

    flipper_i32hex_file_close(flipper_hex_flash);
    instance->progress_flash = 1.0f;

    return ret;
}

static bool
    avr_isp_worker_rw_verification_eeprom(AvrIspWorkerRW* instance, const char* file_path) {
    furry_assert(instance);
    furry_assert(file_path);

    FURRY_LOG_D(TAG, "Verification eeprom %s", file_path);

    instance->progress_eeprom = 0.0;
    bool ret = true;

    FlipperI32HexFile* flipper_hex_eeprom = flipper_i32hex_file_open_read(file_path);

    uint8_t data_read_eeprom[272] = {0};
    uint8_t data_read_hex[272] = {0};

    uint32_t addr = avr_isp_chip_arr[instance->chip_arr_ind].eepromoffset;

    FlipperI32HexFileRet flipper_hex_ret = flipper_i32hex_file_i32hex_to_bin_get_data(
        flipper_hex_eeprom, data_read_hex, sizeof(data_read_hex));

    while(((flipper_hex_ret.status == FlipperI32HexFileStatusData) ||
           (flipper_hex_ret.status == FlipperI32HexFileStatusUdateAddr)) &&
          ret) {
        switch(flipper_hex_ret.status) {
        case FlipperI32HexFileStatusData:
            avr_isp_read_page(
                instance->avr_isp,
                STK_SET_EEPROM_TYPE,
                (uint16_t)addr,
                flipper_hex_ret.data_size,
                data_read_eeprom,
                sizeof(data_read_eeprom));

            if(memcmp(data_read_hex, data_read_eeprom, flipper_hex_ret.data_size) != 0) {
                ret = false;
                FURRY_LOG_E(TAG, "Verification eeprom error");
                FURRY_LOG_E(TAG, "Addr: 0x%04lX", addr);
                for(uint32_t i = 0; i < flipper_hex_ret.data_size; i++) {
                    FURRY_LOG_RAW_E("%02X ", data_read_hex[i]);
                }
                FURRY_LOG_RAW_E("\r\n");
                for(uint32_t i = 0; i < flipper_hex_ret.data_size; i++) {
                    FURRY_LOG_RAW_E("%02X ", data_read_eeprom[i]);
                }
                FURRY_LOG_RAW_E("\r\n");
            }

            addr += flipper_hex_ret.data_size;
            instance->progress_eeprom =
                (float)(addr) / ((float)avr_isp_chip_arr[instance->chip_arr_ind].eepromsize);
            break;

        case FlipperI32HexFileStatusUdateAddr:
            addr = (data_read_hex[0] << 24 | data_read_hex[1] << 16);
            break;

        default:
            furry_crash(TAG " Incorrect status.");
            break;
        }

        flipper_hex_ret = flipper_i32hex_file_i32hex_to_bin_get_data(
            flipper_hex_eeprom, data_read_hex, sizeof(data_read_hex));
    }

    flipper_i32hex_file_close(flipper_hex_eeprom);
    instance->progress_eeprom = 1.0f;

    return ret;
}

bool avr_isp_worker_rw_verification(
    AvrIspWorkerRW* instance,
    const char* file_path,
    const char* file_name) {
    furry_assert(instance);
    furry_assert(file_path);
    furry_assert(file_name);

    FURRY_LOG_D(TAG, "Verification chip");

    instance->progress_flash = 0.0f;
    instance->progress_eeprom = 0.0f;
    FurryString* file_path_name = furry_string_alloc();

    bool ret = false;

    if(avr_isp_auto_set_spi_speed_start_pmode(instance->avr_isp)) {
        do {
            furry_string_printf(
                file_path_name, "%s/%s_%s", file_path, file_name, NAME_PATERN_FLASH_FILE);
            if(!avr_isp_worker_rw_verification_flash(
                   instance, furry_string_get_cstr(file_path_name)))
                break;

            if(avr_isp_chip_arr[instance->chip_arr_ind].eepromsize > 0) {
                furry_string_printf(
                    file_path_name, "%s/%s_%s", file_path, file_name, NAME_PATERN_EEPROM_FILE);

                if(!avr_isp_worker_rw_verification_eeprom(
                       instance, furry_string_get_cstr(file_path_name)))
                    break;
            }
            ret = true;
        } while(false);
        avr_isp_end_pmode(instance->avr_isp);
        furry_string_free(file_path_name);
    }
    return ret;
}

void avr_isp_worker_rw_verification_start(
    AvrIspWorkerRW* instance,
    const char* file_path,
    const char* file_name) {
    furry_assert(instance);

    instance->file_path = file_path;
    instance->file_name = file_name;
    furry_thread_flags_set(furry_thread_get_id(instance->thread), AvrIspWorkerRWEvtVerification);
}

static void avr_isp_worker_rw_write_flash(AvrIspWorkerRW* instance, const char* file_path) {
    furry_assert(instance);
    furry_check(instance->avr_isp);

    instance->progress_flash = 0.0;

    FURRY_LOG_D(TAG, "Write Flash %s", file_path);

    uint8_t data[288] = {0};

    FlipperI32HexFile* flipper_hex_flash = flipper_i32hex_file_open_read(file_path);

    uint32_t addr = avr_isp_chip_arr[instance->chip_arr_ind].flashoffset;
    bool send_extended_addr = ((avr_isp_chip_arr[instance->chip_arr_ind].flashsize / 2) > 0x10000);
    uint8_t extended_addr = 0;

    FlipperI32HexFileRet flipper_hex_ret =
        flipper_i32hex_file_i32hex_to_bin_get_data(flipper_hex_flash, data, sizeof(data));

    while((flipper_hex_ret.status == FlipperI32HexFileStatusData) ||
          (flipper_hex_ret.status == FlipperI32HexFileStatusUdateAddr)) {
        switch(flipper_hex_ret.status) {
        case FlipperI32HexFileStatusData:

            if(send_extended_addr) {
                if(extended_addr <= ((addr >> 16) & 0xFF)) {
                    avr_isp_write_extended_addr(instance->avr_isp, extended_addr);
                    extended_addr = ((addr >> 16) & 0xFF) + 1;
                }
            }

            if(!avr_isp_write_page(
                   instance->avr_isp,
                   STK_SET_FLASH_TYPE,
                   avr_isp_chip_arr[instance->chip_arr_ind].flashsize,
                   (uint16_t)addr,
                   avr_isp_chip_arr[instance->chip_arr_ind].pagesize,
                   data,
                   flipper_hex_ret.data_size)) {
                break;
            }
            addr += flipper_hex_ret.data_size / 2;
            instance->progress_flash =
                (float)(addr) / ((float)avr_isp_chip_arr[instance->chip_arr_ind].flashsize / 2.0f);
            break;

        case FlipperI32HexFileStatusUdateAddr:
            addr = (data[0] << 24 | data[1] << 16) / 2;
            break;

        default:
            furry_crash(TAG " Incorrect status.");
            break;
        }

        flipper_hex_ret =
            flipper_i32hex_file_i32hex_to_bin_get_data(flipper_hex_flash, data, sizeof(data));
    }

    flipper_i32hex_file_close(flipper_hex_flash);
    instance->progress_flash = 1.0f;
}

static void avr_isp_worker_rw_write_eeprom(AvrIspWorkerRW* instance, const char* file_path) {
    furry_assert(instance);
    furry_check(instance->avr_isp);

    instance->progress_eeprom = 0.0;
    uint8_t data[288] = {0};

    FURRY_LOG_D(TAG, "Write EEPROM %s", file_path);

    FlipperI32HexFile* flipper_hex_eeprom_read = flipper_i32hex_file_open_read(file_path);

    uint32_t addr = avr_isp_chip_arr[instance->chip_arr_ind].eepromoffset;
    FlipperI32HexFileRet flipper_hex_ret =
        flipper_i32hex_file_i32hex_to_bin_get_data(flipper_hex_eeprom_read, data, sizeof(data));

    while((flipper_hex_ret.status == FlipperI32HexFileStatusData) ||
          (flipper_hex_ret.status == FlipperI32HexFileStatusUdateAddr)) {
        switch(flipper_hex_ret.status) {
        case FlipperI32HexFileStatusData:
            if(!avr_isp_write_page(
                   instance->avr_isp,
                   STK_SET_EEPROM_TYPE,
                   avr_isp_chip_arr[instance->chip_arr_ind].eepromsize,
                   (uint16_t)addr,
                   avr_isp_chip_arr[instance->chip_arr_ind].eeprompagesize,
                   data,
                   flipper_hex_ret.data_size)) {
                break;
            }
            addr += flipper_hex_ret.data_size;
            instance->progress_eeprom =
                (float)(addr) / ((float)avr_isp_chip_arr[instance->chip_arr_ind].eepromsize);
            break;

        case FlipperI32HexFileStatusUdateAddr:
            addr = data[0] << 24 | data[1] << 16;
            break;

        default:
            furry_crash(TAG " Incorrect status.");
            break;
        }

        flipper_hex_ret = flipper_i32hex_file_i32hex_to_bin_get_data(
            flipper_hex_eeprom_read, data, sizeof(data));
    }

    flipper_i32hex_file_close(flipper_hex_eeprom_read);
    instance->progress_eeprom = 1.0f;
}

bool avr_isp_worker_rw_write_dump(
    AvrIspWorkerRW* instance,
    const char* file_path,
    const char* file_name) {
    furry_assert(instance);
    furry_assert(file_path);
    furry_assert(file_name);

    FURRY_LOG_D(TAG, "Write dump chip");

    instance->progress_flash = 0.0f;
    instance->progress_eeprom = 0.0f;
    bool ret = false;

    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);
    FurryString* file_path_name = furry_string_alloc();

    FurryString* temp_str_1 = furry_string_alloc();
    FurryString* temp_str_2 = furry_string_alloc();
    uint32_t temp_data32;

    if(!avr_isp_worker_rw_detect_chip(instance)) {
        FURRY_LOG_E(TAG, "No detect AVR chip");
    } else {
        //upload file with description
        do {
            furry_string_printf(
                file_path_name, "%s/%s%s", file_path, file_name, AVR_ISP_APP_EXTENSION);
            if(!flipper_format_file_open_existing(
                   flipper_format, furry_string_get_cstr(file_path_name))) {
                FURRY_LOG_E(TAG, "Error open file %s", furry_string_get_cstr(file_path_name));
                break;
            }

            if(!flipper_format_read_header(flipper_format, temp_str_1, &temp_data32)) {
                FURRY_LOG_E(TAG, "Missing or incorrect header");
                break;
            }

            if((!strcmp(furry_string_get_cstr(temp_str_1), AVR_ISP_APP_FILE_TYPE)) &&
               temp_data32 == AVR_ISP_APP_FILE_VERSION) {
            } else {
                FURRY_LOG_E(TAG, "Type or version mismatch");
                break;
            }

            AvrIspSignature sig_read = {0};

            if(!flipper_format_read_hex(
                   flipper_format, "Signature", (uint8_t*)&sig_read, sizeof(AvrIspSignature))) {
                FURRY_LOG_E(TAG, "Missing Signature");
                break;
            }

            if(memcmp(
                   (uint8_t*)&instance->signature, (uint8_t*)&sig_read, sizeof(AvrIspSignature)) !=
               0) {
                FURRY_LOG_E(
                    TAG,
                    "Wrong chip. Connected (%02X %02X %02X), read from file (%02X %02X %02X)",
                    instance->signature.vendor,
                    instance->signature.part_family,
                    instance->signature.part_number,
                    sig_read.vendor,
                    sig_read.part_family,
                    sig_read.part_number);
                break;
            }

            if(!flipper_format_read_string(flipper_format, "Dump_flash", temp_str_1)) {
                FURRY_LOG_E(TAG, "Missing Dump_flash");
                break;
            }

            //may not be
            flipper_format_read_string(flipper_format, "Dump_eeprom", temp_str_2);
            ret = true;
        } while(false);
    }
    flipper_format_free(flipper_format);
    furry_record_close(RECORD_STORAGE);

    if(ret) {
        do {
            //checking .hex files for errors

            furry_string_printf(
                file_path_name, "%s/%s", file_path, furry_string_get_cstr(temp_str_1));

            FURRY_LOG_D(TAG, "Check flash file");
            FlipperI32HexFile* flipper_hex_flash_read =
                flipper_i32hex_file_open_read(furry_string_get_cstr(file_path_name));
            if(flipper_i32hex_file_check(flipper_hex_flash_read)) {
                FURRY_LOG_D(TAG, "Check flash file: OK");
            } else {
                FURRY_LOG_E(TAG, "Check flash file: Error");
                ret = false;
            }
            flipper_i32hex_file_close(flipper_hex_flash_read);

            if(furry_string_size(temp_str_2) > 4) {
                furry_string_printf(
                    file_path_name, "%s/%s", file_path, furry_string_get_cstr(temp_str_2));

                FURRY_LOG_D(TAG, "Check eeprom file");
                FlipperI32HexFile* flipper_hex_eeprom_read =
                    flipper_i32hex_file_open_read(furry_string_get_cstr(file_path_name));
                if(flipper_i32hex_file_check(flipper_hex_eeprom_read)) {
                    FURRY_LOG_D(TAG, "Check eeprom file: OK");
                } else {
                    FURRY_LOG_E(TAG, "Check eeprom file: Error");
                    ret = false;
                }
                flipper_i32hex_file_close(flipper_hex_eeprom_read);
            }

            if(!ret) break;
            ret = false;

            //erase chip
            FURRY_LOG_D(TAG, "Erase chip");
            if(!avr_isp_erase_chip(instance->avr_isp)) {
                FURRY_LOG_E(TAG, "Erase chip: Error");
                break;
            }

            if(!avr_isp_auto_set_spi_speed_start_pmode(instance->avr_isp)) {
                FURRY_LOG_E(TAG, "Well, I managed to enter the mod program");
                break;
            }

            //write flash
            furry_string_printf(
                file_path_name, "%s/%s", file_path, furry_string_get_cstr(temp_str_1));
            avr_isp_worker_rw_write_flash(instance, furry_string_get_cstr(file_path_name));

            //write eeprom
            if(furry_string_size(temp_str_2) > 4) {
                furry_string_printf(
                    file_path_name, "%s/%s", file_path, furry_string_get_cstr(temp_str_2));
                avr_isp_worker_rw_write_eeprom(instance, furry_string_get_cstr(file_path_name));
            }
            ret = true;
            avr_isp_end_pmode(instance->avr_isp);
        } while(false);
    }

    furry_string_free(file_path_name);
    furry_string_free(temp_str_1);
    furry_string_free(temp_str_2);

    return ret;
}

void avr_isp_worker_rw_write_dump_start(
    AvrIspWorkerRW* instance,
    const char* file_path,
    const char* file_name) {
    furry_assert(instance);

    instance->file_path = file_path;
    instance->file_name = file_name;
    furry_thread_flags_set(furry_thread_get_id(instance->thread), AvrIspWorkerRWEvtWriting);
}

bool avr_isp_worker_rw_write_fuse(
    AvrIspWorkerRW* instance,
    const char* file_path,
    const char* file_name) {
    furry_assert(instance);
    furry_assert(file_path);
    furry_assert(file_name);

    FURRY_LOG_D(TAG, "Write fuse chip");

    bool ret = false;
    uint8_t lfuse;
    uint8_t hfuse;
    uint8_t efuse;
    uint8_t lock;

    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);
    FurryString* temp_str = furry_string_alloc();

    uint32_t temp_data32;

    if(!avr_isp_worker_rw_detect_chip(instance)) {
        FURRY_LOG_E(TAG, "No detect AVR chip");
    } else {
        //upload file with description
        do {
            furry_string_printf(temp_str, "%s/%s%s", file_path, file_name, AVR_ISP_APP_EXTENSION);
            if(!flipper_format_file_open_existing(flipper_format, furry_string_get_cstr(temp_str))) {
                FURRY_LOG_E(TAG, "Error open file %s", furry_string_get_cstr(temp_str));
                break;
            }

            if(!flipper_format_read_header(flipper_format, temp_str, &temp_data32)) {
                FURRY_LOG_E(TAG, "Missing or incorrect header");
                break;
            }

            if((!strcmp(furry_string_get_cstr(temp_str), AVR_ISP_APP_FILE_TYPE)) &&
               temp_data32 == AVR_ISP_APP_FILE_VERSION) {
            } else {
                FURRY_LOG_E(TAG, "Type or version mismatch");
                break;
            }

            AvrIspSignature sig_read = {0};

            if(!flipper_format_read_hex(
                   flipper_format, "Signature", (uint8_t*)&sig_read, sizeof(AvrIspSignature))) {
                FURRY_LOG_E(TAG, "Missing Signature");
                break;
            }

            if(memcmp(
                   (uint8_t*)&instance->signature, (uint8_t*)&sig_read, sizeof(AvrIspSignature)) !=
               0) {
                FURRY_LOG_E(
                    TAG,
                    "Wrong chip. Connected (%02X %02X %02X), read from file (%02X %02X %02X)",
                    instance->signature.vendor,
                    instance->signature.part_family,
                    instance->signature.part_number,
                    sig_read.vendor,
                    sig_read.part_family,
                    sig_read.part_number);
                break;
            }

            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 0) {
                if(!flipper_format_read_hex(flipper_format, "Lfuse", &lfuse, 1)) {
                    FURRY_LOG_E(TAG, "Missing Lfuse");
                    break;
                }
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 1) {
                if(!flipper_format_read_hex(flipper_format, "Hfuse", &hfuse, 1)) {
                    FURRY_LOG_E(TAG, "Missing Hfuse");
                    break;
                }
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 2) {
                if(!flipper_format_read_hex(flipper_format, "Efuse", &efuse, 1)) {
                    FURRY_LOG_E(TAG, "Missing Efuse");
                    break;
                }
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nlocks == 1) {
                if(!flipper_format_read_hex(flipper_format, "Lock", &lock, 1)) {
                    FURRY_LOG_E(TAG, "Missing Lock");
                    break;
                }
            }

            if(!avr_isp_auto_set_spi_speed_start_pmode(instance->avr_isp)) {
                FURRY_LOG_E(TAG, "Well, I managed to enter the mod program");
                break;
            }

            ret = true;

            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 0) {
                if(instance->lfuse != lfuse) {
                    if(!avr_isp_write_fuse_low(instance->avr_isp, lfuse)) {
                        FURRY_LOG_E(TAG, "Write Lfuse: error");
                        ret = false;
                    }
                }
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 1) {
                if(instance->hfuse != hfuse) {
                    if(!avr_isp_write_fuse_high(instance->avr_isp, hfuse)) {
                        FURRY_LOG_E(TAG, "Write Hfuse: error");
                        ret = false;
                    }
                }
            }
            if(avr_isp_chip_arr[instance->chip_arr_ind].nfuses > 2) {
                if(instance->efuse != efuse) {
                    if(!avr_isp_write_fuse_extended(instance->avr_isp, efuse)) {
                        FURRY_LOG_E(TAG, "Write Efuse: error");
                        ret = false;
                    }
                }
            }

            if(avr_isp_chip_arr[instance->chip_arr_ind].nlocks == 1) {
                FURRY_LOG_D(TAG, "Write lock byte");
                if(instance->lock != lock) {
                    if(!avr_isp_write_lock_byte(instance->avr_isp, lock)) {
                        FURRY_LOG_E(TAG, "Write Lock byte: error");
                        ret = false;
                    }
                }
            }
            avr_isp_end_pmode(instance->avr_isp);
        } while(false);
    }

    flipper_format_free(flipper_format);
    furry_record_close(RECORD_STORAGE);
    furry_string_free(temp_str);
    return ret;
}

void avr_isp_worker_rw_write_fuse_start(
    AvrIspWorkerRW* instance,
    const char* file_path,
    const char* file_name) {
    furry_assert(instance);

    instance->file_path = file_path;
    instance->file_name = file_name;
    furry_thread_flags_set(furry_thread_get_id(instance->thread), AvrIspWorkerRWEvtWritingFuse);
}