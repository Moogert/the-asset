// Host-only comparison and mocked-I/O tests; outside the JSF source scope.
#include <control/control_runtime.h>
extern "C"
{
#include "dual_drive.h"
#include "prius_inverter_adapter.h"
}
#include <stdio.h>
#include <math.h>
#include <limits>
#include <string.h>

static int failures = 0;
static unsigned comparisons = 0U;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)
#define NEAR(x, y) CHECK(fabs(static_cast<double>((x) - (y))) < 0.001)

static Capstone::Inputs ready(Capstone::Timestamp now)
{
    Capstone::Inputs input = Capstone::Inputs();
    input.arm = true;
    input.drive_request = true;
    input.permission = true;
    input.mode = Capstone::combined;
    input.request_percent = 100.0F;
    input.discharge_limit_a = 200.0F;
    input.charge_limit_a = 20.0F;
    for (unsigned i = 0U; i < 2U; ++i)
    {
        input.motor[i].valid = true;
        input.motor[i].ready = true;
        input.motor[i].rotor_valid = true;
        input.motor[i].received_ms = now;
        input.motor[i].motor_temp_c = 25.0F;
        input.motor[i].inverter_temp_c = 25.0F;
    }
    return input;
}

static dual_config_t legacyconfig(const Capstone::Config& cfg)
{
    dual_config_t old = dual_simulation_config();
    old.stale_ms = cfg.stale_ms;
    old.max_rpm = cfg.max_rpm;
    old.derate_c = cfg.derate_c;
    old.stop_c = cfg.stop_c;
    old.pack_current_tolerance_a = cfg.pack_current_tolerance_a;
    old.circulation_threshold_a = cfg.circulation_threshold_a;
    for (unsigned i = 0U; i < 2U; ++i)
    {
        old.draw_a[i] = cfg.draw_a[i];
        old.regen_a[i] = cfg.regen_a[i];
    }
    return old;
}

static dual_inputs_t legacyinput(const Capstone::Inputs& input)
{
    dual_inputs_t old = dual_inputs_t();
    old.arm = input.arm;
    old.drive_request = input.drive_request;
    old.permission = input.permission;
    old.bus_off = input.bus_off;
    old.reset_request = input.reset_request;
    old.mode = static_cast<dual_mode_t>(input.mode);
    old.request_percent = input.request_percent;
    old.discharge_limit_a = input.discharge_limit_a;
    old.charge_limit_a = input.charge_limit_a;
    old.pack_current_a = input.pack_current_a;
    for (unsigned i = 0U; i < 2U; ++i)
    {
        old.motor[i].valid = input.motor[i].valid;
        old.motor[i].ready = input.motor[i].ready;
        old.motor[i].rotor_valid = input.motor[i].rotor_valid;
        old.motor[i].fault = input.motor[i].fault;
        old.motor[i].received_ms = input.motor[i].received_ms;
        old.motor[i].rpm = input.motor[i].rpm;
        old.motor[i].motor_temp_c = input.motor[i].motor_temp_c;
        old.motor[i].inverter_temp_c = input.motor[i].inverter_temp_c;
        old.motor[i].dc_current_a = input.motor[i].dc_current_a;
    }
    return old;
}

static Capstone::Outputs compare(const Capstone::Config& cfg,
                                 const Capstone::Inputs& input,
                                 Capstone::Timestamp now,
                                 Capstone::State& state,
                                 dual_outputs_t& legacy)
{
    const dual_config_t oldcfg = legacyconfig(cfg);
    const dual_inputs_t oldin = legacyinput(input);
    dual_drive_evaluate(&oldcfg, &oldin, now, &legacy);
    const Capstone::Outputs out = Capstone::evaluate(cfg, input, now, state);
    CHECK(out.enabled == legacy.enabled);
    CHECK(out.faults == legacy.faults);
    CHECK(state.latched == legacy.latched);
    CHECK(state.latched_faults == legacy.latched_faults);
    CHECK(state.last_step_ms == legacy.last_step_ms);
    CHECK(static_cast<unsigned>(state.previous_mode) ==
          static_cast<unsigned>(legacy.previous_mode));
    for (unsigned i = 0U; i < 2U; ++i)
    {
        NEAR(out.command_percent[i], legacy.command_percent[i]);
        NEAR(out.allocated_pack_a[i], legacy.allocated_pack_a[i]);
    }
    ++comparisons;
    return out;
}

