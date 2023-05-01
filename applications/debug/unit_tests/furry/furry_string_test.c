#include <furry.h>
#include "../minunit.h"

static void test_setup(void) {
}

static void test_teardown(void) {
}

static FurryString* furry_string_alloc_vprintf_test(const char format[], ...) {
    va_list args;
    va_start(args, format);
    FurryString* string = furry_string_alloc_vprintf(format, args);
    va_end(args);
    return string;
}

MU_TEST(mu_test_furry_string_alloc_free) {
    FurryString* tmp;
    FurryString* string;

    // test alloc and free
    string = furry_string_alloc();
    mu_check(string != NULL);
    mu_check(furry_string_empty(string));
    furry_string_free(string);

    // test furry_string_alloc_set_str and free
    string = furry_string_alloc_set_str("test");
    mu_check(string != NULL);
    mu_check(!furry_string_empty(string));
    mu_check(furry_string_cmp(string, "test") == 0);
    furry_string_free(string);

    // test furry_string_alloc_set and free
    tmp = furry_string_alloc_set("more");
    string = furry_string_alloc_set(tmp);
    furry_string_free(tmp);
    mu_check(string != NULL);
    mu_check(!furry_string_empty(string));
    mu_check(furry_string_cmp(string, "more") == 0);
    furry_string_free(string);

    // test alloc_printf and free
    string = furry_string_alloc_printf("test %d %s %c 0x%02x", 1, "two", '3', 0x04);
    mu_check(string != NULL);
    mu_check(!furry_string_empty(string));
    mu_check(furry_string_cmp(string, "test 1 two 3 0x04") == 0);
    furry_string_free(string);

    // test alloc_vprintf and free
    string = furry_string_alloc_vprintf_test("test %d %s %c 0x%02x", 4, "five", '6', 0x07);
    mu_check(string != NULL);
    mu_check(!furry_string_empty(string));
    mu_check(furry_string_cmp(string, "test 4 five 6 0x07") == 0);
    furry_string_free(string);

    // test alloc_move and free
    tmp = furry_string_alloc_set("move");
    string = furry_string_alloc_move(tmp);
    mu_check(string != NULL);
    mu_check(!furry_string_empty(string));
    mu_check(furry_string_cmp(string, "move") == 0);
    furry_string_free(string);
}

MU_TEST(mu_test_furry_string_mem) {
    FurryString* string = furry_string_alloc_set("test");
    mu_check(string != NULL);
    mu_check(!furry_string_empty(string));

    // TODO: how to test furry_string_reserve?

    // test furry_string_reset
    furry_string_reset(string);
    mu_check(furry_string_empty(string));

    // test furry_string_swap
    furry_string_set(string, "test");
    FurryString* swap_string = furry_string_alloc_set("swap");
    furry_string_swap(string, swap_string);
    mu_check(furry_string_cmp(string, "swap") == 0);
    mu_check(furry_string_cmp(swap_string, "test") == 0);
    furry_string_free(swap_string);

    // test furry_string_move
    FurryString* move_string = furry_string_alloc_set("move");
    furry_string_move(string, move_string);
    mu_check(furry_string_cmp(string, "move") == 0);
    // move_string is now empty
    // and tested by leaked memory check at the end of the tests

    furry_string_set(string, "abracadabra");

    // test furry_string_hash
    mu_assert_int_eq(0xc3bc16d7, furry_string_hash(string));

    // test furry_string_size
    mu_assert_int_eq(11, furry_string_size(string));

    // test furry_string_empty
    mu_check(!furry_string_empty(string));
    furry_string_reset(string);
    mu_check(furry_string_empty(string));

    furry_string_free(string);
}

MU_TEST(mu_test_furry_string_getters) {
    FurryString* string = furry_string_alloc_set("test");

    // test furry_string_get_char
    mu_check(furry_string_get_char(string, 0) == 't');
    mu_check(furry_string_get_char(string, 1) == 'e');
    mu_check(furry_string_get_char(string, 2) == 's');
    mu_check(furry_string_get_char(string, 3) == 't');

    // test furry_string_get_cstr
    mu_assert_string_eq("test", furry_string_get_cstr(string));
    furry_string_free(string);
}

