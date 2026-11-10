#include <control/release_control.h>
namespace Capstone
{
namespace
{
bool bounded(Real value, Real low, Real high)
{
    return value >= low && value <= high;
}
Real absolute(Real value)
{
    return value < 0.0F ? -value : value;
}
bool fresh(const Measurement& m, Timestamp now, uint32_t age, Real low, Real high)
{
    return m.valid && bounded(m.value, low, high) && bounded(m.uncertainty, 0.0F, high - low) &&
           m.acquired_ms <= now && now - m.acquired_ms < age;
}
bool validlimit(const Temperaturelimit& limit)
{
    return bounded(limit.derate_c, -30.0F, 180.0F) &&
           bounded(limit.stop_c, limit.derate_c + 1.0F, 200.0F) &&
           bounded(limit.hysteresis_c, 0.1F, limit.stop_c - limit.derate_c) &&
           limit.stale_ms > 0U && limit.stale_ms <= 1000U && limit.filter_ms <= 10000U;
}
Real thermal(const Temperaturelimit& limit,
             const Measurement& m,
             unsigned i,
             Timestamp now,
             Monitorstate& state)
{
    // Update only on a new acquisition; polling one sample repeatedly cannot advance the filter.
    if (!state.initialized[i])
    {
        state.filtered[i] = m.value;
        state.initialized[i] = true;
    }
    else if (m.acquired_ms > state.sampled[i])
    {
        const Real dt = static_cast<Real>(m.acquired_ms - state.sampled[i]);
        const Real alpha = dt / (static_cast<Real>(limit.filter_ms) + dt);
        state.filtered[i] += alpha * (m.value - state.filtered[i]);
    }
    state.sampled[i] = m.acquired_ms;
    const Real upper = m.value + m.uncertainty;
    if (upper >= limit.stop_c)
    {
        state.hot[i] = true;
    }
    else if (upper <= limit.stop_c - limit.hysteresis_c)
    {
        state.hot[i] = false;
    }
    const Component component = static_cast<Component>(i);
    recordfault(state,
                thermal_fault,
                component,
                shutdown,
                state.hot[i],
                now,
                faultvalue(upper, limit.stop_c, true));
    const Real filtered = state.filtered[i] + m.uncertainty;
    const Real factor = filtered <= limit.derate_c
                            ? 1.0F
                            : (filtered >= limit.stop_c
                                   ? 0.0F
                                   : (limit.stop_c - filtered) / (limit.stop_c - limit.derate_c));
    recordfault(state,
                thermal_fault,
                component,
                derating,
                factor < 1.0F,
                now,
                faultvalue(filtered, limit.derate_c, true));
    return state.hot[i] ? 0.0F : factor;
}
void temperatures(const Releaseconfig& cfg,
                  const Releaseinput& input,
                  Timestamp now,
                  Monitorstate& state,
                  Monitorresult& out)
{
    for (unsigned i = 0U; i < temperature_count; ++i)
    {
        const Temperaturelimit& limit = cfg.temperature[i];
        const bool valid =
            validlimit(limit) && limit.sensor_id != 0U &&
            input.temperature[i].sensor_id == limit.sensor_id &&
            fresh(input.temperature[i], now, limit.stale_ms, -40.0F, 200.0F) &&
            (!state.initialized[i] || input.temperature[i].acquired_ms >= state.sampled[i]);
        recordfault(
            state,
            measurement_fault,
            static_cast<Component>(i),
            limit.required ? shutdown : advisory,
            !valid,
            now,
            faultvalue(input.temperature[i].value, static_cast<Real>(limit.stale_ms), false));
        if (!validlimit(limit))
        {
            out.faults |= config_fault;
        }
        if (!valid)
        {
            state.initialized[i] = false;
            if (limit.required || state.hot[i])
            {
                out.faults |= measurement_fault;
            }
        }
        else
        {
            const Real factor = thermal(limit, input.temperature[i], i, now, state);
            if (state.hot[i])
            {
                out.faults |= thermal_fault;
            }
            for (unsigned channel = 0U; channel < 2U; ++channel)
            {
                if ((i >= 4U || (i % 2U) == channel) && factor < out.factor[channel])
                {
                    out.factor[channel] = factor;
                }
            }
        }
    }
}
bool circulationvalid(const Releaseconfig& cfg)
{
    return bounded(cfg.opposing_w, 1.0F, 1000000.0F) && cfg.power_stale_ms > 0U &&
           cfg.power_stale_ms <= 100U && cfg.power_skew_ms <= cfg.power_stale_ms &&
           cfg.confirmation_ms > 0U && cfg.confirmation_ms <= 10000U;
}
bool synchronizedpower(const Releaseconfig& cfg,
                       const Measurement& a,
                       const Measurement& b,
                       Timestamp now)
{
    const Timestamp skew = a.acquired_ms > b.acquired_ms ? a.acquired_ms - b.acquired_ms
                                                         : b.acquired_ms - a.acquired_ms;
    return fresh(a, now, cfg.power_stale_ms, -1000000.0F, 1000000.0F) &&
           fresh(b, now, cfg.power_stale_ms, -1000000.0F, 1000000.0F) && skew <= cfg.power_skew_ms;
}
void circulation(const Releaseconfig& cfg,
                 const Releaseinput& input,
                 Timestamp now,
                 Monitorstate& state,
                 Monitorresult& out)
{
    const Measurement& a = input.power[0];
    const Measurement& b = input.power[1];
    out.power_available = synchronizedpower(cfg, a, b, now);
    if (!circulationvalid(cfg))
    {
        out.faults |= config_fault;
    }
    recordfault(state,
                measurement_fault,
                shared,
                cfg.circulation_required ? shutdown : advisory,
                !out.power_available,
                now,
                faultvalue(0.0F, 0.0F, false));
    if (!out.power_available && cfg.circulation_required)
    {
        out.faults |= measurement_fault;
    }
    const bool opposite = (a.value > 0.0F && b.value < 0.0F) || (a.value < 0.0F && b.value > 0.0F);
    const Real lower_a = absolute(a.value) - a.uncertainty;
    const Real lower_b = absolute(b.value) - b.uncertainty;
    const Real lower = lower_a < lower_b ? lower_a : lower_b;
    out.circulation_suspected = out.power_available && opposite && lower > cfg.opposing_w;
    // A gap between evaluations cannot count as continuously observed opposing power.
    if (!out.circulation_suspected || now < state.evaluated_ms ||
        now - state.evaluated_ms >= cfg.power_stale_ms)
    {
        state.opposing = false;
    }
    if (out.circulation_suspected && !state.opposing)
    {
        state.opposing = true;
        state.opposing_since = now;
    }
    const bool confirmed = state.opposing && now - state.opposing_since >= cfg.confirmation_ms;
    if (confirmed)
    {
        out.faults |= circulation_fault;
    }
    recordfault(state,
                circulation_fault,
                shared,
                advisory,
                out.circulation_suspected,
                now,
                faultvalue(lower, cfg.opposing_w, out.power_available));
    recordfault(state,
                circulation_fault,
                shared,
                shutdown,
                confirmed,
                now,
                faultvalue(lower, cfg.opposing_w, out.power_available));
}
} // namespace
// Fixed keys prevent unbounded logging; full capacity is explicitly fail-closed.
Faultvalue faultvalue(Real measured, Real threshold, bool valid)
{
    const Faultvalue value = {measured, threshold, valid};
    return value;
}
void recordfault(Monitorstate& state,
                 uint32_t identity,
                 Component component,
                 Severity severity,
                 bool active,
                 Timestamp now,
                 const Faultvalue& value)
{
    unsigned i = 0U;
    while (i < state.count &&
           !(state.records[i].identity == identity && state.records[i].component == component &&
             state.records[i].severity == severity))
    {
        ++i;
    }
    if (i == state.count && active)
    {
        if (state.count == record_capacity)
        {
            state.overflow = true;
        }
        else
        {
            state.records[i] = Faultrecord();
            state.records[i].identity = identity;
            state.records[i].component = component;
            state.records[i].severity = severity;
            state.records[i].first_ms = now;
            ++state.count;
        }
    }
    if (i < state.count)
    {
        Faultrecord& r = state.records[i];
        if (active && !r.active && r.occurrences < UINT32_MAX)
        {
            ++r.occurrences;
        }
        r.active = active;
        if (active)
        {
            r.latest_ms = now;
            r.measured = value.valid ? value.measured : 0.0F;
            r.threshold = value.threshold;
            r.valid = value.valid;
        }
    }
}
Monitorresult
monitor(const Releaseconfig& cfg, const Releaseinput& input, Timestamp now, Monitorstate& state)
{
    Monitorresult out = Monitorresult();
    out.factor[0] = 1.0F;
    out.factor[1] = 1.0F;
    if (now < state.evaluated_ms)
    {
        out.faults |= scheduler_fault;
    }
    temperatures(cfg, input, now, state, out);
    circulation(cfg, input, now, state, out);
    state.evaluated_ms = now;
    if (state.overflow)
    {
        out.faults |= config_fault;
    }
    const Real minimum = out.factor[0] < out.factor[1] ? out.factor[0] : out.factor[1];
    out.cooling_percent = out.faults ? 100U : static_cast<uint8_t>((1.0F - minimum) * 100.0F);
    return out;
}
// Demonstration values are intentionally unrelated to component operating ratings.
Releaseconfig demonstration_release_config()
{
    Releaseconfig cfg = Releaseconfig();
    cfg.runtime.policy = simulationconfig();
    cfg.runtime.period_ms = 10U;
    cfg.runtime.deadline_ms = 10U;
    cfg.runtime.bms_stale_ms = 80U;
    cfg.runtime.policy.stale_ms = 80U;
    cfg.runtime.inhibit_budget_ms = 10U;
    cfg.runtime.control_id[0] = 0X201U;
    cfg.runtime.control_id[1] = 0X202U;
    cfg.runtime.rise_percent_per_second = 100.0F;
    for (unsigned i = 0U; i < temperature_count; ++i)
    {
        Temperaturelimit& limit = cfg.temperature[i];
        limit.required = i < 4U;
        limit.location = i < 2U ? Temperaturelimit::winding
                                : (i < 4U ? Temperaturelimit::case_sensor
                                          : (i == 4U ? Temperaturelimit::oil_sensor
                                                     : Temperaturelimit::coolant_sensor));
        limit.sensor_id = static_cast<uint16_t>(i + 1U);
        limit.derate_c = 70.0F;
        limit.stop_c = 90.0F;
        limit.hysteresis_c = 5.0F;
        limit.stale_ms = 80U;
        limit.filter_ms = 100U;
    }
    cfg.circulation_required = true;
    cfg.opposing_w = 500.0F;
    cfg.power_stale_ms = 80U;
    cfg.power_skew_ms = 5U;
    cfg.confirmation_ms = 100U;
    cfg.command_age_ms = 5U;
    cfg.pair_skew_ms = 2U;
    return cfg;
}
} // namespace Capstone
