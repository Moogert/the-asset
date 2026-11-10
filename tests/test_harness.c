#include "harness.h"
#include "json_util.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            (void)fprintf(stderr, "FAIL: %s (%s:%d)\n", #x, __FILE__, __LINE__);                   \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static void make_ready(harness_t* h)
{
    h->inputs.arm = true;
    h->inputs.drive_request = true;
    h->inputs.precharge_ok = true;
    h->inputs.throttle_a_percent = 30.0F;
    h->inputs.throttle_b_percent = 30.0F;
}

static void advance(harness_t* h, unsigned count)
{
    for (unsigned i = 0U; i < count; i++)
        harness_step(h, 10U);
}

static void test_safe_start(void)
{
    harness_t h;
    harness_init(&h);
    advance(&h, 5U);
    CHECK(!h.safety.torque_authorized);
    CHECK(!h.safety.charger_authorized);
    CHECK(!h.safety.contactor_command);
}

static void test_nominal_drive(void)
{
    harness_t h;
    harness_init(&h);
    make_ready(&h);
    advance(&h, 5U);
    CHECK(h.safety.torque_authorized);
    CHECK(h.applied_torque_percent > 0.0F);
    CHECK(h.safety.faults == 0U);
}

static void test_bms_timeout(void)
{
    harness_t h;
    harness_init(&h);
    make_ready(&h);
    harness_set_fault(&h, "bms_timeout", true);
    advance(&h, 40U);
    CHECK(!h.safety.torque_authorized);
    CHECK((h.safety.faults & FAULT_BMS_STALE) != 0U);
}

static void test_throttle_mismatch(void)
{
    harness_t h;
    harness_init(&h);
    make_ready(&h);
    h.inputs.throttle_b_percent = 70.0F;
    advance(&h, 5U);
    CHECK(!h.safety.torque_authorized);
    CHECK((h.safety.faults & FAULT_THROTTLE) != 0U);
    CHECK(h.safety.latched);
}

static void test_bus_off(void)
{
    harness_t h;
    harness_init(&h);
    make_ready(&h);
    harness_set_fault(&h, "bus_off", true);
    advance(&h, 5U);
    CHECK(!h.safety.torque_authorized);
    CHECK((h.safety.faults & FAULT_BUS_OFF) != 0U);
}

static void test_limit(void)
{
    harness_t h;
    harness_init(&h);
    h.inputs.arm = true;
    h.inputs.charge_request = true;
    h.inputs.ac_present = true;
    h.inputs.bms.charge_limit_a = 7.0F;
    advance(&h, 5U);
    CHECK(h.safety.charger_authorized);
    CHECK(h.applied_charge_current_a <= 7.0F);
}

static void test_inverter_faults(void)
{
    harness_t h;
    harness_init(&h);
    make_ready(&h);
    harness_set_fault(&h, "inverter_desat", true);
    advance(&h, 5U);
    CHECK(!h.safety.torque_authorized);
    CHECK((h.safety.faults & FAULT_INVERTER) != 0U);
    CHECK(h.safety.latched);
}

static void test_hv_and_estop(void)
{
    harness_t h;
    harness_init(&h);
    make_ready(&h);
    harness_set_fault(&h, "hvil_open", true);
    advance(&h, 5U);
    CHECK(!h.safety.torque_authorized);
    CHECK((h.safety.faults & FAULT_HVIL) != 0U);
    harness_reset(&h);
    make_ready(&h);
    harness_set_fault(&h, "estop", true);
    advance(&h, 5U);
    CHECK(!h.safety.torque_authorized);
    CHECK((h.safety.faults & FAULT_EMERGENCY_STOP) != 0U);
}

static void test_conflict_and_welded(void)
{
    harness_t h;
    harness_init(&h);
    make_ready(&h);
    harness_set_fault(&h, "conflicting_command", true);
    advance(&h, 5U);
    CHECK(!h.safety.torque_authorized);
    CHECK(!h.safety.charger_authorized);
    CHECK((h.safety.faults & FAULT_CONTRADICTORY) != 0U);
    harness_reset(&h);
    harness_set_fault(&h, "contactor_welded", true);
    advance(&h, 5U);
    CHECK((h.safety.faults & FAULT_CONTACTOR) != 0U);
    CHECK(h.safety.latched);
}

static void test_frame_integrity(void)
{
    harness_t h;
    harness_init(&h);
    make_ready(&h);
    harness_set_fault(&h, "bad_crc", true);
    advance(&h, 30U);
    CHECK(!h.safety.torque_authorized);
    CHECK((h.safety.faults & FAULT_BMS_STALE) != 0U || (h.safety.faults & FAULT_BAD_FRAME) != 0U);
}

static void test_parser_bounds(void)
{
    bool b = false;
    double n = 0.0;
    char s[8];
    const char* json = "{\"arm\":true,\"value\":12.5,\"name\":\"ok\"}";
    CHECK(json_is_safe_document(json, strlen(json)));
    CHECK(json_get_bool(json, strlen(json), "arm", &b) && b);
    CHECK(json_get_number(json, strlen(json), "value", &n) && n == 12.5);
    CHECK(json_get_string(json, strlen(json), "name", s, sizeof(s)));
    CHECK(!json_is_safe_document("{", 1U));
    CHECK(!json_get_string(json, strlen(json), "name", s, 2U));
}

int main(void)
{
    test_safe_start();
    test_nominal_drive();
    test_bms_timeout();
    test_throttle_mismatch();
    test_bus_off();
    test_limit();
    test_inverter_faults();
    test_hv_and_estop();
    test_conflict_and_welded();
    test_frame_integrity();
    test_parser_bounds();
    if (failures != 0)
    {
        (void)fprintf(stderr, "%d test failures\n", failures);
        return 1;
    }
    (void)printf("All harness tests passed.\n");
    return 0;
}