static FurryString* furry_string_vprintf_test(FurryString* string, const char format[], ...) {
    va_list args;
    va_start(args, format);
    furry_string_vprintf(string, format, args);
    va_end(args);
    return string;
}

MU_TEST(mu_test_furry_string_setters) {
    FurryString* tmp;
    FurryString* string = furry_string_alloc();

    // test furry_string_set_str
    furry_string_set_str(string, "test");
    mu_assert_string_eq("test", furry_string_get_cstr(string));

    // test furry_string_set
    tmp = furry_string_alloc_set("more");
    furry_string_set(string, tmp);
    furry_string_free(tmp);
    mu_assert_string_eq("more", furry_string_get_cstr(string));

    // test furry_string_set_strn
    furry_string_set_strn(string, "test", 2);
    mu_assert_string_eq("te", furry_string_get_cstr(string));

    // test furry_string_set_char
    furry_string_set_char(string, 0, 'a');
    furry_string_set_char(string, 1, 'b');
    mu_assert_string_eq("ab", furry_string_get_cstr(string));

    // test furry_string_set_n
    tmp = furry_string_alloc_set("dodecahedron");
    furry_string_set_n(string, tmp, 4, 5);
    furry_string_free(tmp);
    mu_assert_string_eq("cahed", furry_string_get_cstr(string));

    // test furry_string_printf
    furry_string_printf(string, "test %d %s %c 0x%02x", 1, "two", '3', 0x04);
    mu_assert_string_eq("test 1 two 3 0x04", furry_string_get_cstr(string));

    // test furry_string_vprintf
    furry_string_vprintf_test(string, "test %d %s %c 0x%02x", 4, "five", '6', 0x07);
    mu_assert_string_eq("test 4 five 6 0x07", furry_string_get_cstr(string));

    furry_string_free(string);
}

static FurryString* furry_string_cat_vprintf_test(FurryString* string, const char format[], ...) {
    va_list args;
    va_start(args, format);
    furry_string_cat_vprintf(string, format, args);
    va_end(args);
    return string;
}

MU_TEST(mu_test_furry_string_appends) {
    FurryString* tmp;
    FurryString* string = furry_string_alloc();

    // test furry_string_push_back
    furry_string_push_back(string, 't');
    furry_string_push_back(string, 'e');
    furry_string_push_back(string, 's');
    furry_string_push_back(string, 't');
    mu_assert_string_eq("test", furry_string_get_cstr(string));
    furry_string_push_back(string, '!');
    mu_assert_string_eq("test!", furry_string_get_cstr(string));

    // test furry_string_cat_str
    furry_string_cat_str(string, "test");
    mu_assert_string_eq("test!test", furry_string_get_cstr(string));

    // test furry_string_cat
    tmp = furry_string_alloc_set("more");
    furry_string_cat(string, tmp);
    furry_string_free(tmp);
    mu_assert_string_eq("test!testmore", furry_string_get_cstr(string));

    // test furry_string_cat_printf
    furry_string_cat_printf(string, "test %d %s %c 0x%02x", 1, "two", '3', 0x04);
    mu_assert_string_eq("test!testmoretest 1 two 3 0x04", furry_string_get_cstr(string));

    // test furry_string_cat_vprintf
    furry_string_cat_vprintf_test(string, "test %d %s %c 0x%02x", 4, "five", '6', 0x07);
    mu_assert_string_eq(
        "test!testmoretest 1 two 3 0x04test 4 five 6 0x07", furry_string_get_cstr(string));

    furry_string_free(string);
}

