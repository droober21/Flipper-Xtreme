#include "string.h"
#include <m-string.h>

struct FurryString {
    string_t string;
};

#undef furry_string_alloc_set
#undef furry_string_set
#undef furry_string_cmp
#undef furry_string_cmpi
#undef furry_string_search
#undef furry_string_search_str
#undef furry_string_equal
#undef furry_string_replace
#undef furry_string_replace_str
#undef furry_string_replace_all
#undef furry_string_start_with
#undef furry_string_end_with
#undef furry_string_search_char
#undef furry_string_search_rchar
#undef furry_string_trim
#undef furry_string_cat

FurryString* furry_string_alloc() {
    FurryString* string = malloc(sizeof(FurryString));
    string_init(string->string);
    return string;
}

FurryString* furry_string_alloc_set(const FurryString* s) {
    FurryString* string = malloc(sizeof(FurryString)); //-V799
    string_init_set(string->string, s->string);
    return string;
} //-V773

FurryString* furry_string_alloc_set_str(const char cstr[]) {
    FurryString* string = malloc(sizeof(FurryString)); //-V799
    string_init_set(string->string, cstr);
    return string;
} //-V773

FurryString* furry_string_alloc_printf(const char format[], ...) {
    va_list args;
    va_start(args, format);
    FurryString* string = furry_string_alloc_vprintf(format, args);
    va_end(args);
    return string;
}

FurryString* furry_string_alloc_vprintf(const char format[], va_list args) {
    FurryString* string = malloc(sizeof(FurryString));
    string_init_vprintf(string->string, format, args);
    return string;
}

FurryString* furry_string_alloc_move(FurryString* s) {
    FurryString* string = malloc(sizeof(FurryString));
    string_init_move(string->string, s->string);
    free(s);
    return string;
}

void furry_string_free(FurryString* s) {
    string_clear(s->string);
    free(s);
}

void furry_string_reserve(FurryString* s, size_t alloc) {
    string_reserve(s->string, alloc);
}

void furry_string_reset(FurryString* s) {
    string_reset(s->string);
}

void furry_string_swap(FurryString* v1, FurryString* v2) {
    string_swap(v1->string, v2->string);
}

void furry_string_move(FurryString* v1, FurryString* v2) {
    string_clear(v1->string);
    string_init_move(v1->string, v2->string);
    free(v2);
}

size_t furry_string_hash(const FurryString* v) {
    return string_hash(v->string);
}

char furry_string_get_char(const FurryString* v, size_t index) {
    return string_get_char(v->string, index);
}

const char* furry_string_get_cstr(const FurryString* s) {
    return string_get_cstr(s->string);
}

void furry_string_set(FurryString* s, FurryString* source) {
    string_set(s->string, source->string);
}

void furry_string_set_str(FurryString* s, const char cstr[]) {
    string_set(s->string, cstr);
}

void furry_string_set_strn(FurryString* s, const char str[], size_t n) {
    string_set_strn(s->string, str, n);
}

void furry_string_set_char(FurryString* s, size_t index, const char c) {
    string_set_char(s->string, index, c);
}

int furry_string_cmp(const FurryString* s1, const FurryString* s2) {
    return string_cmp(s1->string, s2->string);
}

int furry_string_cmp_str(const FurryString* s1, const char str[]) {
    return string_cmp(s1->string, str);
}

int furry_string_cmpi(const FurryString* v1, const FurryString* v2) {
    return string_cmpi(v1->string, v2->string);
}

int furry_string_cmpi_str(const FurryString* v1, const char p2[]) {
    return string_cmpi_str(v1->string, p2);
}

size_t furry_string_search(const FurryString* v, const FurryString* needle, size_t start) {
    return string_search(v->string, needle->string, start);
}

size_t furry_string_search_str(const FurryString* v, const char needle[], size_t start) {
    return string_search(v->string, needle, start);
}

bool furry_string_equal(const FurryString* v1, const FurryString* v2) {
    return string_equal_p(v1->string, v2->string);
}

bool furry_string_equal_str(const FurryString* v1, const char v2[]) {
    return string_equal_p(v1->string, v2);
}

void furry_string_push_back(FurryString* v, char c) {
    string_push_back(v->string, c);
}

size_t furry_string_size(const FurryString* s) {
    return string_size(s->string);
}

int furry_string_printf(FurryString* v, const char format[], ...) {
    va_list args;
    va_start(args, format);
    int result = furry_string_vprintf(v, format, args);
    va_end(args);
    return result;
}

int furry_string_vprintf(FurryString* v, const char format[], va_list args) {
    return string_vprintf(v->string, format, args);
}

