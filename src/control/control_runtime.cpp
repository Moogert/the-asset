#include <control/control_runtime.h>

namespace Capstone
{
namespace
{
// Check that stale-data detection, scheduler delay and inhibit allowance fit the 100 ms study budget.
bool timingvalid(const Runtimeconfig& cfg)
{
    return (cfg.period_ms >= 1U) && (cfg.period_ms <= 10U) && (cfg.deadline_ms >= cfg.period_ms) &&
           (cfg.deadline_ms <= 20U) && (cfg.bms_stale_ms > 0U) && (cfg.bms_stale_ms <= 80U) &&
           (cfg.policy.stale_ms > 0U) && (cfg.policy.stale_ms <= 80U) &&
           (cfg.inhibit_budget_ms > 0U) && (cfg.inhibit_budget_ms <= 20U) &&
           (cfg.bms_stale_ms + cfg.deadline_ms + cfg.inhibit_budget_ms <= 100U) &&
           (cfg.policy.stale_ms + cfg.deadline_ms + cfg.inhibit_budget_ms <= 100U);
}

// Require distinct valid CAN identities and a positive finite-range effort-rise limit.
bool runtimeconfigvalid(const Runtimeconfig& cfg)
{
    return timingvalid(cfg) && (cfg.control_id[0] > 0U) && (cfg.control_id[0] <= 0X7FFU) &&
           (cfg.control_id[1] > 0U) && (cfg.control_id[1] <= 0X7FFU) &&
           (cfg.control_id[0] != cfg.control_id[1]) && (cfg.rise_percent_per_second > 0.0F) &&
           (cfg.rise_percent_per_second <= 10000.0F);
}

// Accept reset intent only while disarmed with no drive request and exactly zero effort.
bool resetallowed(const Snapshot& snapshot)
{
    return snapshot.policy.reset_request && !snapshot.policy.arm &&
           !snapshot.policy.drive_request && (snapshot.policy.request_percent == 0.0F);
}

Real limited(Real requested, Real previous, Real increment)
{
    Real result = requested;
    // Reductions are immediate. Sign reversal passes through zero for one tick.
    if (((requested > 0.0F) && (previous < 0.0F)) || ((requested < 0.0F) && (previous > 0.0F)))
    {
        result = 0.0F;
    }
    else if ((requested > 0.0F) && (requested > previous + increment))
    {
        result = previous + increment;
    }
    else if ((requested < 0.0F) && (requested < previous - increment))
    {
        result = previous - increment;
    }
    return result;
}

// Combine acquisition/permission/watchdog checks before transport can receive a command.
uint32_t snapshotfaults(const Runtimeconfig& cfg, const Snapshot& snapshot, Timestamp now_ms)
{
    uint32_t faults = 0U;
    if (!snapshot.bms_valid || (snapshot.bms_received_ms > now_ms) ||
        ((now_ms - snapshot.bms_received_ms) >= cfg.bms_stale_ms))
    {
        faults |= bms_fault;
    }
    if (!snapshot.transport_healthy)
    {
        faults |= transport_fault;
    }
    if (!snapshot.watchdog_healthy)
    {
        faults |= inhibit_fault;
    }
    return faults;
}

// Map and encode both channels before any transmission; mapping failure suppresses dispatch.
bool prepareframes(const Runtimeconfig& cfg,
                   const Outputs& output,
                   const Runtimestate& state,
                   Hardwareport& port,
                   Frame frames[2])
{
    bool valid = true;
    for (unsigned channel = 0U; channel < 2U; ++channel)
    {
        Rawcommand raw = Rawcommand();
        frames[channel].identifier = cfg.control_id[channel];
        const bool mapped = port.mapcommand(channel, output.command_percent[channel], raw);
        const bool encoded = encode(raw, state.counter[channel], frames[channel]);
        valid = valid && mapped && encoded;
    }
    return valid;
}

// Advance each channel counter only when its local send call succeeds.
bool dispatch(const Frame frames[2], Runtimestate& state, Hardwareport& port)
{
    bool valid = true;
    for (unsigned channel = 0U; channel < 2U; ++channel)
    {
        if (valid)
        {
            valid = port.send(frames[channel]);
            if (valid)
            {
                state.counter[channel] = static_cast<uint8_t>((state.counter[channel] + 1U) & 3U);
            }
        }
    }
    return valid;
}

// Remove requested and remembered effort together so a future cycle cannot reuse stale torque intent.
void cleareffort(Cycle& result, Runtimestate& state)
{
    result.policy.enabled = false;
    for (unsigned channel = 0U; channel < 2U; ++channel)
    {
        result.policy.command_percent[channel] = 0.0F;
        result.policy.allocated_pack_a[channel] = 0.0F;
        state.previous_command[channel] = 0.0F;
    }
}

// Limit effort increases using elapsed milliseconds; reductions and reversal-to-zero remain immediate.
void limitcommands(const Runtimeconfig& cfg,
                   uint32_t elapsed_ms,
                   Runtimestate& state,
                   Outputs& output)
{
    const Real increment = cfg.rise_percent_per_second * static_cast<Real>(elapsed_ms) / 1000.0F;
    for (unsigned channel = 0U; channel < 2U; ++channel)
    {
        const Real requested = output.command_percent[channel];
        output.command_percent[channel] =
            limited(requested, state.previous_command[channel], increment);
        if (requested != 0.0F)
        {
            output.allocated_pack_a[channel] *= output.command_percent[channel] / requested;
        }
    }
}

// Retain runtime faults until a healthy deliberate reset and remember the disarmed startup gate.
void updatelatches(uint32_t faults, const Snapshot& snapshot, Runtimestate& state)
{
    if (faults != 0U)
    {
        state.latched_faults |= faults;
    }
    else if (resetallowed(snapshot))
    {
        state.latched_faults = 0U;
    }
    if (!snapshot.policy.arm && !snapshot.policy.drive_request &&
        (snapshot.policy.request_percent == 0.0F) && (faults == 0U))
    {
        state.disarmed_seen = true;
    }
}

// Release inhibit only after both frames send and the hardware port acknowledges the request.
uint32_t attemptdispatch(const Runtimeconfig& cfg,
                         uint32_t elapsed_ms,
                         Runtimestate& state,
                         Hardwareport& port,
                         Cycle& result)
{
    uint32_t faults = 0U;
    limitcommands(cfg, elapsed_ms, state, result.policy);
    Frame frames[2] = {Frame(), Frame()};
    if (!prepareframes(cfg, result.policy, state, port, frames))
    {
        faults |= mapping_fault;
    }
    else if (!dispatch(frames, state, port))
    {
        faults |= transport_fault;
    }
    else if (!port.setinhibit(false))
    {
        faults |= inhibit_fault;
    }
    else
    {
        result.dispatched = true;
        result.inhibit_requested = false;
        state.inhibit_requested = false;
        for (unsigned channel = 0U; channel < 2U; ++channel)
        {
            state.previous_command[channel] = result.policy.command_percent[channel];
        }
    }
    return faults;
}

// Undispatched cycles request inhibit; watchdog service is conditional on healthy unlatched state.
void finishcycle(Cycle& result, uint32_t faults, Runtimestate& state, Hardwareport& port)
{
    if (!result.dispatched)
    {
        state.inhibit_requested = true;
        cleareffort(result, state);
        if (!port.setinhibit(true))
        {
            faults |= inhibit_fault;
        }
    }
    state.latched_faults |= faults;
    result.faults = state.latched_faults;
    if ((state.latched_faults == 0U) && !state.policy.latched)
    {
        port.kickwatchdog();
        result.watchdog_kicked = true;
    }
}
} // namespace

// Start with inhibit asserted; an acknowledgment is a driver result, not measured physical timing.
bool initialize(uint32_t clock_ms, Runtimestate& state, Hardwareport& port)
{
    state = Runtimestate();
    state.last_clock_ms = clock_ms;
    state.inhibit_requested = true;
    const bool acknowledged = port.setinhibit(true);
    state.initialized = true;
    if (!acknowledged)
    {
        state.latched_faults = inhibit_fault;
    }
    return acknowledged;
}

// Extend the wrapping board clock and execute only scheduled cycles; outputs remain owned by the caller.
Cycle run(const Runtimeconfig& cfg,
          const Snapshot& snapshot,
          uint32_t clock_ms,
          Runtimestate& state,
          Hardwareport& port)
{
    Cycle result = Cycle();
    result.inhibit_requested = true;
    const uint32_t elapsed_ms = clock_ms - state.last_clock_ms;
    uint32_t faults = 0U;
    if (!state.initialized || !runtimeconfigvalid(cfg))
    {
        faults |= runtime_config_fault;
    }
    if (elapsed_ms > cfg.deadline_ms)
    {
        faults |= runtime_timing_fault;
    }
    if ((faults != 0U) || (elapsed_ms >= cfg.period_ms))
    {
        result.due = true;
        state.last_clock_ms = clock_ms;
        state.monotonic_ms += elapsed_ms;
        faults |= snapshotfaults(cfg, snapshot, state.monotonic_ms);
        Inputs input = snapshot.policy;
        input.permission = input.permission && snapshot.energy_ready;
        result.policy = evaluate(cfg.policy, input, state.monotonic_ms, state.policy);
        faults |= result.policy.faults;
        updatelatches(faults, snapshot, state);
        const bool allowed =
            result.policy.enabled && state.disarmed_seen && (state.latched_faults == 0U);
        if (allowed)
        {
            faults |= attemptdispatch(cfg, elapsed_ms, state, port, result);
        }
        finishcycle(result, faults, state, port);
    }
    else
    {
        // No I/O or watchdog feed on calls before the next scheduled tick.
        result.inhibit_requested = state.inhibit_requested;
        result.faults = state.latched_faults;
    }
    return result;
}
} // namespace Capstone