MU_TEST(mu_test_furry_string_compare) {
    FurryString* string_1 = furry_string_alloc_set("string_1");
    FurryString* string_2 = furry_string_alloc_set("string_2");

    // test furry_string_cmp
    mu_assert_int_eq(0, furry_string_cmp(string_1, string_1));
    mu_assert_int_eq(0, furry_string_cmp(string_2, string_2));
    mu_assert_int_eq(-1, furry_string_cmp(string_1, string_2));
    mu_assert_int_eq(1, furry_string_cmp(string_2, string_1));

    // test furry_string_cmp_str
    mu_assert_int_eq(0, furry_string_cmp_str(string_1, "string_1"));
    mu_assert_int_eq(0, furry_string_cmp_str(string_2, "string_2"));
    mu_assert_int_eq(-1, furry_string_cmp_str(string_1, "string_2"));
    mu_assert_int_eq(1, furry_string_cmp_str(string_2, "string_1"));

    // test furry_string_cmpi
    furry_string_set(string_1, "string");
    furry_string_set(string_2, "StrIng");
    mu_assert_int_eq(0, furry_string_cmpi(string_1, string_1));
    mu_assert_int_eq(0, furry_string_cmpi(string_2, string_2));
    mu_assert_int_eq(0, furry_string_cmpi(string_1, string_2));
    mu_assert_int_eq(0, furry_string_cmpi(string_2, string_1));
    furry_string_set(string_1, "string_1");
    furry_string_set(string_2, "StrIng_2");
    mu_assert_int_eq(32, furry_string_cmp(string_1, string_2));
    mu_assert_int_eq(-32, furry_string_cmp(string_2, string_1));
    mu_assert_int_eq(-1, furry_string_cmpi(string_1, string_2));
    mu_assert_int_eq(1, furry_string_cmpi(string_2, string_1));

    // test furry_string_cmpi_str
    furry_string_set(string_1, "string");
    mu_assert_int_eq(0, furry_string_cmp_str(string_1, "string"));
    mu_assert_int_eq(32, furry_string_cmp_str(string_1, "String"));
    mu_assert_int_eq(32, furry_string_cmp_str(string_1, "STring"));
    mu_assert_int_eq(32, furry_string_cmp_str(string_1, "STRing"));
    mu_assert_int_eq(32, furry_string_cmp_str(string_1, "STRIng"));
    mu_assert_int_eq(32, furry_string_cmp_str(string_1, "STRINg"));
    mu_assert_int_eq(32, furry_string_cmp_str(string_1, "STRING"));
    mu_assert_int_eq(0, furry_string_cmpi_str(string_1, "string"));
    mu_assert_int_eq(0, furry_string_cmpi_str(string_1, "String"));
    mu_assert_int_eq(0, furry_string_cmpi_str(string_1, "STring"));
    mu_assert_int_eq(0, furry_string_cmpi_str(string_1, "STRing"));
    mu_assert_int_eq(0, furry_string_cmpi_str(string_1, "STRIng"));
    mu_assert_int_eq(0, furry_string_cmpi_str(string_1, "STRINg"));
    mu_assert_int_eq(0, furry_string_cmpi_str(string_1, "STRING"));

    furry_string_free(string_1);
    furry_string_free(string_2);
}

MU_TEST(mu_test_furry_string_search) {
    //                                            012345678901234567
    FurryString* haystack = furry_string_alloc_set("test321test123test");
    FurryString* needle = furry_string_alloc_set("test");

    // test furry_string_search
    mu_assert_int_eq(0, furry_string_search(haystack, needle));
    mu_assert_int_eq(7, furry_string_search(haystack, needle, 1));
    mu_assert_int_eq(14, furry_string_search(haystack, needle, 8));
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_search(haystack, needle, 15));

    FurryString* tmp = furry_string_alloc_set("testnone");
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_search(haystack, tmp));
    furry_string_free(tmp);

    // test furry_string_search_str
    mu_assert_int_eq(0, furry_string_search_str(haystack, "test"));
    mu_assert_int_eq(7, furry_string_search_str(haystack, "test", 1));
    mu_assert_int_eq(14, furry_string_search_str(haystack, "test", 8));
    mu_assert_int_eq(4, furry_string_search_str(haystack, "321"));
    mu_assert_int_eq(11, furry_string_search_str(haystack, "123"));
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_search_str(haystack, "testnone"));
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_search_str(haystack, "test", 15));

    // test furry_string_search_char
    mu_assert_int_eq(0, furry_string_search_char(haystack, 't'));
    mu_assert_int_eq(1, furry_string_search_char(haystack, 'e'));
    mu_assert_int_eq(2, furry_string_search_char(haystack, 's'));
    mu_assert_int_eq(3, furry_string_search_char(haystack, 't', 1));
    mu_assert_int_eq(7, furry_string_search_char(haystack, 't', 4));
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_search_char(haystack, 'x'));

    // test furry_string_search_rchar
    mu_assert_int_eq(17, furry_string_search_rchar(haystack, 't'));
    mu_assert_int_eq(15, furry_string_search_rchar(haystack, 'e'));
    mu_assert_int_eq(16, furry_string_search_rchar(haystack, 's'));
    mu_assert_int_eq(13, furry_string_search_rchar(haystack, '3'));
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_search_rchar(haystack, '3', 14));
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_search_rchar(haystack, 'x'));

    furry_string_free(haystack);
    furry_string_free(needle);
}

