/**
 * @file string.h
 * Furry string primitive
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <m-core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Furry string failure constant.
 */
#define FURRY_STRING_FAILURE ((size_t)-1)

/**
 * @brief Furry string primitive.
 */
typedef struct FurryString FurryString;

//---------------------------------------------------------------------------
//                               Constructors
//---------------------------------------------------------------------------

/**
 * @brief Allocate new FurryString.
 * @return FurryString* 
 */
FurryString* furry_string_alloc();

/**
 * @brief Allocate new FurryString and set it to string.
 * Allocate & Set the string a to the string.
 * @param source 
 * @return FurryString* 
 */
FurryString* furry_string_alloc_set(const FurryString* source);

/**
 * @brief Allocate new FurryString and set it to C string.
 * Allocate & Set the string a to the C string.
 * @param cstr_source 
 * @return FurryString* 
 */
FurryString* furry_string_alloc_set_str(const char cstr_source[]);

/**
 * @brief Allocate new FurryString and printf to it.
 * Initialize and set a string to the given formatted value.
 * @param format 
 * @param ... 
 * @return FurryString* 
 */
FurryString* furry_string_alloc_printf(const char format[], ...)
    _ATTRIBUTE((__format__(__printf__, 1, 2)));

/**
 * @brief Allocate new FurryString and printf to it.
 * Initialize and set a string to the given formatted value.
 * @param format 
 * @param args 
 * @return FurryString* 
 */
FurryString* furry_string_alloc_vprintf(const char format[], va_list args);

/**
 * @brief Allocate new FurryString and move source string content to it.
 * Allocate the string, set it to the other one, and destroy the other one.
 * @param source 
 * @return FurryString* 
 */
FurryString* furry_string_alloc_move(FurryString* source);

//---------------------------------------------------------------------------
//                               Destructors
//---------------------------------------------------------------------------

/**
 * @brief Free FurryString.
 * @param string 
 */
void furry_string_free(FurryString* string);

//---------------------------------------------------------------------------
//                         String memory management
//---------------------------------------------------------------------------

/**
 * @brief Reserve memory for string.
 * Modify the string capacity to be able to handle at least 'alloc' characters (including final null char).
 * @param string 
 * @param size 
 */
void furry_string_reserve(FurryString* string, size_t size);

/**
 * @brief Reset string.
 * Make the string empty.
 * @param s 
 */
void furry_string_reset(FurryString* string);

/**
 * @brief Swap two strings.
 * Swap the two strings string_1 and string_2.
 * @param string_1 
 * @param string_2 
 */
void furry_string_swap(FurryString* string_1, FurryString* string_2);

/**
 * @brief Move string_2 content to string_1.
 * Set the string to the other one, and destroy the other one.
 * @param string_1 
 * @param string_2 
 */
void furry_string_move(FurryString* string_1, FurryString* string_2);

/**
 * @brief Compute a hash for the string.
 * @param string 
 * @return size_t 
 */
size_t furry_string_hash(const FurryString* string);

/**
 * @brief Get string size (usually length, but not for UTF-8)
 * @param string 
 * @return size_t 
 */
size_t furry_string_size(const FurryString* string);

/**
 * @brief Check that string is empty or not
 * @param string 
 * @return bool
 */
bool furry_string_empty(const FurryString* string);

//---------------------------------------------------------------------------
//                               Getters
//---------------------------------------------------------------------------

/**
 * @brief Get the character at the given index.
 * Return the selected character of the string.
 * @param string 
 * @param index 
 * @return char 
 */
char furry_string_get_char(const FurryString* string, size_t index);

/**
 * @brief Return the string view a classic C string.
 * @param string 
 * @return const char* 
 */
const char* furry_string_get_cstr(const FurryString* string);

//---------------------------------------------------------------------------
//                               Setters
//---------------------------------------------------------------------------

/**
 * @brief Set the string to the other string.
 * Set the string to the source string.
 * @param string 
 * @param source 
 */
void furry_string_set(FurryString* string, FurryString* source);

/**
 * @brief Set the string to the other C string.
 * Set the string to the source C string.
 * @param string 
 * @param source 
 */
void furry_string_set_str(FurryString* string, const char source[]);

