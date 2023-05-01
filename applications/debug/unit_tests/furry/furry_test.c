#include <stdio.h>
#include <furry.h>
#include <furry_hal.h>
#include "../minunit.h"

// v2 tests
void test_furry_create_open();
void test_furry_concurrent_access();
void test_furry_pubsub();

void test_furry_memmgr();

static int foo = 0;

void test_setup(void) {
    foo = 7;
}

void test_teardown(void) {
    /* Nothing */
}

MU_TEST(test_check) {
    mu_check(foo != 6);
}

// v2 tests
MU_TEST(mu_test_furry_create_open) {
    test_furry_create_open();
}

MU_TEST(mu_test_furry_pubsub) {
    test_furry_pubsub();
}

MU_TEST(mu_test_furry_memmgr) {
    // this test is not accurate, but gives a basic understanding
    // that memory management is working fine
    test_furry_memmgr();
}

MU_TEST_SUITE(test_suite) {
    MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

    MU_RUN_TEST(test_check);

    // v2 tests
    MU_RUN_TEST(mu_test_furry_create_open);
    MU_RUN_TEST(mu_test_furry_pubsub);
    MU_RUN_TEST(mu_test_furry_memmgr);
}

int run_minunit_test_furry() {
    MU_RUN_SUITE(test_suite);

    return MU_EXIT_CODE;
}
