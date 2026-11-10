#include "dual_drive.h"
#include "harness.h"
#include "prius_inverter_adapter.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)
#define NEAR(a, b) CHECK(fabsf((a) - (b)) < 0.001F)
static dual_inputs_t ready(uint64_t now)
{
    dual_inputs_t in = {0};
    in.arm = true;
    in.drive_request = true;
    in.permission = true;
    in.mode = DUAL_COMBINED;
    in.request_percent = 100.0F;
    in.discharge_limit_a = 200.0F;
    in.charge_limit_a = 20.0F;
    for (unsigned i = 0U; i < 2U; ++i)
    {
        in.motor[i].valid = true;
        in.motor[i].ready = true;
        in.motor[i].rotor_valid = true;
        in.motor[i].received_ms = now;
        in.motor[i].motor_temp_c = 25.0F;
        in.motor[i].inverter_temp_c = 25.0F;
    }
    return in;
}
static void stopped(const dual_outputs_t* o)
{
    CHECK(!o->enabled);
    NEAR(o->command_percent[0], 0.0F);
    NEAR(o->command_percent[1], 0.0F);
}
static void modes_and_limits(void)
{
    const dual_config_t cfg = dual_simulation_config();
    dual_inputs_t in = ready(10U);
    dual_outputs_t o = {0};
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    CHECK(o.enabled);
    NEAR(o.command_percent[0], 80.0F);
    NEAR(o.command_percent[1], 80.0F);
    NEAR(o.allocated_pack_a[0] + o.allocated_pack_a[1], 200.0F);
    in.mode = DUAL_MG1_ONLY;
    memset(&o, 0, sizeof(o));
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    NEAR(o.command_percent[0], 100.0F);
    NEAR(o.command_percent[1], 0.0F);
    in.mode = DUAL_MG2_ONLY;
    memset(&o, 0, sizeof(o));
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    NEAR(o.command_percent[0], 0.0F);
    NEAR(o.command_percent[1], 100.0F);
    in.mode = DUAL_COMBINED;
    in.request_percent = -100.0F;
    memset(&o, 0, sizeof(o));
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    NEAR(o.command_percent[0], -20.0F);
    NEAR(o.command_percent[1], -20.0F);
    NEAR(o.allocated_pack_a[0] + o.allocated_pack_a[1], -20.0F);
    in.charge_limit_a = 0.0F;
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    stopped(&o);
    CHECK(!o.latched);
    in.discharge_limit_a = 0.0F;
    in.request_percent = 100.0F;
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    stopped(&o);
    /* Sweep requests/budgets: aggregate allocation must never exceed either budget. */
    for (int request = -100; request <= 100; request += 5)
    {
        for (unsigned budget = 0U; budget <= 250U; budget += 5U)
        {
            in = ready(10U);
            in.request_percent = (float)request;
            in.discharge_limit_a = (float)budget;
            in.charge_limit_a = (float)budget;
            memset(&o, 0, sizeof(o));
            dual_drive_evaluate(&cfg, &in, 10U, &o);
            CHECK(fabsf(o.allocated_pack_a[0] + o.allocated_pack_a[1]) <= (float)budget + 0.001F);
            CHECK(o.command_percent[0] * o.command_percent[1] >= 0.0F);
        }
    }
}
static void faults_and_rearm(void)
{
    const dual_config_t cfg = dual_simulation_config();
    for (unsigned i = 0U; i < 2U; ++i)
    {
        for (unsigned fault = 0U; fault < 7U; ++fault)
        {
            dual_inputs_t in = ready(100U);
            dual_outputs_t o = {0};
            if (fault == 0U)
                in.motor[i].received_ms = 0U;
            if (fault == 1U)
                in.motor[i].rotor_valid = false;
            if (fault == 2U)
                in.motor[i].fault = true;
            if (fault == 3U)
                in.motor[i].rpm = -5000.0F;
            if (fault == 4U)
                in.motor[i].motor_temp_c = 90.0F;
            if (fault == 5U)
                in.motor[i].rpm = NAN;
            if (fault == 6U)
                in.motor[i].received_ms = 101U;
            dual_drive_evaluate(&cfg, &in, 100U, &o);
            stopped(&o);
            CHECK(o.latched);
            in = ready(110U);
            dual_drive_evaluate(&cfg, &in, 110U, &o);
            stopped(&o);
            CHECK(o.latched_faults != 0U);
            in.reset_request = true;
            dual_drive_evaluate(&cfg, &in, 110U, &o);
            stopped(&o);
            in.arm = false;
            in.drive_request = false;
            in.request_percent = 0.0F;
            dual_drive_evaluate(&cfg, &in, 110U, &o);
            CHECK(!o.latched);
            CHECK(o.latched_faults == 0U);
            in = ready(120U);
            dual_drive_evaluate(&cfg, &in, 120U, &o);
            CHECK(o.enabled);
        }
    }
    dual_inputs_t in = ready(100U);
    dual_outputs_t o = {0};
    in.motor[0].received_ms = 1U;
    dual_drive_evaluate(&cfg, &in, 100U, &o);
    CHECK(o.enabled);
    in.motor[1].received_ms = 101U;
    dual_drive_evaluate(&cfg, &in, 101U, &o);
    stopped(&o);
    CHECK(o.faults & DUAL_STALE); /* exact 100 ms boundary */
    in = ready(200U);
    memset(&o, 0, sizeof(o));
    dual_drive_evaluate(&cfg, &in, 200U, &o);
    in = ready(301U);
    dual_drive_evaluate(&cfg, &in, 301U, &o);
    CHECK(o.faults & DUAL_SCHEDULER);
    in = ready(10U);
    memset(&o, 0, sizeof(o));
    in.motor[0].dc_current_a = 10.0F;
    in.motor[1].dc_current_a = -10.0F;
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    CHECK(o.faults & DUAL_CIRCULATION);
    stopped(&o);
    in = ready(10U);
    memset(&o, 0, sizeof(o));
    in.pack_current_a = -30.0F;
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    CHECK(o.faults & DUAL_PACK);
    stopped(&o);
    in = ready(10U);
    memset(&o, 0, sizeof(o));
    in.permission = false;
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    stopped(&o);
    CHECK(o.latched);
    in = ready(10U);
    memset(&o, 0, sizeof(o));
    in.motor[0].motor_temp_c = 80.0F;
    in.discharge_limit_a = 500.0F;
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    NEAR(o.command_percent[0], 50.0F);
    NEAR(o.command_percent[1], 100.0F);
    in.mode = DUAL_MG2_ONLY;
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    CHECK(o.faults & DUAL_CONFIG);
    dual_config_t bad = cfg;
    bad.draw_a[0] = NAN;
    memset(&o, 0, sizeof(o));
    dual_drive_evaluate(&bad, &in, 10U, &o);
    stopped(&o);
    CHECK(o.faults & DUAL_CONFIG);
    in = ready(10U);
    memset(&o, 0, sizeof(o));
    in.motor[0].rpm = -1000;
    in.motor[1].rpm = 1000;
    dual_drive_evaluate(&cfg, &in, 10U, &o);
    stopped(&o);
    CHECK(o.faults & DUAL_KINEMATICS);
}
static void harness_integration(void)
{
    const char* faults[] = {"mg1_timeout",
                            "mg2_timeout",
                            "mg1_fault",
                            "mg2_fault",
                            "mg1_resolver",
                            "mg2_resolver",
                            "replay",
                            "bus_off",
                            "estop",
                            "bms_timeout"};
    for (unsigned f = 0U; f < sizeof(faults) / sizeof(faults[0]); ++f)
    {
        harness_t h;
        harness_init(&h);
        CHECK(harness_run_scenario(&h, "combined_drive"));
        CHECK(h.dual.enabled);
        harness_set_fault(&h, faults[f], true);
        for (unsigned t = 0U; t < 10U; ++t)
            harness_step(&h, 10U);
        stopped(&h.dual);
        CHECK(!h.safety.torque_authorized);
        CHECK(h.safety.latched);
        harness_clear_faults(&h);
        harness_step(&h, 10U);
        stopped(&h.dual);
    }
    harness_t h;
    CHECK(harness_run_scenario(&h, "combined_regen"));
    CHECK(h.dual.enabled);
    CHECK(h.dual.command_percent[0] < 0.0F);
    h.inputs.bms.charge_limit_a = 0.0F;
    harness_step(&h, 10U);
    stopped(&h.dual);
    CHECK(harness_run_scenario(&h, "mg1_only"));
    CHECK(h.dual.command_percent[0] > 0.0F);
    NEAR(h.dual.command_percent[1], 0.0F);
    char json[4096];
    CHECK(harness_state_json(&h, json, sizeof(json)) > 0U);
    CHECK(strstr(json, "mg1_command_percent") != NULL);
    CHECK(harness_state_json(&h, json, 2U) == 0U);
}
static void codec(void)
{
    /* Published CRC-32/MPEG-2 check: ASCII 123456789 = 0376E6E7.
       Hardware CRC word algorithm independently checked with 12345678 = 49E3C2FB. */
    const uint32_t ascii[2] = {0x31323334U, 0x35363738U};
    CHECK(oi_crc_words(ascii, 2U) == 0x49E3C2FBU);
    oi_control_t c = {1234U, 2345U, 8U, 1U, 25U};
    uint8_t bytes[8] = {0};
    CHECK(oi_encode_control(&c, bytes));
    const uint32_t a = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
                       ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
    const uint32_t b =
        (uint32_t)bytes[4] | ((uint32_t)bytes[5] << 8U) | ((uint32_t)bytes[6] << 16U);
    CHECK((a & 4095U) == 1234U);
    CHECK(((a >> 12U) & 4095U) == 2345U);
    CHECK(((a >> 30U) & 3U) == 1U);
    CHECK(((b >> 14U) & 3U) == 1U);
    CHECK(((b >> 16U) & 255U) == 25U);
    const uint32_t words[2] = {a, b};
    CHECK((oi_crc_words(words, 2U) & 255U) == bytes[7]);
    c.throttle_a = 4096U;
    CHECK(!oi_encode_control(&c, bytes));
    c.throttle_a = 0U;
    c.io_bits = 24U;
    CHECK(!oi_encode_control(&c, bytes));
    CHECK(!oi_encode_control(NULL, bytes));
}
int main(void)
{
    modes_and_limits();
    faults_and_rearm();
    harness_integration();
    codec();
    if (failures)
    {
        fprintf(stderr, "%d failures\n", failures);
        return 1;
    }
    puts("Dual drive allocation, fault, integration and offline codec tests passed.");
    return 0;
}