static void allocationcomparison()
{
    const Capstone::Config cfg = Capstone::simulationconfig();
    for (unsigned mode = 0U; mode < 3U; ++mode)
    {
        for (int request = -100; request <= 100; request += 5)
        {
            for (unsigned budget = 0U; budget <= 250U; budget += 5U)
            {
                Capstone::Inputs input = ready(100U);
                input.mode = static_cast<Capstone::Mode>(mode);
                input.request_percent = static_cast<float>(request);
                input.discharge_limit_a = static_cast<float>(budget);
                input.charge_limit_a = static_cast<float>(budget);
                Capstone::State state = Capstone::State();
                dual_outputs_t old = dual_outputs_t();
                compare(cfg, input, 100U, state, old);
            }
        }
    }
}

static void faultcomparison()
{
    const Capstone::Config cfg = Capstone::simulationconfig();
    for (unsigned motor = 0U; motor < 2U; ++motor)
    {
        for (unsigned fault = 0U; fault < 18U; ++fault)
        {
            Capstone::Inputs input = ready(100U);
            Capstone::State state = Capstone::State();
            dual_outputs_t old = dual_outputs_t();
            compare(cfg, input, 100U, state, old);
            input = ready(110U);
            switch (fault)
            {
            case 0U:
                input.motor[motor].valid = false;
                break;
            case 1U:
                input.motor[motor].ready = false;
                break;
            case 2U:
                input.motor[motor].rotor_valid = false;
                break;
            case 3U:
                input.motor[motor].fault = true;
                break;
            case 4U:
                input.motor[motor].received_ms = 10U;
                break;
            case 5U:
                input.motor[motor].received_ms = 111U;
                break;
            case 6U:
                input.motor[motor].rpm = -5000.0F;
                break;
            case 7U:
                input.motor[motor].motor_temp_c = 90.0F;
                break;
            case 8U:
                input.motor[motor].inverter_temp_c = 90.0F;
                break;
            case 9U:
                input.motor[motor].rpm = std::numeric_limits<float>::quiet_NaN();
                break;
            case 10U:
                input.motor[motor].inverter_temp_c = std::numeric_limits<float>::infinity();
                break;
            case 11U:
                input.bus_off = true;
                break;
            case 12U:
                input.permission = false;
                break;
            case 13U:
                input.pack_current_a = 300.0F;
                break;
            case 14U:
                input.pack_current_a = -30.0F;
                break;
            case 15U:
                input.mode = Capstone::mg1_only;
                break;
            case 16U:
                input.motor[0].dc_current_a = 10.0F;
                input.motor[1].dc_current_a = -10.0F;
                break;
            default:
                input.request_percent = std::numeric_limits<float>::quiet_NaN();
                break;
            }
            CHECK(!compare(cfg, input, 110U, state, old).enabled);
            input = ready(120U);
            CHECK(!compare(cfg, input, 120U, state, old).enabled);
            input.reset_request = true;
            CHECK(!compare(cfg, input, 120U, state, old).enabled);
            input.arm = false;
            input.drive_request = false;
            input.request_percent = 0.0F;
            compare(cfg, input, 120U, state, old);
            input = ready(130U);
            CHECK(compare(cfg, input, 130U, state, old).enabled);
        }
    }
    // Thermal allocation; exact stale boundary; scheduler regression and overrun.
    Capstone::Inputs input = ready(100U);
    Capstone::State state = Capstone::State();
    dual_outputs_t old = dual_outputs_t();
    input.motor[0].motor_temp_c = 80.0F;
    compare(cfg, input, 100U, state, old);
    input.motor[0].received_ms = 1U;
    input.motor[1].received_ms = 100U;
    compare(cfg, input, 100U, state, old);
    compare(cfg, input, 101U, state, old);
    input = ready(202U);
    compare(cfg, input, 202U, state, old);
    input = ready(190U);
    compare(cfg, input, 190U, state, old);
}