/**
 * @brief Set the string to the n first characters of the C string.
 * @param string 
 * @param source 
 * @param length 
 */
void furry_string_set_strn(FurryString* string, const char source[], size_t length);

/**
 * @brief Set the character at the given index.
 * @param string 
 * @param index 
 * @param c 
 */
void furry_string_set_char(FurryString* string, size_t index, const char c);

/**
 * @brief Set the string to the n first characters of other one.
 * @param string 
 * @param source 
 * @param offset 
 * @param length 
 */
void furry_string_set_n(FurryString* string, const FurryString* source, size_t offset, size_t length);

/**
 * @brief Format in the string the given printf format
 * @param string 
 * @param format 
 * @param ... 
 * @return int 
 */
int furry_string_printf(FurryString* string, const char format[], ...)
    _ATTRIBUTE((__format__(__printf__, 2, 3)));

/**
 * @brief Format in the string the given printf format
 * @param string 
 * @param format 
 * @param args 
 * @return int 
 */
int furry_string_vprintf(FurryString* string, const char format[], va_list args);

//---------------------------------------------------------------------------
//                               Appending
//---------------------------------------------------------------------------

/**
 * @brief Append a character to the string.
 * @param string 
 * @param c 
 */
void furry_string_push_back(FurryString* string, char c);

/**
 * @brief Append a string to the string.
 * Concatenate the string with the other string.
 * @param string_1 
 * @param string_2 
 */
void furry_string_cat(FurryString* string_1, const FurryString* string_2);

/**
 * @brief Append a C string to the string.
 * Concatenate the string with the C string.
 * @param string_1 
 * @param cstring_2 
 */
void furry_string_cat_str(FurryString* string_1, const char cstring_2[]);

/**
 * @brief Append to the string the formatted string of the given printf format.
 * @param string 
 * @param format 
 * @param ... 
 * @return int 
 */
int furry_string_cat_printf(FurryString* string, const char format[], ...)
    _ATTRIBUTE((__format__(__printf__, 2, 3)));

/**
 * @brief Append to the string the formatted string of the given printf format.
 * @param string 
 * @param format 
 * @param args 
 * @return int 
 */
int furry_string_cat_vprintf(FurryString* string, const char format[], va_list args);

//---------------------------------------------------------------------------
//                               Comparators
//---------------------------------------------------------------------------

/**
 * @brief Compare two strings and return the sort order.
 * @param string_1 
 * @param string_2 
 * @return int 
 */
int furry_string_cmp(const FurryString* string_1, const FurryString* string_2);

/**
 * @brief Compare string with C string and return the sort order.
 * @param string_1 
 * @param cstring_2 
 * @return int 
 */
int furry_string_cmp_str(const FurryString* string_1, const char cstring_2[]);

/**
 * @brief Compare two strings (case insensitive according to the current locale) and return the sort order.
 * Note: doesn't work with UTF-8 strings.
 * @param string_1 
 * @param string_2 
 * @return int 
 */
int furry_string_cmpi(const FurryString* string_1, const FurryString* string_2);

/**
 * @brief Compare string with C string (case insensitive according to the current locale) and return the sort order.
 * Note: doesn't work with UTF-8 strings.
 * @param string_1 
 * @param cstring_2 
 * @return int 
 */
int furry_string_cmpi_str(const FurryString* string_1, const char cstring_2[]);

//---------------------------------------------------------------------------
//                                 Search
//---------------------------------------------------------------------------

/**
 * @brief Search the first occurrence of the needle in the string from the position start.
 * Return STRING_FAILURE if not found.
 * By default, start is zero.
 * @param string 
 * @param needle 
 * @param start 
 * @return size_t 
 */
size_t furry_string_search(const FurryString* string, const FurryString* needle, size_t start);

/**
 * @brief Search the first occurrence of the needle in the string from the position start.
 * Return STRING_FAILURE if not found.
 * @param string 
 * @param needle 
 * @param start 
 * @return size_t 
 */
size_t furry_string_search_str(const FurryString* string, const char needle[], size_t start);

/**
 * @brief Search for the position of the character c from the position start (include) in the string.
 * Return STRING_FAILURE if not found.
 * By default, start is zero.
 * @param string 
 * @param c 
 * @param start 
 * @return size_t 
 */
