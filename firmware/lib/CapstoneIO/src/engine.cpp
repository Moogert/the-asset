#include "engine.h"
#include <string.h>
#include <stdlib.h>
namespace Civic
{
// Build a deliberately artificial 720-degree pattern; this is not a Honda calibration.
Engineprofile synthetic_profile()
{
    Engineprofile p = {};
    p.channel_mask = 7;
    p.synthetic = true;
    p.max_rpm = 8000;
    // Artificial 720-degree cycle: CKP 24 pulses, TDC 4, CYP 1.
    // Events combined at equal angles. NOT a verified Honda distributor pattern.
    uint8_t levels = 0;
    for (uint16_t a = 0; a < 7200; a += 150)
    {
        levels ^= 1;
        if (a % 900 == 0)
            levels ^= 2;
        if (a == 0 || a == 3600)
            levels ^= 4;
        p.edges[p.count++] = Edge{a, levels};
    }
    return p;
}
// Reject invalid masks/order and edge spacing below the software timing study limit.
bool valid_profile(const Engineprofile& p)
{
    if (!p.count || p.count > 128 || !p.channel_mask || p.channel_mask > 15 || p.max_rpm < 200 ||
        p.max_rpm > 10000 || (!p.synthetic && !p.verified) || (p.synthetic && p.verified) ||
        p.edges[0].angle != 0)
        return false;
    for (uint8_t i = 0; i < p.count; ++i)
    {
        if (p.edges[i].angle >= 7200 || (p.edges[i].levels & ~p.channel_mask) ||
            (i && p.edges[i].angle <= p.edges[i - 1].angle))
            return false;
        uint16_t gap = i + 1 < p.count
                           ? static_cast<uint16_t>(p.edges[i + 1].angle - p.edges[i].angle)
                           : static_cast<uint16_t>(7200 - p.edges[i].angle);
        if ((static_cast<uint64_t>(gap) * 120000000ULL) /
                (static_cast<uint32_t>(p.max_rpm) * 7200UL) <
            16)
            return false;
    }
    return true;
}
// Start disarmed with deterministic state; construction never authorizes physical outputs.
Engine::Engine(const Engineprofile& p)
    : profile_(p), mode_(stopped), rpm_(800), throttle_(0), load_(20), drop_(0), edge_(0),
      enabled_(false), scheduled_(false), timing_fault_(false), heartbeat_(0), mode_start_(0),
      deadline_(0), cycle_start_(0), cycle_us_(0)
{
    for (unsigned i = 0; i < 8; ++i)
    {
        faults_[i] = normal;
        indication_[i] = normal;
    }
    injection_us_ = 0;
    oxygen_heater_ = false;
    actuator_seen_ = false;
    actuator_at_ = 0;
}
// Clear enable and phase scheduling together; the board adapter opens the hardware gate.
void Engine::stop()
{
    enabled_ = false;
    scheduled_ = false;
    mode_ = stopped;
}
void Engine::apply_indication(const Inject conditions[8])
{
    for (unsigned i = 0; i < 8; ++i)
        indication_[i] = conditions[i];
}
// Accept unsigned decimal text only; trailing characters and out-of-range values are errors.
static bool number(const char* s, unsigned maximum, unsigned& v)
{
    if (!s || *s < '0' || *s > '9')
        return false;
    char* end = 0;
    unsigned long n = strtoul(s, &end, 10);
    if (*end || n > maximum)
        return false;
    v = static_cast<unsigned>(n);
    return true;
}
// Prepare only immutable-profile-dependent data; no ISR-owned scheduler state is changed here.
bool Engine::prepare_command(const char* text, bool bench, Engineinstruction& out) const
{
    out = Engineinstruction{};
    if (!text || strlen(text) >= 96)
        return false;
    if (!strcmp(text, "DISARM"))
        out.operation = disarm_operation;
    else if (!strcmp(text, "PING"))
        out.operation = ping_operation;
    else if (!strcmp(text, "ARM BENCH") || !strcmp(text, "ARM VEHICLE"))
    {
        const bool requested_bench = !strcmp(text, "ARM BENCH");
        if (!valid_profile(profile_) ||
            (requested_bench ? !(bench && profile_.synthetic) : !profile_.verified))
            return false;
        out.operation = arm_operation;
        out.arm_permitted = true;
    }
    else if (!strncmp(text, "STATE ", 6))
    {
        out.operation = state_operation;
        if (!strcmp(text + 6, "STOP"))
            out.value = stopped;
        else if (!strcmp(text + 6, "CRANK"))
            out.value = cranking;
        else if (!strcmp(text + 6, "RUN"))
            out.value = running;
        else if (!strcmp(text + 6, "WARM"))
            out.value = warming;
        else
            return false;
    }
    else if (!strncmp(text, "FAULT ", 6))
    {
        const char* value = text + 6;
        if (value[0] < '0' || value[0] > '7' || value[1] != ' ')
            return false;
        out.channel = static_cast<uint8_t>(value[0] - '0');
        if (!strcmp(value + 2, "NONE"))
            out.condition = normal;
        else if (!strcmp(value + 2, "OPEN"))
            out.condition = open_circuit;
        else if (!strcmp(value + 2, "LOW"))
            out.condition = stuck_low;
        else if (!strcmp(value + 2, "HIGH"))
            out.condition = stuck_high;
        else
            return false;
        out.operation = fault_operation;
    }
    else
    {
        unsigned value = 0;
        if (!strncmp(text, "RPM ", 4))
        {
            if (!number(text + 4, profile_.max_rpm, value) || value < 200)
                return false;
            out.operation = rpm_operation;
        }
        else if (!strncmp(text, "THROTTLE ", 9) || !strncmp(text, "LOAD ", 5))
        {
            const bool throttle = text[0] == 'T';
            if (!number(text + (throttle ? 9 : 5), 100, value))
                return false;
            out.operation = throttle ? throttle_operation : load_operation;
        }
        else if (!strncmp(text, "DROP ", 5))
        {
            if (!number(text + 5, profile_.channel_mask, value))
                return false;
            out.operation = drop_operation;
        }
        else
            return false;
        out.value = static_cast<uint16_t>(value);
    }
    return true;
}
// Bounded state writes only: the board excludes ISR access around this transaction.
bool Engine::apply_command(const Engineinstruction& in, uint32_t now)
{
    switch (in.operation)
    {
    case disarm_operation:
        stop();
        break;
    case ping_operation:
        heartbeat_ = now;
        break;
    case arm_operation:
        if (!in.arm_permitted)
            return false;
        enabled_ = true;
        scheduled_ = false;
        timing_fault_ = false;
        heartbeat_ = now;
        break;
    case state_operation:
        if (in.value > warming)
            return false;
        mode_ = static_cast<Enginemode>(in.value);
        mode_start_ = now;
        scheduled_ = false;
        if (mode_ == cranking)
            rpm_ = 250;
        break;
    case rpm_operation:
        if (in.value < 200 || in.value > profile_.max_rpm)
            return false;
        rpm_ = in.value;
        scheduled_ = false;
        break;
    case throttle_operation:
    case load_operation:
        if (in.value > 100)
            return false;
        if (in.operation == throttle_operation)
            throttle_ = static_cast<uint8_t>(in.value);
        else
            load_ = static_cast<uint8_t>(in.value);
        break;
    case drop_operation:
        if (in.value & ~profile_.channel_mask)
            return false;
        drop_ = static_cast<uint8_t>(in.value);
        break;
    case fault_operation:
        if (in.channel > 7 || in.condition < normal || in.condition > stuck_high)
            return false;
        faults_[in.channel] = in.condition;
        break;
    default:
        return false;
    }
    return true;
}
// Host-compatible convenience entry point; MCU code separates parsing from atomic application.
bool Engine::command(const char* text, uint32_t now, bool bench)
{
    Engineinstruction instruction = {};
    return prepare_command(text, bench, instruction) && apply_command(instruction, now);
}
// Supervise heartbeat/contact in milliseconds; verified running profiles also require actuator evidence.
void Engine::update(uint32_t now, bool contact)
{
    if (!contact || static_cast<uint32_t>(now - heartbeat_) > 500)
        stop();
    if (profile_.verified && mode_ != stopped &&
        (!actuator_seen_ || static_cast<uint32_t>(now - actuator_at_) > 2000))
        stop();
}
// Record protected feedback in microseconds, heater state and boot-relative milliseconds.
void Engine::observe_actuators(uint16_t pulse, bool heater, uint32_t now)
{
    if (pulse > 20000)
        return;
    injection_us_ = pulse;
    oxygen_heater_ = heater;
    actuator_seen_ = true;
    actuator_at_ = now;
}
// Emit at most one due edge; deadlines use modular arithmetic, with intervals below half a clock wrap.
bool Engine::next_edge(uint32_t now, uint8_t& levels)
{
    if (!enabled_ || mode_ == stopped)
    {
        scheduled_ = false;
        levels = 0;
        return false;
    }
    if (!scheduled_)
    {
        // Two revolutions per engine cycle: 2 * 60 seconds * 1e6 microseconds / rpm.
        cycle_us_ = 120000000UL / rpm_;
        cycle_start_ = now;
        edge_ = 0;
        deadline_ = now;
        scheduled_ = true;
    }
    // Signed modular ordering is valid because scheduled delays remain below 2^31 us.
    if (static_cast<int32_t>(now - deadline_) < 0)
        return false;
    // Missed edges are not replayed as a burst. Fail off and require explicit re-arm.
    uint32_t next;
    if (edge_ + 1 < profile_.count)
        next = cycle_start_ +
               static_cast<uint32_t>(
                   (static_cast<uint64_t>(profile_.edges[edge_ + 1].angle) * cycle_us_) / 7200);
    else
        next = cycle_start_ + cycle_us_;
    if (static_cast<int32_t>(now - next) >= 0)
    {
        timing_fault_ = true;
        stop();
        levels = 0;
        return true;
    }
    // Fault injection removes selected channels without changing the remaining phase schedule.
    levels = static_cast<uint8_t>(profile_.edges[edge_].levels & ~drop_);
    ++edge_;
    if (edge_ == profile_.count)
    {
        edge_ = 0;
        cycle_start_ += cycle_us_;
    }
    deadline_ = next;
    return true;
}
// Return a relative delay; clamp expired deadlines to one microsecond rather than unsigned underflow.
uint32_t Engine::wait_us(uint32_t now) const
{
    if (!enabled_ || mode_ == stopped || !scheduled_)
        return 1000;
    int32_t remaining = static_cast<int32_t>(deadline_ - now);
    return remaining > 0 ? static_cast<uint32_t>(remaining) : 1;
}
// Copy only sensor-model inputs while interrupts are excluded; no parsing/division in this method.
Sensorstate Engine::capture_sensors() const
{
    Sensorstate state = {};
    state.enabled = enabled_;
    state.mode = mode_;
    state.throttle = throttle_;
    state.load = load_;
    state.mode_start_ms = mode_start_;
    state.injection_us = injection_us_;
    state.heater = oxygen_heater_;
    state.actuator_seen = actuator_seen_;
    state.actuator_ms = actuator_at_;
    for (unsigned i = 0; i < 8; ++i)
        state.fault[i] = indication_[i] != normal ? indication_[i] : faults_[i];
    return state;
}
Sensors Engine::sensors(uint32_t now) const
{
    return calculate_sensors(capture_sensors(), now);
}
// Generate teaching sensor commands; voltages/ladder indices are not validated ECU measurements.
Sensors calculate_sensors(const Sensorstate& state, uint32_t now)
{
    Sensors s = {};
    s.enabled = state.enabled;
    s.millivolts[0] = static_cast<uint16_t>(500 + state.throttle * 40);
    s.millivolts[1] = static_cast<uint16_t>(500 + state.load * 40);
    bool cold = state.mode == warming && static_cast<uint32_t>(now - state.mode_start_ms) < 30000;
    s.millivolts[2] = cold ? 450 : static_cast<uint16_t>((now / 500) % 2 ? 800 : 200);
    s.millivolts[3] = cold ? 450 : static_cast<uint16_t>((now / 2000) % 2 ? 650 : 550);
    s.actuator_feedback_valid =
        state.actuator_seen && static_cast<uint32_t>(now - state.actuator_ms) <= 2000;
    if (s.actuator_feedback_valid)
    {
        // Teaching model: injector duration relative to an assumed load demand changes mixture.
        // It is not a calibrated fuel/air/catalyst model and cannot prove monitor completion.
        uint16_t target = static_cast<uint16_t>(1000 + state.load * 40);
        s.millivolts[2] = (!state.heater || cold)
                              ? 450
                              : static_cast<uint16_t>(state.injection_us > target ? 800 : 200);
        s.millivolts[3] = (!state.heater || cold)
                              ? 450
                              : static_cast<uint16_t>(state.injection_us > target ? 650 : 550);
    }
    s.resistance_index[0] = state.mode == warming
                                ? static_cast<uint8_t>((now - state.mode_start_ms) / 15000 > 7
                                                           ? 7
                                                           : (now - state.mode_start_ms) / 15000)
                                : 7;
    s.resistance_index[1] = 2;
    for (unsigned i = 0; i < 8; ++i)
    {
        s.fault[i] = state.fault[i];
        if (s.fault[i] == stuck_low)
            s.millivolts[i] = 0;
        if (s.fault[i] == stuck_high)
            s.millivolts[i] = 5000;
    }
    return s;
}
// Discard an entire oversized/invalid line so a truncated suffix cannot become a new command.
bool Lines::feed(char c)
{
    if (c == '\r')
        return false;
    if (c == '\n')
    {
        bool ready = !discard_ && used_ != 0;
        text_[used_] = 0;
        used_ = 0;
        discard_ = false;
        return ready;
    }
    if (discard_)
        return false;
    if (c < 32 || c > 126 || used_ >= 95)
    {
        discard_ = true;
        used_ = 0;
        return false;
    }
    text_[used_++] = c;
    return false;
}
} // namespace Civic