class Mockport : public Capstone::Hardwareport
{
  public:
    unsigned sends;
    unsigned kicks;
    unsigned fail_send;
    bool mapping_ok;
    bool inhibit_ok;
    bool release_ok;
    bool asserted;
    Capstone::Frame frames[16];
    Mockport()
        : sends(0U), kicks(0U), fail_send(0U), mapping_ok(true), inhibit_ok(true), release_ok(true),
          asserted(true)
    {
    }
    virtual bool send(const Capstone::Frame& frame)
    {
        if (sends < 16U)
        {
            frames[sends] = frame;
        }
        ++sends;
        return sends != fail_send;
    }
    virtual bool setinhibit(bool value)
    {
        asserted = value;
        return value ? inhibit_ok : release_ok;
    }
    virtual void kickwatchdog()
    {
        ++kicks;
    }
    virtual bool mapcommand(unsigned, float effort, Capstone::Rawcommand& raw)
    {
        // SYNTHETIC mapper, for testing only; not a production calibration.
        raw.throttle_a = static_cast<uint16_t>(1000.0F + effort);
        raw.throttle_b = raw.throttle_a;
        raw.io_bits = 8U;
        raw.regen_percent = 0U;
        return mapping_ok;
    }
};

static Capstone::Runtimeconfig runtimeconfig()
{
    Capstone::Runtimeconfig cfg = Capstone::Runtimeconfig();
    cfg.policy = Capstone::simulationconfig();
    cfg.policy.stale_ms = 80U;
    cfg.period_ms = 10U;
    cfg.deadline_ms = 10U;
    cfg.bms_stale_ms = 80U;
    cfg.inhibit_budget_ms = 10U;
    cfg.control_id[0] = 0x03FU;
    cfg.control_id[1] = 0x040U;
    cfg.rise_percent_per_second = 1000.0F;
    return cfg;
}

static Capstone::Snapshot snapshot(Capstone::Timestamp now, bool armed)
{
    Capstone::Snapshot input = Capstone::Snapshot();
    input.policy = ready(now);
    input.policy.arm = armed;
    input.policy.drive_request = armed;
    input.policy.request_percent = armed ? 100.0F : 0.0F;
    input.bms_valid = true;
    input.bms_received_ms = now;
    input.transport_healthy = true;
    input.watchdog_healthy = true;
    input.energy_ready = true;
    return input;
}