size_t furry_string_search_char(const FurryString* string, char c, size_t start);

/**
 * @brief Reverse search for the position of the character c from the position start (include) in the string.
 * Return STRING_FAILURE if not found.
 * By default, start is zero.
 * @param string 
 * @param c 
 * @param start 
 * @return size_t 
 */
size_t furry_string_search_rchar(const FurryString* string, char c, size_t start);

//---------------------------------------------------------------------------
//                                Equality
//---------------------------------------------------------------------------

/**
 * @brief Test if two strings are equal.
 * @param string_1 
 * @param string_2 
 * @return bool 
 */
bool furry_string_equal(const FurryString* string_1, const FurryString* string_2);

/**
 * @brief Test if the string is equal to the C string.
 * @param string_1 
 * @param cstring_2 
 * @return bool 
 */
bool furry_string_equal_str(const FurryString* string_1, const char cstring_2[]);

//---------------------------------------------------------------------------
//                                Replace
//---------------------------------------------------------------------------

/**
 * @brief Replace in the string the sub-string at position 'pos' for 'len' bytes into the C string 'replace'.
 * @param string 
 * @param pos 
 * @param len 
 * @param replace 
 */
void furry_string_replace_at(FurryString* string, size_t pos, size_t len, const char replace[]);

/**
 * @brief Replace a string 'needle' to string 'replace' in a string from 'start' position.
 * By default, start is zero.
 * Return STRING_FAILURE if 'needle' not found or replace position.
 * @param string 
 * @param needle 
 * @param replace 
 * @param start 
 * @return size_t 
 */
size_t
    furry_string_replace(FurryString* string, FurryString* needle, FurryString* replace, size_t start);

/**
 * @brief Replace a C string 'needle' to C string 'replace' in a string from 'start' position.
 * By default, start is zero.
 * Return STRING_FAILURE if 'needle' not found or replace position.
 * @param string 
 * @param needle 
 * @param replace 
 * @param start 
 * @return size_t 
 */
size_t furry_string_replace_str(
    FurryString* string,
    const char needle[],
    const char replace[],
    size_t start);

/**
 * @brief Replace all occurrences of 'needle' string into 'replace' string.
 * @param string 
 * @param needle 
 * @param replace 
 */
void furry_string_replace_all(
    FurryString* string,
    const FurryString* needle,
    const FurryString* replace);

/**
 * @brief Replace all occurrences of 'needle' C string into 'replace' C string.
 * @param string 
 * @param needle 
 * @param replace 
 */
void furry_string_replace_all_str(FurryString* string, const char needle[], const char replace[]);

//---------------------------------------------------------------------------
//                            Start / End tests
//---------------------------------------------------------------------------

/**
 * @brief Test if the string starts with the given string.
 * @param string 
 * @param start 
 * @return bool 
 */
bool furry_string_start_with(const FurryString* string, const FurryString* start);

/**
 * @brief Test if the string starts with the given C string.
 * @param string 
 * @param start 
 * @return bool 
 */
bool furry_string_start_with_str(const FurryString* string, const char start[]);

/**
 * @brief Test if the string ends with the given string.
 * @param string 
 * @param end 
 * @return bool 
 */
bool furry_string_end_with(const FurryString* string, const FurryString* end);

/**
 * @brief Test if the string ends with the given C string.
 * @param string 
 * @param end 
 * @return bool 
 */
bool furry_string_end_with_str(const FurryString* string, const char end[]);

//---------------------------------------------------------------------------
//                                Trim
//---------------------------------------------------------------------------

/**
 * @brief Trim the string left to the first 'index' bytes.
 * @param string 
 * @param index 
 */
void furry_string_left(FurryString* string, size_t index);

/**
 * @brief Trim the string right from the 'index' position to the last position.
 * @param string 
 * @param index 
 */
void furry_string_right(FurryString* string, size_t index);

/**
 * @brief Trim the string from position index to size bytes.
 * See also furry_string_set_n.
 * @param string 
 * @param index 
 * @param size 
 */
void furry_string_mid(FurryString* string, size_t index, size_t size);

/**
 * @brief Trim a string from the given set of characters (default is " \n\r\t").
 * @param string 
 * @param chars 
 */
