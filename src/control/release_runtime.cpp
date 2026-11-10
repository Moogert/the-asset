#include <control/release_control.h>
namespace Capstone
{
namespace
{
// The underlying runtime requests release; only measured local TX completion may enact it.
class Queueport : public Hardwareport
{
  public:
    explicit Queueport(Releaseport& port) : port_(port)
    {
    }
    bool send(const Frame& frame)
    {
        return port_.send(frame);
    }
    bool setinhibit(bool asserted)
    {
        return !asserted || port_.setinhibit(true);
    }
    void kickwatchdog()
    {
        port_.kickwatchdog();
    }
    bool mapcommand(unsigned channel, Real effort, Rawcommand& raw)
    {
        return port_.mapcommand(channel, effort, raw);
    }

  private:
    Releaseport& port_;
};
bool resetallowed(const Releaseinput& input)
{
    return input.snapshot.policy.reset_request && !input.snapshot.policy.arm &&
           !input.snapshot.policy.drive_request && input.snapshot.policy.request_percent == 0.0F;
}
bool inhibit(Releasestate& state, Releaseport& port)
{
    // Always try both operations; a failed inhibit request cannot skip queue cancellation.
    const bool gated = port.setinhibit(true);
    const bool discarded = port.discardpending();
    if (!gated || !discarded)
    {
        state.latched_faults |= inhibit_fault;
    }
    state.pending = false;
    state.released = false;
    return gated && discarded;
}
uint32_t
dispatchstatus(const Releaseconfig& cfg, Timestamp now, Releasestate& state, Releaseport& port)
{
    uint32_t faults = 0U;
    for (unsigned channel = 0U; channel < 2U; ++channel)
    {
        state.last_transmission[channel] = port.transmission(channel, state.generation);
        const Transmission& tx = state.last_transmission[channel];
        if (tx.failed ||
            (tx.complete && (tx.completed_ms > now || tx.completed_ms < state.queued_ms)))
        {
            faults |= dispatch_fault;
        }
    }
    const Transmission& a = state.last_transmission[0];
    const Transmission& b = state.last_transmission[1];
    if (a.complete && b.complete)
    {
        const Timestamp skew = a.completed_ms > b.completed_ms ? a.completed_ms - b.completed_ms
                                                               : b.completed_ms - a.completed_ms;
        if (skew > cfg.pair_skew_ms || a.completed_ms - state.queued_ms > cfg.command_age_ms ||
            b.completed_ms - state.queued_ms > cfg.command_age_ms)
        {
            faults |= dispatch_fault;
        }
        if (faults == 0U)
        {
            if (!port.setinhibit(false))
            {
                faults |= inhibit_fault;
            }
            else
            {
                state.pending = false;
                state.released = true;
            }
        }
    }
    else if (now < state.queued_ms || now - state.queued_ms >= cfg.command_age_ms)
    {
        faults |= dispatch_fault;
    }
    return faults;
}
void runtimerecords(Releasestate& state, uint32_t active, Timestamp now)
{
    for (unsigned bit = 0U; bit <= 22U; ++bit)
    {
        // Component monitors own these records; an aggregate must not overwrite their evidence.
        if (bit != 5U && bit != 7U && bit != 11U)
        {
            const uint32_t identity = 1U << bit;
            recordfault(state.monitor,
                        identity,
                        bit >= 19U ? transport : shared,
                        shutdown,
                        (active & identity) != 0U,
                        now,
                        faultvalue(0.0F, 0.0F, false));
        }
    }
}
bool releasevalid(const Releaseconfig& cfg, const Releaseport& port)
{
    const bool verified = cfg.production && cfg.hardware_verified;
    bool sensors = true;
    for (unsigned i = 0U; i < temperature_count; ++i)
    {
        if (cfg.temperature[i].required &&
            (cfg.temperature[i].location == Temperaturelimit::unspecified ||
             cfg.temperature[i].sensor_id == 0U))
        {
            sensors = false;
        }
    }
    return sensors && (!cfg.production || cfg.hardware_verified) &&
           (!port.physicaloutputs() || verified) && cfg.command_age_ms > 0U &&
           cfg.command_age_ms <= cfg.runtime.period_ms && cfg.pair_skew_ms <= cfg.command_age_ms;
}
bool driveallowed(const Releaseinput& input)
{
    return input.snapshot.policy.arm && input.snapshot.policy.drive_request &&
           input.snapshot.policy.permission && input.snapshot.energy_ready;
}
// Polling TX completion must never release a channel after a newly invalid snapshot.
uint32_t acquisitionfaults(const Releaseconfig& cfg,
                           const Releaseinput& input,
                           const Monitorresult& monitoring,
                           Timestamp now,
                           const Releasestate& state)
{
    uint32_t faults = 0U;
    const Snapshot& snapshot = input.snapshot;
    if (!snapshot.bms_valid || snapshot.bms_received_ms > now ||
        now - snapshot.bms_received_ms >= cfg.runtime.bms_stale_ms)
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
    Config policy = cfg.runtime.policy;
    policy.revised_monitoring = true;
    Inputs demand = snapshot.policy;
    demand.effort_factor[0] = monitoring.factor[0];
    demand.effort_factor[1] = monitoring.factor[1];
    State preview = state.runtime.policy;
    faults |= evaluate(policy, demand, now, preview).faults;
    return faults;
}
} // namespace
bool initialize_release(uint32_t clock_ms, Releasestate& state, Releaseport& port)
{
    state = Releasestate();
    const bool initialized = initialize(clock_ms, state.runtime, port);
    const bool discarded = port.discardpending();
    if (!initialized || !discarded)
    {
        state.latched_faults |= inhibit_fault;
    }
    return initialized && discarded;
}
Releasecycle run_release(const Releaseconfig& cfg,
                         const Releaseinput& input,
                         uint32_t clock_ms,
                         Releasestate& state,
                         Releaseport& port)
{
    Releasecycle result = Releasecycle();
    const uint32_t elapsed = clock_ms - state.runtime.last_clock_ms;
    const Timestamp now = state.runtime.monotonic_ms + elapsed;
    result.monitoring = monitor(cfg, input, now, state.monitor);
    uint32_t active = result.monitoring.faults;
    active |= acquisitionfaults(cfg, input, result.monitoring, now, state);
    if (!releasevalid(cfg, port))
    {
        active |= runtime_config_fault;
    }
    if (!driveallowed(input) || active != 0U)
    {
        inhibit(state, port);
    }
    if (state.pending)
    {
        active |= dispatchstatus(cfg, now, state, port);
    }
    state.latched_faults |= active;
    if (active == 0U && resetallowed(input))
    {
        const bool safe = inhibit(state, port);
        // A failed physical inhibit or purge must remain latched during a reset attempt.
        if (safe)
        {
            state.latched_faults = 0U;
        }
    }
    if (!state.pending)
    {
        Runtimeconfig runtime = cfg.runtime;
        runtime.policy.revised_monitoring = true;
        Snapshot snapshot = input.snapshot;
        for (unsigned channel = 0U; channel < 2U; ++channel)
        {
            snapshot.policy.effort_factor[channel] = result.monitoring.factor[channel];
            // Component channels above own thermal validity; legacy temperature fields are unused.
        }
        snapshot.policy.permission = snapshot.policy.permission && state.latched_faults == 0U;
        const bool due = elapsed >= runtime.period_ms;
        if (due && state.latched_faults == 0U && !port.beginpair(++state.generation))
        {
            state.latched_faults |= dispatch_fault;
            snapshot.policy.permission = false;
        }
        Queueport queue(port);
        result.runtime = run(runtime, snapshot, clock_ms, state.runtime, queue);
        active |= result.runtime.faults;
        state.latched_faults |= active;
        if (result.runtime.dispatched)
        {
            state.pending = true;
            state.queued_ms = now;
        }
        else if (result.runtime.due)
        {
            inhibit(state, port);
        }
    }
    if (state.latched_faults != 0U)
    {
        inhibit(state, port);
    }
    runtimerecords(state, active, now);
    if (state.monitor.overflow)
    {
        state.latched_faults |= config_fault;
        inhibit(state, port);
    }
    result.faults = state.latched_faults;
    result.pending = state.pending;
    result.released = state.released;
    return result;
}
} // namespace Capstone