static void runtimetests()
{
    const Capstone::Runtimeconfig cfg = runtimeconfig();
    Capstone::Runtimestate state = Capstone::Runtimestate();
    Mockport port;
    CHECK(Capstone::initialize(0U, state, port));
    Capstone::Cycle cycle = Capstone::run(cfg, snapshot(10U, true), 10U, state, port);
    CHECK(!cycle.dispatched && port.asserted); // boot while armed cannot drive
    Capstone::run(cfg, snapshot(20U, false), 20U, state, port);
    cycle = Capstone::run(cfg, snapshot(30U, true), 30U, state, port);
    CHECK(cycle.dispatched && !port.asserted && cycle.watchdog_kicked);
    NEAR(cycle.policy.command_percent[0], 10.0F);
    CHECK(port.frames[0].identifier == 0x03FU && port.frames[1].identifier == 0x040U);
    CHECK(state.counter[0] == 1U && state.counter[1] == 1U);
    unsigned sends = port.sends;
    unsigned kicks = port.kicks;
    cycle = Capstone::run(cfg, snapshot(35U, true), 35U, state, port);
    CHECK(!cycle.due && !cycle.inhibit_requested && port.sends == sends && port.kicks == kicks);
    Capstone::Snapshot input = snapshot(40U, true);
    input.policy.request_percent = -100.0F;
    cycle = Capstone::run(cfg, input, 40U, state, port);
    NEAR(cycle.policy.command_percent[0], 0.0F); // reversal passes through zero
    input = snapshot(50U, true);
    input.policy.request_percent = -100.0F;
    cycle = Capstone::run(cfg, input, 50U, state, port);
    NEAR(cycle.policy.command_percent[0], -10.0F);
    input = snapshot(60U, true);
    input.policy.charge_limit_a = 0.0F;
    input.policy.request_percent = -100.0F;
    cycle = Capstone::run(cfg, input, 60U, state, port);
    CHECK(!cycle.dispatched && port.asserted);
    CHECK(Capstone::run(cfg, snapshot(65U, false), 65U, state, port).inhibit_requested);

    // Each runtime fault inhibits, stays latched, then accepts only disarmed reset.
    for (unsigned fault = 0U; fault < 10U; ++fault)
    {
        Mockport mock;
        Capstone::Runtimestate memory = Capstone::Runtimestate();
        Capstone::initialize(0U, memory, mock);
        Capstone::run(cfg, snapshot(10U, false), 10U, memory, mock);
        input = snapshot(20U, true);
        uint32_t clock = 20U;
        switch (fault)
        {
        case 0U:
            input.bms_valid = false;
            break;
        case 1U:
            input.transport_healthy = false;
            break;
        case 2U:
            input.watchdog_healthy = false;
            break;
        case 3U:
            input.energy_ready = false;
            break;
        case 4U:
            mock.mapping_ok = false;
            break;
        case 5U:
            mock.fail_send = 1U;
            break;
        case 6U:
            mock.fail_send = 2U;
            break;
        case 7U:
            mock.release_ok = false;
            break;
        case 8U:
            clock = 31U;
            input = snapshot(31U, true);
            break;
        default:
            input.policy.motor[0].valid = false;
            break;
        }
        cycle = Capstone::run(cfg, input, clock, memory, mock);
        CHECK(cycle.faults != 0U && !cycle.dispatched && mock.asserted && !cycle.watchdog_kicked);
        NEAR(cycle.policy.command_percent[0], 0.0F);
        if (fault == 6U)
        {
            CHECK(memory.counter[0] == 1U && memory.counter[1] == 0U);
        }
        mock.mapping_ok = true;
        mock.fail_send = 0U;
        mock.release_ok = true;
        clock += 10U;
        cycle = Capstone::run(cfg, snapshot(clock, true), clock, memory, mock);
        CHECK(!cycle.dispatched && cycle.faults != 0U);
        clock += 10U;
        input = snapshot(clock, false);
        input.policy.reset_request = true;
        cycle = Capstone::run(cfg, input, clock, memory, mock);
        CHECK(cycle.faults == 0U && !cycle.dispatched);
        clock += 10U;
        CHECK(Capstone::run(cfg, snapshot(clock, true), clock, memory, mock).dispatched);
    }
    // uint32 hardware clock wraps; internal acquisition timestamps remain monotonic.
    Mockport wrap;
    Capstone::Runtimestate memory = Capstone::Runtimestate();
    Capstone::initialize(0xFFFFFFF8U, memory, wrap);
    CHECK(!Capstone::run(cfg, snapshot(10U, false), 2U, memory, wrap).faults);
    CHECK(Capstone::run(cfg, snapshot(20U, true), 12U, memory, wrap).dispatched);
    CHECK(memory.monotonic_ms == 20U);
    input = snapshot(30U, true);
    input.bms_received_ms = 31U;
    CHECK(Capstone::run(cfg, input, 22U, memory, wrap).faults & Capstone::bms_fault);

    // Fresh motor frames do not refresh a stale BMS permission snapshot.
    Mockport stale;
    memory = Capstone::Runtimestate();
    Capstone::initialize(0U, memory, stale);
    for (uint32_t clock = 10U; clock <= 80U; clock += 10U)
    {
        input = snapshot(clock, clock > 10U);
        input.bms_received_ms = 0U;
        cycle = Capstone::run(cfg, input, clock, memory, stale);
        CHECK(((cycle.faults & Capstone::bms_fault) != 0U) == (clock == 80U));
    }
    CHECK(stale.asserted && !cycle.dispatched);
    CHECK(cycle.inhibit_requested);

    // Incomplete feedback, future acquisition times and no automatic restart.
    for (unsigned channel = 0U; channel < 2U; ++channel)
    {
        Mockport partial;
        memory = Capstone::Runtimestate();
        Capstone::initialize(0U, memory, partial);
        Capstone::run(cfg, snapshot(10U, false), 10U, memory, partial);
        input = snapshot(20U, true);
        input.policy.motor[channel].received_ms = 21U;
        cycle = Capstone::run(cfg, input, 20U, memory, partial);
        CHECK((cycle.faults & Capstone::stale_fault) != 0U && partial.asserted);
    }

    // Invalid timing budgets and duplicate node IDs cannot dispatch.
    for (unsigned invalid = 0U; invalid < 4U; ++invalid)
    {
        Mockport bad;
        memory = Capstone::Runtimestate();
        Capstone::initialize(0U, memory, bad);
        Capstone::Runtimeconfig changed = cfg;
        if (invalid == 0U)
        {
            changed.control_id[1] = changed.control_id[0];
        }
        if (invalid == 1U)
        {
            changed.period_ms = 0U;
        }
        if (invalid == 2U)
        {
            changed.deadline_ms = 20U;
        }
        if (invalid == 3U)
        {
            changed.rise_percent_per_second = std::numeric_limits<float>::quiet_NaN();
        }
        cycle = Capstone::run(changed, snapshot(10U, false), 10U, memory, bad);
        CHECK((cycle.faults & Capstone::runtime_config_fault) != 0U && bad.sends == 0U &&
              bad.asserted);
    }
    Mockport failed_inhibit;
    failed_inhibit.inhibit_ok = false;
    memory = Capstone::Runtimestate();
    CHECK(!Capstone::initialize(0U, memory, failed_inhibit));
    cycle = Capstone::run(cfg, snapshot(10U, false), 10U, memory, failed_inhibit);
    CHECK((cycle.faults & Capstone::inhibit_fault) != 0U && !cycle.watchdog_kicked);

    Mockport uninitialized;
    memory = Capstone::Runtimestate();
    cycle = Capstone::run(cfg, snapshot(10U, true), 10U, memory, uninitialized);
    CHECK((cycle.faults & Capstone::runtime_config_fault) != 0U && uninitialized.sends == 0U);
}