void furry_string_trim(FurryString* string, const char chars[]);

//---------------------------------------------------------------------------
//                                UTF8
//---------------------------------------------------------------------------

/**
 * @brief An unicode value.
 */
typedef unsigned int FurryStringUnicodeValue;

/**
 * @brief Compute the length in UTF8 characters in the string.
 * @param string 
 * @return size_t 
 */
size_t furry_string_utf8_length(FurryString* string);

/**
 * @brief Push unicode into string, encoding it in UTF8.
 * @param string 
 * @param unicode 
 */
void furry_string_utf8_push(FurryString* string, FurryStringUnicodeValue unicode);

/**
 * @brief State of the UTF8 decoding machine state.
 */
typedef enum {
    FurryStringUTF8StateStarting,
    FurryStringUTF8StateDecoding1,
    FurryStringUTF8StateDecoding2,
    FurryStringUTF8StateDecoding3,
    FurryStringUTF8StateError
} FurryStringUTF8State;

/**
 * @brief Main generic UTF8 decoder.
 * It takes a character, and the previous state and the previous value of the unicode value.
 * It updates the state and the decoded unicode value.
 * A decoded unicode encoded value is valid only when the state is FurryStringUTF8StateStarting.
 * @param c 
 * @param state 
 * @param unicode 
 */
void furry_string_utf8_decode(char c, FurryStringUTF8State* state, FurryStringUnicodeValue* unicode);

//---------------------------------------------------------------------------
//                Lasciate ogne speranza, voi ch’entrate
//---------------------------------------------------------------------------

/**
 *
 * Select either the string function or the str function depending on
 * the b operand to the function.
 * func1 is the string function / func2 is the str function.
 */

/**
 * @brief Select for 1 argument 
 */
#define FURRY_STRING_SELECT1(func1, func2, a) \
    _Generic((a), char* : func2, const char* : func2, FurryString* : func1, const FurryString* : func1)(a)

/**
 * @brief Select for 2 arguments
 */
#define FURRY_STRING_SELECT2(func1, func2, a, b) \
    _Generic((b), char* : func2, const char* : func2, FurryString* : func1, const FurryString* : func1)(a, b)

/**
 * @brief Select for 3 arguments
 */
#define FURRY_STRING_SELECT3(func1, func2, a, b, c) \
    _Generic((b), char* : func2, const char* : func2, FurryString* : func1, const FurryString* : func1)(a, b, c)

/**
 * @brief Select for 4 arguments
 */
#define FURRY_STRING_SELECT4(func1, func2, a, b, c, d) \
    _Generic((b), char* : func2, const char* : func2, FurryString* : func1, const FurryString* : func1)(a, b, c, d)

/**
 * @brief Allocate new FurryString and set it content to string (or C string).
 * ([c]string)
 */
#define furry_string_alloc_set(a) \
    FURRY_STRING_SELECT1(furry_string_alloc_set, furry_string_alloc_set_str, a)

/**
 * @brief Set the string content to string (or C string).
 * (string, [c]string)
 */
#define furry_string_set(a, b) FURRY_STRING_SELECT2(furry_string_set, furry_string_set_str, a, b)

/**
 * @brief Compare string with string (or C string) and return the sort order.
 * Note: doesn't work with UTF-8 strings.
 * (string, [c]string)
 */
#define furry_string_cmp(a, b) FURRY_STRING_SELECT2(furry_string_cmp, furry_string_cmp_str, a, b)

/**
 * @brief Compare string with string (or C string) (case insensitive according to the current locale) and return the sort order.
 * Note: doesn't work with UTF-8 strings.
 * (string, [c]string)
 */
#define furry_string_cmpi(a, b) FURRY_STRING_SELECT2(furry_string_cmpi, furry_string_cmpi_str, a, b)

/**
 * @brief Test if the string is equal to the string (or C string).
 * (string, [c]string)
 */
#define furry_string_equal(a, b) FURRY_STRING_SELECT2(furry_string_equal, furry_string_equal_str, a, b)

/**
 * @brief Replace all occurrences of string into string (or C string to another C string) in a string.
 * (string, [c]string, [c]string)
 */
