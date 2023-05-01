#include "path.h"
#include <stddef.h>

void path_extract_filename_no_ext(const char* path, FurryString* filename) {
    furry_string_set(filename, path);

    size_t start_position = furry_string_search_rchar(filename, '/');
    size_t end_position = furry_string_search_rchar(filename, '.');

    if(start_position == FURRY_STRING_FAILURE) {
        start_position = 0;
    } else {
        start_position += 1;
    }

    if(end_position == FURRY_STRING_FAILURE) {
        end_position = furry_string_size(filename);
    }

    furry_string_mid(filename, start_position, end_position - start_position);
}

void path_extract_filename(FurryString* path, FurryString* name, bool trim_ext) {
    size_t filename_start = furry_string_search_rchar(path, '/');
    if(filename_start > 0) {
        filename_start++;
        furry_string_set_n(name, path, filename_start, furry_string_size(path) - filename_start);
    }
    if(trim_ext) {
        size_t dot = furry_string_search_rchar(name, '.');
        if(dot > 0) {
            furry_string_left(name, dot);
        }
    }
}

void path_extract_extension(FurryString* path, char* ext, size_t ext_len_max) {
    size_t dot = furry_string_search_rchar(path, '.');
    size_t filename_start = furry_string_search_rchar(path, '/');

    if((dot != FURRY_STRING_FAILURE) && (filename_start < dot)) {
        strlcpy(ext, &(furry_string_get_cstr(path))[dot], ext_len_max);
    }
}

static inline void path_cleanup(FurryString* path) {
    furry_string_trim(path);
    while(furry_string_end_with(path, "/")) {
        furry_string_left(path, furry_string_size(path) - 1);
    }
}

void path_extract_basename(const char* path, FurryString* basename) {
    furry_string_set(basename, path);
    path_cleanup(basename);
    size_t pos = furry_string_search_rchar(basename, '/');
    if(pos != FURRY_STRING_FAILURE) {
        furry_string_right(basename, pos + 1);
    }
}

void path_extract_dirname(const char* path, FurryString* dirname) {
    furry_string_set(dirname, path);
    path_cleanup(dirname);
    size_t pos = furry_string_search_rchar(dirname, '/');
    if(pos != FURRY_STRING_FAILURE) {
        furry_string_left(dirname, pos);
    }
}

void path_append(FurryString* path, const char* suffix) {
    path_cleanup(path);
    FurryString* suffix_str;
    suffix_str = furry_string_alloc_set(suffix);
    furry_string_trim(suffix_str);
    furry_string_trim(suffix_str, "/");
    furry_string_cat_printf(path, "/%s", furry_string_get_cstr(suffix_str));
    furry_string_free(suffix_str);
}

void path_concat(const char* path, const char* suffix, FurryString* out_path) {
    furry_string_set(out_path, path);
    path_append(out_path, suffix);
}

bool path_contains_only_ascii(const char* path) {
    if(!path) {
        return false;
    }

    const char* name_pos = strrchr(path, '/');
    if(name_pos == NULL) {
        name_pos = path;
    } else {
        name_pos++;
    }

    for(; *name_pos; ++name_pos) {
        const char c = *name_pos;

        // Regular ASCII characters from 0x20 to 0x7e
        const bool is_out_of_range = (c < ' ') || (c > '~');
        // Cross-platform forbidden character set
        const bool is_forbidden = strchr("\\<>*|\":?", c);

        if(is_out_of_range || is_forbidden) {
            return false;
        }
    }

    return true;
}