MU_TEST(mu_test_furry_string_equality) {
    FurryString* string = furry_string_alloc_set("test");
    FurryString* string_eq = furry_string_alloc_set("test");
    FurryString* string_neq = furry_string_alloc_set("test2");

    // test furry_string_equal
    mu_check(furry_string_equal(string, string_eq));
    mu_check(!furry_string_equal(string, string_neq));

    // test furry_string_equal_str
    mu_check(furry_string_equal_str(string, "test"));
    mu_check(!furry_string_equal_str(string, "test2"));
    mu_check(furry_string_equal_str(string_neq, "test2"));
    mu_check(!furry_string_equal_str(string_neq, "test"));

    furry_string_free(string);
    furry_string_free(string_eq);
    furry_string_free(string_neq);
}

MU_TEST(mu_test_furry_string_replace) {
    FurryString* needle = furry_string_alloc_set("test");
    FurryString* replace = furry_string_alloc_set("replace");
    FurryString* string = furry_string_alloc_set("test123test");

    // test furry_string_replace_at
    furry_string_replace_at(string, 4, 3, "!biglongword!");
    mu_assert_string_eq("test!biglongword!test", furry_string_get_cstr(string));

    // test furry_string_replace
    mu_assert_int_eq(17, furry_string_replace(string, needle, replace, 1));
    mu_assert_string_eq("test!biglongword!replace", furry_string_get_cstr(string));
    mu_assert_int_eq(0, furry_string_replace(string, needle, replace));
    mu_assert_string_eq("replace!biglongword!replace", furry_string_get_cstr(string));
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_replace(string, needle, replace));
    mu_assert_string_eq("replace!biglongword!replace", furry_string_get_cstr(string));

    // test furry_string_replace_str
    mu_assert_int_eq(20, furry_string_replace_str(string, "replace", "test", 1));
    mu_assert_string_eq("replace!biglongword!test", furry_string_get_cstr(string));
    mu_assert_int_eq(0, furry_string_replace_str(string, "replace", "test"));
    mu_assert_string_eq("test!biglongword!test", furry_string_get_cstr(string));
    mu_assert_int_eq(FURRY_STRING_FAILURE, furry_string_replace_str(string, "replace", "test"));
    mu_assert_string_eq("test!biglongword!test", furry_string_get_cstr(string));

    // test furry_string_replace_all
    furry_string_replace_all(string, needle, replace);
    mu_assert_string_eq("replace!biglongword!replace", furry_string_get_cstr(string));

    // test furry_string_replace_all_str
    furry_string_replace_all_str(string, "replace", "test");
    mu_assert_string_eq("test!biglongword!test", furry_string_get_cstr(string));

    furry_string_free(string);
    furry_string_free(needle);
    furry_string_free(replace);
}