#define furry_string_replace_all(a, b, c) \
    FURRY_STRING_SELECT3(furry_string_replace_all, furry_string_replace_all_str, a, b, c)

/**
 * @brief Search for a string (or C string) in a string
 * (string, [c]string[, start=0])
 */
#define furry_string_search(v, ...) \
    M_APPLY(                       \
        FURRY_STRING_SELECT3,       \
        furry_string_search,        \
        furry_string_search_str,    \
        v,                         \
        M_IF_DEFAULT1(0, __VA_ARGS__))

/**
 * @brief Search for a C string in a string
 * (string, cstring[, start=0])
 */
#define furry_string_search_str(v, ...) \
    M_APPLY(furry_string_search_str, v, M_IF_DEFAULT1(0, __VA_ARGS__))

/**
 * @brief Test if the string starts with the given string (or C string).
 * (string, [c]string)
 */
#define furry_string_start_with(a, b) \
    FURRY_STRING_SELECT2(furry_string_start_with, furry_string_start_with_str, a, b)

/**
 * @brief Test if the string ends with the given string (or C string).
 * (string, [c]string)
 */
#define furry_string_end_with(a, b) \
    FURRY_STRING_SELECT2(furry_string_end_with, furry_string_end_with_str, a, b)

/**
 * @brief Append a string (or C string) to the string.
 * (string, [c]string)
 */
#define furry_string_cat(a, b) FURRY_STRING_SELECT2(furry_string_cat, furry_string_cat_str, a, b)

/**
 * @brief Trim a string from the given set of characters (default is " \n\r\t").
 * (string[, set=" \n\r\t"])
 */
#define furry_string_trim(...) M_APPLY(furry_string_trim, M_IF_DEFAULT1("  \n\r\t", __VA_ARGS__))

/**
 * @brief Search for a character in a string.
 * (string, character[, start=0])
 */
#define furry_string_search_char(v, ...) \
    M_APPLY(furry_string_search_char, v, M_IF_DEFAULT1(0, __VA_ARGS__))

/**
 * @brief Reverse Search for a character in a string.
 * (string, character[, start=0])
 */
#define furry_string_search_rchar(v, ...) \
    M_APPLY(furry_string_search_rchar, v, M_IF_DEFAULT1(0, __VA_ARGS__))

/**
 * @brief Replace a string to another string (or C string to another C string) in a string.
 * (string, [c]string, [c]string[, start=0])
 */
#define furry_string_replace(a, b, ...) \
    M_APPLY(                           \
        FURRY_STRING_SELECT4,           \
        furry_string_replace,           \
        furry_string_replace_str,       \
        a,                             \
        b,                             \
        M_IF_DEFAULT1(0, __VA_ARGS__))

/**
 * @brief Replace a C string to another C string in a string.
 * (string, cstring, cstring[, start=0])
 */
#define furry_string_replace_str(a, b, ...) \
    M_APPLY(furry_string_replace_str, a, b, M_IF_DEFAULT1(0, __VA_ARGS__))

/**
 * @brief INIT OPLIST for FurryString.
 */
#define F_STR_INIT(a) ((a) = furry_string_alloc())

/**
 * @brief INIT SET OPLIST for FurryString.
 */
#define F_STR_INIT_SET(a, b) ((a) = furry_string_alloc_set(b))

/**
 * @brief INIT MOVE OPLIST for FurryString.
 */
#define F_STR_INIT_MOVE(a, b) ((a) = furry_string_alloc_move(b))

/**
 * @brief OPLIST for FurryString.
 */
#define FURRY_STRING_OPLIST       \
    (INIT(F_STR_INIT),           \
     INIT_SET(F_STR_INIT_SET),   \
     SET(furry_string_set),       \
     INIT_MOVE(F_STR_INIT_MOVE), \
     MOVE(furry_string_move),     \
     SWAP(furry_string_swap),     \
     RESET(furry_string_reset),   \
     EMPTY_P(furry_string_empty), \
     CLEAR(furry_string_free),    \
     HASH(furry_string_hash),     \
     EQUAL(furry_string_equal),   \
     CMP(furry_string_cmp),       \
     TYPE(FurryString*))

#ifdef __cplusplus
}
#endif