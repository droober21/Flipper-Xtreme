#include "args.h"
#include "hex.h"

size_t args_get_first_word_length(FurryString* args) {
    size_t ws = furry_string_search_char(args, ' ');
    if(ws == FURRY_STRING_FAILURE) {
        ws = furry_string_size(args);
    }

    return ws;
}

size_t args_length(FurryString* args) {
    return furry_string_size(args);
}

bool args_read_int_and_trim(FurryString* args, int* value) {
    size_t cmd_length = args_get_first_word_length(args);

    if(cmd_length == 0) {
        return false;
    }

    if(sscanf(furry_string_get_cstr(args), "%d", value) == 1) {
        furry_string_right(args, cmd_length);
        furry_string_trim(args);
        return true;
    }

    return false;
}

bool args_read_string_and_trim(FurryString* args, FurryString* word) {
    size_t cmd_length = args_get_first_word_length(args);

    if(cmd_length == 0) {
        return false;
    }

    furry_string_set_n(word, args, 0, cmd_length);
    furry_string_right(args, cmd_length);
    furry_string_trim(args);

    return true;
}

bool args_read_probably_quoted_string_and_trim(FurryString* args, FurryString* word) {
    if(furry_string_size(args) > 1 && furry_string_get_char(args, 0) == '\"') {
        size_t second_quote_pos = furry_string_search_char(args, '\"', 1);

        if(second_quote_pos == 0) {
            return false;
        }

        furry_string_set_n(word, args, 1, second_quote_pos - 1);
        furry_string_right(args, second_quote_pos + 1);
        furry_string_trim(args);
        return true;
    } else {
        return args_read_string_and_trim(args, word);
    }
}

bool args_char_to_hex(char hi_nibble, char low_nibble, uint8_t* byte) {
    uint8_t hi_nibble_value = 0;
    uint8_t low_nibble_value = 0;
    bool result = false;

    if(hex_char_to_hex_nibble(hi_nibble, &hi_nibble_value)) {
        if(hex_char_to_hex_nibble(low_nibble, &low_nibble_value)) {
            result = true;
            *byte = (hi_nibble_value << 4) | low_nibble_value;
        }
    }

    return result;
}

bool args_read_hex_bytes(FurryString* args, uint8_t* bytes, size_t bytes_count) {
    bool result = true;
    const char* str_pointer = furry_string_get_cstr(args);

    if(args_get_first_word_length(args) == (bytes_count * 2)) {
        for(size_t i = 0; i < bytes_count; i++) {
            if(!args_char_to_hex(str_pointer[i * 2], str_pointer[i * 2 + 1], &(bytes[i]))) {
                result = false;
                break;
            }
        }
    } else {
        result = false;
    }

    return result;
}
