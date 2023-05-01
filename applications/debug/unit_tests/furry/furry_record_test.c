#include <stdio.h>
#include <string.h>
#include <furry.h>
#include "../minunit.h"

void test_furry_create_open() {
    // 1. Create record
    uint8_t test_data = 0;
    furry_record_create("test/holding", (void*)&test_data);

    // 2. Open it
    void* record = furry_record_open("test/holding");
    mu_assert_pointers_eq(record, &test_data);

    // 3. Close it
    furry_record_close("test/holding");

    // 4. Clean up
    furry_record_destroy("test/holding");
}
