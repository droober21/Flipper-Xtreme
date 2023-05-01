#include "namechangersrv.h"
#include "m-string.h"
#include <toolbox/path.h>
#include <flipper_format/flipper_format.h>

void namechanger_on_system_start() {
    if(!furry_hal_is_normal_boot()) {
        FURRY_LOG_W(TAG, "NameChangerSRV load skipped. Device is in special startup mode.");
    } else {
        Storage* storage = furry_record_open(RECORD_STORAGE);
        FlipperFormat* file = flipper_format_file_alloc(storage);

        FurryString* NAMEHEADER;
        NAMEHEADER = furry_string_alloc_set("Flipper Name File");

        bool result = false;

        FurryString* data;
        data = furry_string_alloc();

        do {
            if(!flipper_format_file_open_existing(file, NAMECHANGER_PATH)) {
                break;
            }

            // header
            uint32_t version;

            if(!flipper_format_read_header(file, data, &version)) {
                break;
            }

            if(furry_string_cmp_str(data, furry_string_get_cstr(NAMEHEADER)) != 0) {
                break;
            }

            if(version != 1) {
                break;
            }

            // get Name
            if(!flipper_format_read_string(file, "Name", data)) {
                break;
            }

            result = true;
        } while(false);

        flipper_format_free(file);

        if(!result) {
            //file not good - write new one
            FlipperFormat* file = flipper_format_file_alloc(storage);

            bool res = false;

            do {
                // Open file for write
                if(!flipper_format_file_open_always(file, NAMECHANGER_PATH)) {
                    break;
                }

                // Write header
                if(!flipper_format_write_header_cstr(file, furry_string_get_cstr(NAMEHEADER), 1)) {
                    break;
                }

                // Write comments
                if(!flipper_format_write_comment_cstr(
                       file,
                       "Changing the value below will change your FlipperZero device name.")) {
                    break;
                }

                if(!flipper_format_write_comment_cstr(
                       file,
                       "Note: This is limited to 8 characters using the following: a-z, A-Z, 0-9, and _")) {
                    break;
                }

                if(!flipper_format_write_comment_cstr(
                       file, "It can contain other characters but use at your own risk.")) {
                    break;
                }

                //Write name
                if(!flipper_format_write_string_cstr(
                       file, "Name", furry_hal_version_get_name_ptr())) {
                    break;
                }

                res = true;
            } while(false);

            flipper_format_free(file);

            if(!res) {
                FURRY_LOG_E(TAG, "Save failed.");
            }
        } else {
            if(!furry_string_size(data)) {
                //Empty file - get default name and write to file.
                FlipperFormat* file = flipper_format_file_alloc(storage);

                bool res = false;

                do {
                    // Open file for write
                    if(!flipper_format_file_open_always(file, NAMECHANGER_PATH)) {
                        break;
                    }

                    // Write header
                    if(!flipper_format_write_header_cstr(
                           file, furry_string_get_cstr(NAMEHEADER), 1)) {
                        break;
                    }

                    // Write comments
                    if(!flipper_format_write_comment_cstr(
                           file,
                           "Changing the value below will change your FlipperZero device name.")) {
                        break;
                    }

                    if(!flipper_format_write_comment_cstr(
                           file,
                           "Note: This is limited to 8 characters using the following: a-z, A-Z, 0-9, and _")) {
                        break;
                    }

                    if(!flipper_format_write_comment_cstr(
                           file, "It cannot contain any other characters.")) {
                        break;
                    }

                    //Write name
                    if(!flipper_format_write_string_cstr(
                           file, "Name", furry_hal_version_get_name_ptr())) {
                        break;
                    }

                    res = true;
                } while(false);

                flipper_format_free(file);

                if(!res) {
                    FURRY_LOG_E(TAG, "Save failed.");
                }
            } else {
                char newdata[9];
                snprintf(newdata, 9, "%s", furry_string_get_cstr(data));
                //set name from file
                furry_hal_version_set_custom_name(newdata);
            }
        }

        furry_string_free(data);

        furry_record_close(RECORD_STORAGE);
    }
}
