// Host-only output/interface tests: no Arduino, harness or ECU is connected.
#include "emulator_io.h"
#include "indication.h"
#include "emulator_pins.h"
#include <stdio.h>
#include <string.h>
static unsigned checks = 0, failures = 0;
#define CHECK(value)                                                                               \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(value))                                                                              \
        {                                                                                          \
            ++failures;                                                                            \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value);                                    \
        }                                                                                          \
    } while (0)
using namespace Civic;
static Sensors normal_sensors()
{
    Sensors s = {};
    s.enabled = true;
    s.millivolts[0] = 2500;
    s.millivolts[1] = 1000;
    s.millivolts[2] = 800;
    s.millivolts[3] = 550;
    s.resistance_index[0] = 3;
    s.resistance_index[1] = 2;
    return s;
}
static void output_tests()
{
    Outputprofile p = synthetic_output_profile();
    Sensors s = normal_sensors();
    CHECK(valid_output_profile(p));
    CHECK(!plan_outputs(s, p, false).enabled);
    Outputplan out = plan_outputs(s, p, true);
    CHECK(out.enabled && !out.fault);
    CHECK(out.dac_code[0] == 2048); // 2500*4096/5000; MCP4922 half scale.
    CHECK(out.dac_code[2] == 655);  // floor(800*4096/5000), not 5 V at the O2 output.
    CHECK(out.connect[4] && out.connect[5]);
    CHECK(!out.connect[6] && !out.connect[7]);
    for (unsigned channel = 0; channel < 6; ++channel)
    {
        s = normal_sensors();
        s.fault[channel] = open_circuit;
        out = plan_outputs(s, p, true);
        CHECK(out.enabled && !out.connect[channel]);
        s.fault[channel] = stuck_high;
        out = plan_outputs(s, p, true);
        CHECK(out.enabled && out.connect[channel]);
        if (channel < 4)
            CHECK(out.dac_code[channel] == (channel < 2 ? 4095 : 819));
        else
            CHECK(out.resistance_index[channel - 4] == 7);
        s.fault[channel] = stuck_low;
        out = plan_outputs(s, p, true);
        if (channel < 4)
            CHECK(out.dac_code[channel] == 0);
        else
            CHECK(out.resistance_index[channel - 4] == 0);
    }
    s = normal_sensors();
    s.millivolts[2] = 1001;
    CHECK(plan_outputs(s, p, true).fault);
    CHECK(!plan_outputs(s, p, true).enabled);
    s = normal_sensors();
    s.resistance_index[0] = 8;
    CHECK(plan_outputs(s, p, true).fault);
    s = normal_sensors();
    s.fault[2] = static_cast<Inject>(99);
    CHECK(plan_outputs(s, p, true).fault);
    s = normal_sensors();
    s.enabled = false;
    CHECK(!plan_outputs(s, p, true).enabled);
    p.voltage[4].fitted = true;
    CHECK(!valid_output_profile(p));
    p = synthetic_output_profile();
    p.dac_reference_mv = 0;
    CHECK(!valid_output_profile(p));
    p = synthetic_output_profile();
    p.synthetic = false;
    CHECK(!valid_output_profile(p));
    p.verified = true;
    CHECK(!valid_output_profile(p)); // Production ladder ohms still unknown.
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned j = 0; j < 8; ++j)
            p.resistance[i].ohms[j] = 100 + j * 100;
    CHECK(valid_output_profile(p));
    CHECK(plan_outputs(normal_sensors(), p, false).enabled);
    p.resistance[0].low_fault_index = p.resistance[0].high_fault_index;
    CHECK(!valid_output_profile(p));
}
static void command_tests()
{
    Engine e(synthetic_profile());
    Engineinstruction in = {};
    in.operation = arm_operation;
    CHECK(!e.apply_command(in, 0));
    CHECK(e.prepare_command("ARM BENCH", true, in));
    CHECK(!e.armed());
    CHECK(e.apply_command(in, 10));
    CHECK(e.armed());
    CHECK(e.prepare_command("STATE RUN", true, in));
    CHECK(e.mode() == stopped);
    CHECK(e.apply_command(in, 10));
    CHECK(e.mode() == running);
    CHECK(e.prepare_command("RPM 1200", true, in));
    CHECK(e.rpm() == 800);
    CHECK(e.apply_command(in, 20));
    CHECK(e.rpm() == 1200);
    CHECK(!e.prepare_command("RPM 999999999999999999999", true, in));
    CHECK(!e.prepare_command("ARM VEHICLE", true, in));
    CHECK(!e.prepare_command("FAULT 8 OPEN", true, in));
    CHECK(!e.prepare_command("STATE RUN extra", true, in));
    CHECK(e.command("FAULT 4 HIGH", 20, true));
    CHECK(plan_outputs(e.sensors(20), synthetic_output_profile(), true).resistance_index[0] == 7);
    CHECK(e.command("FAULT 5 LOW", 20, true));
    CHECK(plan_outputs(e.sensors(20), synthetic_output_profile(), true).resistance_index[1] == 0);
    e.update(521, true);
    CHECK(!e.armed());
    Indicationprofile profile = demonstration_indication_profile();
    profile.mapping[7].available = false;
    Indicationreceiver receiver(profile);
    char command[96], ack[96];
    CHECK(indication_command(1, 1, 128, command, sizeof(command)) > 0);
    CHECK(receiver.receive(command, 0, true, e, ack, sizeof(ack)));
    CHECK(receiver.status(0).unavailable == 128 && receiver.status(0).applied == 0);
    CHECK(Emulatorpins::ect_select[0] == 42 && Emulatorpins::channel_connect[ect_channel] == 38);
    CHECK(Emulatorpins::gateway_tx == 18 && Emulatorpins::gateway_rx == 19);
}
int main()
{
    output_tests();
    command_tests();
    printf("emulator interface: %u checks, %u failures (synthetic host evidence only)\n",
           checks,
           failures);
    return failures ? 1 : 0;
}
