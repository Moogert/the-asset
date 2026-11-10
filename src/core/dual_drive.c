// C policy/reference model for coupled motors; currents and effort envelopes are study assumptions.
#include "dual_drive.h"
#include <math.h>
#include <string.h>

static bool range(float x, float low, float high)
{
    return isfinite(x) && x >= low && x <= high;
}

dual_config_t dual_simulation_config(void)
{
    const dual_config_t cfg = {
        100U, 5000.0F, 70.0F, 90.0F, 5.0F, 5.0F, {100.0F, 150.0F}, {40.0F, 60.0F}};
    return cfg;
}

void dual_drive_evaluate(const dual_config_t* cfg,
                         const dual_inputs_t* in,
                         uint64_t now_ms,
                         dual_outputs_t* out)
{
    if (out == NULL)
        return;
    const uint64_t last = out->last_step_ms;
    const bool stepped = out->has_step;
    const bool latched = out->latched;
    const uint32_t history = out->latched_faults;
    const dual_mode_t previous_mode = out->previous_mode;
    memset(out, 0, sizeof(*out));
    out->last_step_ms = now_ms;
    out->has_step = true;
    out->latched = latched;
    out->latched_faults = history;
    if (cfg == NULL || in == NULL)
    {
        out->faults = DUAL_CONFIG;
        out->latched = true;
        out->latched_faults |= out->faults;
        return;
    }
    out->previous_mode = in->mode;
    if (cfg->stale_ms == 0U || cfg->stale_ms > 100U || !range(cfg->max_rpm, 1.0F, 20000.0F) ||
        !range(cfg->derate_c, -20.0F, 180.0F) ||
        !range(cfg->stop_c, cfg->derate_c + 1.0F, 200.0F) ||
        !range(cfg->pack_current_tolerance_a, 0.0F, 50.0F) ||
        !range(cfg->circulation_threshold_a, 0.0F, 50.0F) || in->mode < DUAL_MG2_ONLY ||
        in->mode > DUAL_COMBINED || !range(in->request_percent, -100.0F, 100.0F))
        out->faults |= DUAL_CONFIG;
    if (stepped && (now_ms < last || now_ms - last > cfg->stale_ms))
        out->faults |= DUAL_SCHEDULER;
    if (stepped && previous_mode != in->mode && in->arm && in->drive_request)
        out->faults |= DUAL_CONFIG;
    if (!range(in->discharge_limit_a, 0.0F, 5000.0F) || !range(in->charge_limit_a, 0.0F, 5000.0F) ||
        !range(in->pack_current_a, -5000.0F, 5000.0F))
        out->faults |= DUAL_PACK;
    else if (in->pack_current_a > in->discharge_limit_a + cfg->pack_current_tolerance_a ||
             -in->pack_current_a > in->charge_limit_a + cfg->pack_current_tolerance_a)
        out->faults |= DUAL_PACK;
    if (in->bus_off || (in->arm && in->drive_request && !in->permission))
        out->faults |= DUAL_PERMISSION;
    float thermal[2] = {1.0F, 1.0F};
    for (unsigned i = 0U; i < 2U; ++i)
    {
        const dual_feedback_t* m = &in->motor[i];
        if (!range(cfg->draw_a[i], 0.1F, 5000.0F) || !range(cfg->regen_a[i], 0.1F, 5000.0F))
            out->faults |= DUAL_CONFIG;
        if (!m->valid || m->received_ms > now_ms || now_ms - m->received_ms >= cfg->stale_ms)
            out->faults |= DUAL_STALE;
        if (!range(m->rpm, -20000.0F, 20000.0F) || !range(m->motor_temp_c, -40.0F, 200.0F) ||
            !range(m->inverter_temp_c, -40.0F, 200.0F) ||
            !range(m->dc_current_a, -5000.0F, 5000.0F))
            out->faults |= DUAL_FEEDBACK;
        if (!m->rotor_valid || m->fault || !m->ready)
            out->faults |= DUAL_CHANNEL;
        if (fabsf(m->rpm) >= cfg->max_rpm)
            out->faults |= DUAL_SPEED;
        const float temp = fmaxf(m->motor_temp_c, m->inverter_temp_c);
        if (temp >= cfg->stop_c)
            out->faults |= DUAL_THERMAL;
        else if (temp > cfg->derate_c)
            thermal[i] = (cfg->stop_c - temp) / (cfg->stop_c - cfg->derate_c);
    }
    const float threshold = cfg->circulation_threshold_a;
    /* Locked P112 planetary: calibrated signed shaft speeds must agree.
       50 rpm + 1% is a simulation tolerance, to be measured for the real sensors. */
    if (fabsf(in->motor[0].rpm - in->motor[1].rpm) >
        50.0F + 0.01F * fmaxf(fabsf(in->motor[0].rpm), fabsf(in->motor[1].rpm)))
        out->faults |= DUAL_KINEMATICS;
    if ((in->motor[0].dc_current_a > threshold && in->motor[1].dc_current_a < -threshold) ||
        (in->motor[1].dc_current_a > threshold && in->motor[0].dc_current_a < -threshold))
        out->faults |= DUAL_CIRCULATION;
    if (out->faults != 0U)
    {
        out->latched = true;
        out->latched_faults |= out->faults;
    }
    else if (in->reset_request && !in->arm && !in->drive_request && in->request_percent == 0.0F)
    {
        out->latched = false;
        out->latched_faults = 0U;
    }
    if (out->latched || !in->arm || !in->drive_request || !in->permission)
        return;
    const bool regen = in->request_percent < 0.0F;
    const float budget = regen ? in->charge_limit_a : in->discharge_limit_a;
    if (budget <= 0.0F)
        return;
    float demand = 0.0F;
    for (unsigned i = 0U; i < 2U; ++i)
    {
        const bool selected = in->mode == DUAL_COMBINED || (i == 0U && in->mode == DUAL_MG1_ONLY) ||
                              (i == 1U && in->mode == DUAL_MG2_ONLY);
        out->command_percent[i] = selected ? in->request_percent * thermal[i] : 0.0F;
        const float envelope = regen ? cfg->regen_a[i] : cfg->draw_a[i];
        out->allocated_pack_a[i] = envelope * fabsf(out->command_percent[i]) / 100.0F;
        demand += out->allocated_pack_a[i];
    }
    const float scale = demand > budget ? budget / demand : 1.0F;
    for (unsigned i = 0U; i < 2U; ++i)
    {
        out->command_percent[i] *= scale;
        out->allocated_pack_a[i] *= scale;
        if (regen)
            out->allocated_pack_a[i] = -out->allocated_pack_a[i];
    }
    out->enabled = true;
}
