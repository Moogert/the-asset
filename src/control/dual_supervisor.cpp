#include <control/dual_supervisor.h>

namespace Capstone
{
namespace
{
bool within(Real value, Real low, Real high)
{
    // Ordered bounded comparisons reject NaN and both infinities.
    return (value >= low) && (value <= high);
}

Real magnitude(Real value)
{
    return (value < 0.0F) ? -value : value;
}

Real maximum(Real first, Real second)
{
    return ((first > second) || (second != second)) ? first : second;
}

// Validate study parameters and both current envelopes before allocating either motor.
uint32_t configfaults(const Config& cfg, const Inputs& input)
{
    uint32_t faults = 0U;
    if ((cfg.stale_ms == 0U) || (cfg.stale_ms > 100U) || !within(cfg.max_rpm, 1.0F, 20000.0F) ||
        !within(cfg.derate_c, -20.0F, 180.0F) || !within(cfg.stop_c, cfg.derate_c + 1.0F, 200.0F) ||
        !within(cfg.pack_current_tolerance_a, 0.0F, 50.0F) ||
        !within(cfg.circulation_threshold_a, 0.0F, 50.0F) || (input.mode < mg2_only) ||
        (input.mode > combined) || !within(input.request_percent, -100.0F, 100.0F))
    {
        faults |= config_fault;
    }
    for (unsigned channel = 0U; channel < 2U; ++channel)
    {
        if (!within(cfg.draw_a[channel], 0.1F, 5000.0F) ||
            !within(cfg.regen_a[channel], 0.1F, 5000.0F) ||
            (cfg.revised_monitoring && !within(input.effort_factor[channel], 0.0F, 1.0F)))
        {
            faults |= config_fault;
        }
    }
    return faults;
}

// Check signed pack-current limits and shared drive permission; positive current denotes discharge.
uint32_t packfaults(const Config& cfg, const Inputs& input)
{
    uint32_t faults = 0U;
    if (!within(input.discharge_limit_a, 0.0F, 5000.0F) ||
        !within(input.charge_limit_a, 0.0F, 5000.0F) ||
        !within(input.pack_current_a, -5000.0F, 5000.0F))
    {
        faults |= pack_fault;
    }
    else if ((input.pack_current_a > input.discharge_limit_a + cfg.pack_current_tolerance_a) ||
             (-input.pack_current_a > input.charge_limit_a + cfg.pack_current_tolerance_a))
    {
        faults |= pack_fault;
    }
    if (input.bus_off || (input.arm && input.drive_request && !input.permission))
    {
        faults |= permission_fault;
    }
    return faults;
}

// Require plausible, fresh feedback from each mechanically coupled machine, even in single-motor mode.
uint32_t motorfaults(const Config& cfg, const Motorfeedback& motor, Timestamp now_ms)
{
    uint32_t faults = 0U;
    if (!motor.valid || (motor.received_ms > now_ms) ||
        ((now_ms - motor.received_ms) >= cfg.stale_ms))
    {
        faults |= stale_fault;
    }
    if (!within(motor.rpm, -20000.0F, 20000.0F) ||
        (!cfg.revised_monitoring && (!within(motor.motor_temp_c, -40.0F, 200.0F) ||
                                     !within(motor.inverter_temp_c, -40.0F, 200.0F))) ||
        !within(motor.dc_current_a, -5000.0F, 5000.0F))
    {
        faults |= feedback_fault;
    }
    if (!motor.rotor_valid || motor.fault || !motor.ready)
    {
        faults |= channel_fault;
    }
    if (magnitude(motor.rpm) >= cfg.max_rpm)
    {
        faults |= speed_fault;
    }
    if (!cfg.revised_monitoring && maximum(motor.motor_temp_c, motor.inverter_temp_c) >= cfg.stop_c)
    {
        faults |= thermal_fault;
    }
    return faults;
}

// Check gearset speed consistency and sustained opposing current signs using study thresholds.
uint32_t coupledchecks(const Config& cfg, const Inputs& input)
{
    uint32_t faults = 0U;
    const Real speed_difference = magnitude(input.motor[0].rpm - input.motor[1].rpm);
    const Real speed = maximum(magnitude(input.motor[0].rpm), magnitude(input.motor[1].rpm));
    // Locked-planetary simulation tolerance; not a validated sensor tolerance.
    if (speed_difference > 50.0F + 0.01F * speed)
    {
        faults |= kinematics_fault;
    }
    const Real threshold = cfg.circulation_threshold_a;
    const Real first = input.motor[0].dc_current_a;
    const Real second = input.motor[1].dc_current_a;
    if (!cfg.revised_monitoring && (((first > threshold) && (second < -threshold)) ||
                                    ((second > threshold) && (first < -threshold))))
    {
        faults |= circulation_fault;
    }
    return faults;
}

// Return a bounded thermal effort factor between derating onset and shutdown.
Real thermalfactor(const Config& cfg, const Motorfeedback& motor)
{
    const Real temperature = maximum(motor.motor_temp_c, motor.inverter_temp_c);
    Real factor = 1.0F;
    if (temperature > cfg.derate_c)
    {
        factor = (cfg.stop_c - temperature) / (cfg.stop_c - cfg.derate_c);
    }
    return factor;
}

// Share one simulated pack-current envelope; normalized effort is not calibrated shaft torque.
Outputs allocate(const Config& cfg, const Inputs& input)
{
    Outputs output = Outputs();
    const bool regen = input.request_percent < 0.0F;
    const Real budget = regen ? input.charge_limit_a : input.discharge_limit_a;
    if (budget > 0.0F)
    {
        Real demand = 0.0F;
        for (unsigned channel = 0U; channel < 2U; ++channel)
        {
            const bool selected = (input.mode == combined) ||
                                  ((channel == 0U) && (input.mode == mg1_only)) ||
                                  ((channel == 1U) && (input.mode == mg2_only));
            output.command_percent[channel] =
                selected ? input.request_percent * (cfg.revised_monitoring
                                                        ? input.effort_factor[channel]
                                                        : thermalfactor(cfg, input.motor[channel]))
                         : 0.0F;
            const Real envelope = regen ? cfg.regen_a[channel] : cfg.draw_a[channel];
            output.allocated_pack_a[channel] =
                envelope * magnitude(output.command_percent[channel]) / 100.0F;
            demand += output.allocated_pack_a[channel];
        }
        const Real scale = (demand > budget) ? budget / demand : 1.0F;
        for (unsigned channel = 0U; channel < 2U; ++channel)
        {
            output.command_percent[channel] *= scale;
            output.allocated_pack_a[channel] *= scale;
            if (regen)
            {
                output.allocated_pack_a[channel] = -output.allocated_pack_a[channel];
            }
        }
        output.enabled = true;
    }
    return output;
}
} // namespace

// Provide host-study defaults only; these are not validated hardware commissioning values.
Config simulationconfig()
{
    // Simulation envelopes only; NOT physical limits or a flashable configuration.
    const Config cfg = {
        100U, 5000.0F, 70.0F, 90.0F, 5.0F, 5.0F, {100.0F, 150.0F}, {40.0F, 60.0F}, false};
    return cfg;
}

// Apply faults/latches before effort allocation; healthy disarmed reset is required after a fault.
Outputs evaluate(const Config& cfg, const Inputs& input, Timestamp now_ms, State& state)
{
    Outputs output = Outputs();
    uint32_t faults = configfaults(cfg, input) | packfaults(cfg, input) | coupledchecks(cfg, input);
    if (state.has_step &&
        ((now_ms < state.last_step_ms) || ((now_ms - state.last_step_ms) > cfg.stale_ms)))
    {
        faults |= scheduler_fault;
    }
    if (state.has_step && (state.previous_mode != input.mode) && input.arm && input.drive_request)
    {
        faults |= config_fault;
    }
    for (unsigned channel = 0U; channel < 2U; ++channel)
    {
        faults |= motorfaults(cfg, input.motor[channel], now_ms);
    }
    state.last_step_ms = now_ms;
    state.has_step = true;
    state.previous_mode = input.mode;
    if (faults != 0U)
    {
        state.latched = true;
        state.latched_faults |= faults;
    }
    else if (input.reset_request && !input.arm && !input.drive_request &&
             (input.request_percent == 0.0F))
    {
        state.latched = false;
        state.latched_faults = 0U;
    }
    if (!state.latched && input.arm && input.drive_request && input.permission)
    {
        output = allocate(cfg, input);
    }
    output.faults = faults;
    return output;
}
} // namespace Capstone