int furry_string_cat_printf(FurryString* v, const char format[], ...) {
    va_list args;
    va_start(args, format);
    int result = furry_string_cat_vprintf(v, format, args);
    va_end(args);
    return result;
}

int furry_string_cat_vprintf(FurryString* v, const char format[], va_list args) {
    FurryString* string = furry_string_alloc();
    int ret = furry_string_vprintf(string, format, args);
    furry_string_cat(v, string);
    furry_string_free(string);
    return ret;
}

bool furry_string_empty(const FurryString* v) {
    return string_empty_p(v->string);
}

void furry_string_replace_at(FurryString* v, size_t pos, size_t len, const char str2[]) {
    string_replace_at(v->string, pos, len, str2);
}

size_t
    furry_string_replace(FurryString* string, FurryString* needle, FurryString* replace, size_t start) {
    return string_replace(string->string, needle->string, replace->string, start);
}

size_t furry_string_replace_str(FurryString* v, const char str1[], const char str2[], size_t start) {
    return string_replace_str(v->string, str1, str2, start);
}

void furry_string_replace_all_str(FurryString* v, const char str1[], const char str2[]) {
    string_replace_all_str(v->string, str1, str2);
}

void furry_string_replace_all(FurryString* v, const FurryString* str1, const FurryString* str2) {
    string_replace_all(v->string, str1->string, str2->string);
}

bool furry_string_start_with(const FurryString* v, const FurryString* v2) {
    return string_start_with_string_p(v->string, v2->string);
}

bool furry_string_start_with_str(const FurryString* v, const char str[]) {
    return string_start_with_str_p(v->string, str);
}

bool furry_string_end_with(const FurryString* v, const FurryString* v2) {
    return string_end_with_string_p(v->string, v2->string);
}

bool furry_string_end_with_str(const FurryString* v, const char str[]) {
    return string_end_with_str_p(v->string, str);
}

size_t furry_string_search_char(const FurryString* v, char c, size_t start) {
    return string_search_char(v->string, c, start);
}

size_t furry_string_search_rchar(const FurryString* v, char c, size_t start) {
    return string_search_rchar(v->string, c, start);
}

void furry_string_left(FurryString* v, size_t index) {
    string_left(v->string, index);
}

void furry_string_right(FurryString* v, size_t index) {
    string_right(v->string, index);
}

void furry_string_mid(FurryString* v, size_t index, size_t size) {
    string_mid(v->string, index, size);
}

void furry_string_trim(FurryString* v, const char charac[]) {
    string_strim(v->string, charac);
}

void furry_string_cat(FurryString* v, const FurryString* v2) {
    string_cat(v->string, v2->string);
}

void furry_string_cat_str(FurryString* v, const char str[]) {
    string_cat(v->string, str);
}

void furry_string_set_n(FurryString* v, const FurryString* ref, size_t offset, size_t length) {
    string_set_n(v->string, ref->string, offset, length);
}

size_t furry_string_utf8_length(FurryString* str) {
    return string_length_u(str->string);
}

void furry_string_utf8_push(FurryString* str, FurryStringUnicodeValue u) {
    string_push_u(str->string, u);
}

static m_str1ng_utf8_state_e furry_state_to_state(FurryStringUTF8State state) {
    switch(state) {
    case FurryStringUTF8StateStarting:
        return M_STRING_UTF8_STARTING;
    case FurryStringUTF8StateDecoding1:
        return M_STRING_UTF8_DECODING_1;
    case FurryStringUTF8StateDecoding2:
        return M_STRING_UTF8_DECODING_2;
    case FurryStringUTF8StateDecoding3:
        return M_STRING_UTF8_DOCODING_3;
    default:
        return M_STRING_UTF8_ERROR;
    }
}

static FurryStringUTF8State state_to_furry_state(m_str1ng_utf8_state_e state) {
    switch(state) {
    case M_STRING_UTF8_STARTING:
        return FurryStringUTF8StateStarting;
    case M_STRING_UTF8_DECODING_1:
        return FurryStringUTF8StateDecoding1;
    case M_STRING_UTF8_DECODING_2:
        return FurryStringUTF8StateDecoding2;
    case M_STRING_UTF8_DOCODING_3:
        return FurryStringUTF8StateDecoding3;
    default:
        return FurryStringUTF8StateError;
    }
}

void furry_string_utf8_decode(char c, FurryStringUTF8State* state, FurryStringUnicodeValue* unicode) {
    m_str1ng_utf8_state_e m_state = furry_state_to_state(*state);
    m_str1ng_utf8_decode(c, &m_state, unicode);
    *state = state_to_furry_state(m_state);
}