static void codectests()
{
    for (unsigned counter = 0U; counter < 4U; ++counter)
    {
        for (unsigned throttle = 0U; throttle <= 4095U; throttle += 63U)
        {
            oi_control_t c = {
                static_cast<uint16_t>(throttle), 2345U, 8U, static_cast<uint8_t>(counter), 25U};
            const Capstone::Rawcommand raw = {
                c.throttle_a, c.throttle_b, c.io_bits, c.regen_percent};
            Capstone::Frame frame = Capstone::Frame();
            uint8_t bytes[8] = {0U};
            CHECK(oi_encode_control(&c, bytes));
            CHECK(Capstone::encode(raw, c.counter, frame));
            CHECK(memcmp(bytes, frame.data, 8U) == 0);
        }
    }
    Capstone::Rawcommand raw = Capstone::Rawcommand();
    Capstone::Frame frame = Capstone::Frame();
    raw.throttle_a = 4096U;
    CHECK(!Capstone::encode(raw, 0U, frame));
    raw.throttle_a = 0U;
    raw.io_bits = 24U;
    CHECK(!Capstone::encode(raw, 0U, frame));
    raw.io_bits = 0U;
    CHECK(!Capstone::encode(raw, 4U, frame));
}

int main()
{
    allocationcomparison();
    faultcomparison();
    runtimetests();
    codectests();
    printf("%u C/C++ policy comparisons; codec and mocked runtime checks; %d failures\n",
           comparisons,
           failures);
    return failures == 0 ? 0 : 1;
}
