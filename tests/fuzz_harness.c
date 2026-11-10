#include "harness.h"
#include "json_util.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == NULL || size > 16384U)
        return 0;
    (void)json_is_safe_document((const char*)data, size);
    char value[48];
    bool enabled = false;
    double number = 0.0;
    (void)json_get_string((const char*)data, size, "name", value, sizeof(value));
    (void)json_get_bool((const char*)data, size, "enabled", &enabled);
    (void)json_get_number((const char*)data, size, "value", &number);
    harness_t h;
    harness_init(&h);
    if (enabled)
        harness_set_fault(&h, "bad_crc", true);
    for (unsigned i = 0U; i < 3U; ++i)
        harness_step(&h, 10U);
    return 0;
}