MU_TEST(mu_test_furry_string_start_end) {
    FurryString* string = furry_string_alloc_set("start_end");
    FurryString* start = furry_string_alloc_set("start");
    FurryString* end = furry_string_alloc_set("end");

    // test furry_string_start_with
    mu_check(furry_string_start_with(string, start));
    mu_check(!furry_string_start_with(string, end));

    // test furry_string_start_with_str
    mu_check(furry_string_start_with_str(string, "start"));
    mu_check(!furry_string_start_with_str(string, "end"));

    // test furry_string_end_with
    mu_check(furry_string_end_with(string, end));
    mu_check(!furry_string_end_with(string, start));

    // test furry_string_end_with_str
    mu_check(furry_string_end_with_str(string, "end"));
    mu_check(!furry_string_end_with_str(string, "start"));

    furry_string_free(string);
    furry_string_free(start);
    furry_string_free(end);
}

MU_TEST(mu_test_furry_string_trim) {
    FurryString* string = furry_string_alloc_set("biglongstring");

    // test furry_string_left
    furry_string_left(string, 7);
    mu_assert_string_eq("biglong", furry_string_get_cstr(string));

    // test furry_string_right
    furry_string_right(string, 3);
    mu_assert_string_eq("long", furry_string_get_cstr(string));

    // test furry_string_mid
    furry_string_mid(string, 1, 2);
    mu_assert_string_eq("on", furry_string_get_cstr(string));

    // test furry_string_trim
    furry_string_set(string, "   \n\r\tbiglongstring \n\r\t  ");
    furry_string_trim(string);
    mu_assert_string_eq("biglongstring", furry_string_get_cstr(string));
    furry_string_set(string, "aaaabaaaabbaaabaaaabbtestaaaaaabbaaabaababaa");
    furry_string_trim(string, "ab");
    mu_assert_string_eq("test", furry_string_get_cstr(string));

    furry_string_free(string);
}

MU_TEST(mu_test_furry_string_utf8) {
    FurryString* utf8_string = furry_string_alloc_set("イルカ");

    // test furry_string_utf8_length
    mu_assert_int_eq(9, furry_string_size(utf8_string));
    mu_assert_int_eq(3, furry_string_utf8_length(utf8_string));

    // test furry_string_utf8_decode
    const uint8_t dolphin_emoji_array[4] = {0xF0, 0x9F, 0x90, 0xAC};
    FurryStringUTF8State state = FurryStringUTF8StateStarting;
    FurryStringUnicodeValue value = 0;
    furry_string_utf8_decode(dolphin_emoji_array[0], &state, &value);
    mu_assert_int_eq(FurryStringUTF8StateDecoding3, state);
    furry_string_utf8_decode(dolphin_emoji_array[1], &state, &value);
    mu_assert_int_eq(FurryStringUTF8StateDecoding2, state);
    furry_string_utf8_decode(dolphin_emoji_array[2], &state, &value);
    mu_assert_int_eq(FurryStringUTF8StateDecoding1, state);
    furry_string_utf8_decode(dolphin_emoji_array[3], &state, &value);
    mu_assert_int_eq(FurryStringUTF8StateStarting, state);
    mu_assert_int_eq(0x1F42C, value);

    // test furry_string_utf8_push
    furry_string_set(utf8_string, "");
    furry_string_utf8_push(utf8_string, value);
    mu_assert_string_eq("🐬", furry_string_get_cstr(utf8_string));

    furry_string_free(utf8_string);
}

MU_TEST_SUITE(test_suite) {
    MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

    MU_RUN_TEST(mu_test_furry_string_alloc_free);
    MU_RUN_TEST(mu_test_furry_string_mem);
    MU_RUN_TEST(mu_test_furry_string_getters);
    MU_RUN_TEST(mu_test_furry_string_setters);
    MU_RUN_TEST(mu_test_furry_string_appends);
    MU_RUN_TEST(mu_test_furry_string_compare);
    MU_RUN_TEST(mu_test_furry_string_search);
    MU_RUN_TEST(mu_test_furry_string_equality);
    MU_RUN_TEST(mu_test_furry_string_replace);
    MU_RUN_TEST(mu_test_furry_string_start_end);
    MU_RUN_TEST(mu_test_furry_string_trim);
    MU_RUN_TEST(mu_test_furry_string_utf8);
}

int run_minunit_test_furry_string() {
    MU_RUN_SUITE(test_suite);

    return MU_EXIT_CODE;
}