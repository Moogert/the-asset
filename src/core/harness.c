// Coordinate synthetic devices, scenario injection and host state; do not use as a live vehicle controller.
#include "harness.h"
#include "json_util.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define SIM_BMS_ID 0x180U
#define SIM_INVERTER_ID 0x280U
#define SIM_CHARGER_ID 0x380U
#define SIM_STALE_MS 250U
#define SIM_MG1_ID 0x281U
#define SIM_MG2_ID 0x282U

static void emit_motor(harness_t* h, unsigned i)
{
    const uint32_t timeout = i == 0U ? INJECT_MG1_TIMEOUT : INJECT_MG2_TIMEOUT;
    if ((h->active_injected_faults & timeout) != 0U)
        return;
    const dual_feedback_t* m = &h->motor_sim[i];
    /* Invalid values are handled locally as invalid feedback, never cast to integers. */
    if (!m->valid || !isfinite(m->rpm) || fabsf(m->rpm) > 20000.0F || !isfinite(m->dc_current_a) ||
        fabsf(m->dc_current_a) > 3000.0F || !isfinite(m->motor_temp_c) ||
        m->motor_temp_c < -40.0F || m->motor_temp_c > 200.0F || !isfinite(m->inverter_temp_c) ||
        m->inverter_temp_c < -40.0F || m->inverter_temp_c > 200.0F)
    {
        h->motor_rx[i].valid = false;
        return;
    }
    can_frame_t f = {0};
    f.id = i == 0U ? SIM_MG1_ID : SIM_MG2_ID;
    f.source = i == 0U ? CAN_NODE_MG1 : CAN_NODE_MG2;
    f.dlc = 8U;
    const uint8_t seq = h->motor_sequence[i]++ & 15U;
    const uint32_t fault = i == 0U ? INJECT_MG1_FAULT : INJECT_MG2_FAULT;
    const uint32_t resolver = i == 0U ? INJECT_MG1_RESOLVER : INJECT_MG2_RESOLVER;
    f.data[0] = (uint8_t)((seq << 4U) | (m->ready ? 1U : 0U) |
                          ((m->rotor_valid && !(h->active_injected_faults & resolver)) ? 2U : 0U) |
                          ((m->fault || (h->active_injected_faults & fault)) ? 4U : 0U));
    const uint16_t rpm = (uint16_t)(int16_t)m->rpm;
    const uint16_t current = (uint16_t)(int16_t)(m->dc_current_a * 10.0F);
    f.data[1] = (uint8_t)rpm;
    f.data[2] = (uint8_t)(rpm >> 8U);
    f.data[3] = (uint8_t)(m->motor_temp_c + 40.0F);
    f.data[4] = (uint8_t)(m->inverter_temp_c + 40.0F);
    f.data[5] = (uint8_t)current;
    f.data[6] = (uint8_t)(current >> 8U);
    f.data[7] = can_crc8(f.data, 7U);
    (void)can_bus_transmit(&h->bus, &f, h->time_ms);
}

static bool receive_motor(harness_t* h, const can_frame_t* f, unsigned i)
{
    const uint8_t seq = f->data[0] >> 4U;
    if (f->dlc != 8U || f->source != (i == 0U ? CAN_NODE_MG1 : CAN_NODE_MG2) ||
        (f->data[0] & 8U) != 0U || f->data[7] != can_crc8(f->data, 7U) ||
        (h->motor_expected[i] != 255U && seq != h->motor_expected[i]) ||
        f->timestamp_ms > h->time_ms || h->time_ms - f->timestamp_ms >= h->dual_config.stale_ms)
    {
        h->motor_rx[i].valid = false;
        return false;
    }
    h->motor_expected[i] = (uint8_t)((seq + 1U) & 15U);
    dual_feedback_t* m = &h->motor_rx[i];
    m->valid = true;
    m->ready = (f->data[0] & 1U) != 0U;
    m->rotor_valid = (f->data[0] & 2U) != 0U;
    m->fault = (f->data[0] & 4U) != 0U;
    m->rpm = (float)(int16_t)((uint16_t)f->data[1] | ((uint16_t)f->data[2] << 8U));
    m->motor_temp_c = (float)f->data[3] - 40.0F;
    m->inverter_temp_c = (float)f->data[4] - 40.0F;
    m->dc_current_a = (float)(int16_t)((uint16_t)f->data[5] | ((uint16_t)f->data[6] << 8U)) / 10.0F;
    m->received_ms = f->timestamp_ms;
    return true;
}

static bool injected(const harness_t* h, uint32_t flag)
{
    return (h->active_injected_faults & flag) != 0U;
}

static void add_event(harness_t* h, const char* text)
{
    if (h->event_count >= HARNESS_EVENT_MAX)
        return;
    h->events[h->event_count].time_ms = h->time_ms;
    h->events[h->event_count].faults = h->safety.faults;
    (void)snprintf(
        h->events[h->event_count].text, sizeof(h->events[h->event_count].text), "%s", text);
    h->event_count++;
}

static can_frame_t make_frame(uint32_t id, can_node_t source, uint8_t sequence)
{
    can_frame_t f;
    memset(&f, 0, sizeof(f));
    f.id = id;
    f.dlc = 8U;
    f.source = source;
    f.sequence = sequence;
    return f;
}

static void emit_bms(harness_t* h)
{
    if (injected(h, INJECT_BMS_TIMEOUT))
        return;
    const leaf_bms_status_t* s = &h->inputs.bms;
    if (!isfinite(s->charge_limit_a) || s->charge_limit_a < 0.0F || s->charge_limit_a > 255.0F ||
        !isfinite(s->discharge_limit_a) || s->discharge_limit_a < 0.0F ||
        s->discharge_limit_a > 255.0F || !isfinite(s->pack_voltage_v) || s->pack_voltage_v < 0.0F ||
        s->pack_voltage_v > 1000.0F || !isfinite(s->min_temp_c) || s->min_temp_c < -40.0F ||
        s->min_temp_c > 200.0F || !isfinite(s->max_temp_c) || s->max_temp_c < -40.0F ||
        s->max_temp_c > 200.0F)
        return;
    can_frame_t f = make_frame(SIM_BMS_ID, CAN_NODE_BMS, h->bms_sequence++);
    f.data[0] = (uint8_t)((h->inputs.bms.contactor_closed ? 1U : 0U) |
                          (h->inputs.bms.interlock_ok ? 2U : 0U));
    f.data[1] = injected(h, INJECT_ZERO_LIMITS) ? 0U : (uint8_t)h->inputs.bms.charge_limit_a;
    f.data[2] = injected(h, INJECT_ZERO_LIMITS) ? 0U : (uint8_t)h->inputs.bms.discharge_limit_a;
    f.data[3] = (uint8_t)(h->inputs.bms.min_temp_c + 40.0F);
    f.data[4] = (uint8_t)(h->inputs.bms.max_temp_c + 40.0F);
    const uint16_t voltage = (uint16_t)(h->inputs.bms.pack_voltage_v / 2.0F);
    f.data[5] = (uint8_t)(voltage >> 8U);
    f.data[6] = (uint8_t)voltage;
    f.data[7] = can_crc8(f.data, 7U);
    if (injected(h, INJECT_BAD_CRC))
        f.data[7] ^= 0x5AU;
    (void)can_bus_transmit(&h->bus, &f, h->time_ms);
}

static void emit_inverter(harness_t* h)
{
    if (injected(h, INJECT_INVERTER_TIMEOUT))
        return;
    if (!isfinite(h->inputs.speed_rpm) || fabsf(h->inputs.speed_rpm) > 20000.0F ||
        !isfinite(h->inputs.motor_temp_c) || h->inputs.motor_temp_c < -40.0F ||
        h->inputs.motor_temp_c > 200.0F)
        return;
    can_frame_t f = make_frame(SIM_INVERTER_ID, CAN_NODE_INVERTER, h->inverter_sequence++);
    f.data[0] = (uint8_t)((h->inputs.inverter_ready ? 1U : 0U) |
                          (injected(h, INJECT_INVERTER_FAULT) ? 2U : 0U) |
                          (injected(h, INJECT_INVERTER_DESAT) ? 4U : 0U));
    const uint16_t rpm = (uint16_t)(h->inputs.speed_rpm > 0.0F ? h->inputs.speed_rpm : 0.0F);
    f.data[1] = (uint8_t)(rpm >> 8U);
    f.data[2] = (uint8_t)rpm;
    f.data[3] = (uint8_t)(h->inputs.motor_temp_c + 40.0F);
    f.data[4] = can_crc8(f.data, 4U);
    (void)can_bus_transmit(&h->bus, &f, h->time_ms);
}

static void emit_charger(harness_t* h)
{
    if (injected(h, INJECT_CHARGER_TIMEOUT))
        return;
    can_frame_t f = make_frame(SIM_CHARGER_ID, CAN_NODE_CHARGER, h->charger_sequence++);
    f.data[0] =
        (uint8_t)((h->inputs.charger_ready ? 1U : 0U) | (h->inputs.charger_fault ? 2U : 0U));
    f.data[1] = h->inputs.ac_present ? 1U : 0U;
    f.data[2] = can_crc8(f.data, 2U);
    (void)can_bus_transmit(&h->bus, &f, h->time_ms);
}

static void consume_frames(
    harness_t* h, uint32_t dt_ms, bool* bms_ok, bool* inverter_ok, bool* charger_ok, bool* bad)
{
    can_frame_t f;
    while (can_bus_receive(&h->bus, &f))
    {
        if (f.id == SIM_BMS_ID)
        {
            leaf_bms_status_t status;
            if (leaf_bms_decode(&f, h->time_ms, &h->bms_rx_expected, &status))
            {
                h->inputs.bms = status;
                *bms_ok = true;
                h->bms_age_ms = 0U;
            }
            else
                *bad = true;
        }
        else if (f.id == SIM_MG1_ID || f.id == SIM_MG2_ID)
        {
            if (!receive_motor(h, &f, f.id == SIM_MG1_ID ? 0U : 1U))
                *bad = true;
        }
        else if (f.id == SIM_INVERTER_ID)
        {
            if (f.dlc != 8U || f.data[4] != can_crc8(f.data, 4U))
                *bad = true;
            else
            {
                *inverter_ok = true;
                h->inverter_age_ms = 0U;
            }
        }
        else if (f.id == SIM_CHARGER_ID)
        {
            if (f.dlc != 8U || f.data[2] != can_crc8(f.data, 2U))
                *bad = true;
            else
            {
                *charger_ok = true;
                h->charger_age_ms = 0U;
            }
        }
    }
    h->bms_age_ms += dt_ms;
    h->inverter_age_ms += dt_ms;
    h->charger_age_ms += dt_ms;
}

void harness_init(harness_t* h)
{
    memset(h, 0, sizeof(*h));
    h->bms_rx_expected = 0xFFU;
    can_bus_init(&h->bus, 500000U);
    h->dual_config = dual_simulation_config();
    h->dual_mode = DUAL_MG2_ONLY;
    for (unsigned i = 0U; i < 2U; ++i)
    {
        h->motor_expected[i] = 255U;
        h->motor_sim[i].valid = true;
        h->motor_sim[i].ready = true;
        h->motor_sim[i].rotor_valid = true;
        h->motor_sim[i].motor_temp_c = 25.0F;
        h->motor_sim[i].inverter_temp_c = 25.0F;
    }
    h->inputs.bms.valid = true;
    h->inputs.bms.contactor_closed = true;
    h->inputs.bms.interlock_ok = true;
    h->inputs.bms.pack_voltage_v = 360.0F;
    h->inputs.bms.charge_limit_a = 20.0F;
    h->inputs.bms.discharge_limit_a = 200.0F;
    h->inputs.bms.min_temp_c = 20.0F;
    h->inputs.bms.max_temp_c = 25.0F;
    h->inputs.inverter_ready = true;
    h->inputs.inverter_rotor_valid = true;
    h->inputs.charger_ready = true;
    h->inputs.insulation_ok = true;
    h->inputs.hvil_closed = true;
    h->inputs.coolant_ok = true;
    h->inputs.precharge_ok = true;
    add_event(h, "initialized in safe simulation mode");
}

void harness_reset(harness_t* h)
{
    harness_init(h);
}

void harness_set_fault(harness_t* h, const char* name, bool enabled)
{
    struct
    {
        const char* name;
        uint32_t bit;
    } map[] = {{"bms_timeout", INJECT_BMS_TIMEOUT},
               {"inverter_timeout", INJECT_INVERTER_TIMEOUT},
               {"charger_timeout", INJECT_CHARGER_TIMEOUT},
               {"bus_off", INJECT_BUS_OFF},
               {"bad_crc", INJECT_BAD_CRC},
               {"zero_limits", INJECT_ZERO_LIMITS},
               {"inverter_fault", INJECT_INVERTER_FAULT},
               {"inverter_desat", INJECT_INVERTER_DESAT},
               {"overcurrent", INJECT_OVERCURRENT},
               {"estop", INJECT_ESTOP},
               {"hvil_open", INJECT_HVIL_OPEN},
               {"contactor_welded", INJECT_CONTACTOR_WELDED},
               {"replay", INJECT_REPLAY},
               {"bus_overload", INJECT_BUS_OVERLOAD},
               {"conflicting_command", INJECT_CONFLICTING_COMMAND},
               {"mg1_timeout", INJECT_MG1_TIMEOUT},
               {"mg2_timeout", INJECT_MG2_TIMEOUT},
               {"mg1_fault", INJECT_MG1_FAULT},
               {"mg2_fault", INJECT_MG2_FAULT},
               {"mg1_resolver", INJECT_MG1_RESOLVER},
               {"mg2_resolver", INJECT_MG2_RESOLVER}};
    for (size_t i = 0U; i < sizeof(map) / sizeof(map[0]); ++i)
    {
        if (strcmp(name, map[i].name) == 0)
        {
            if (enabled)
                h->active_injected_faults |= map[i].bit;
            else
                h->active_injected_faults &= ~map[i].bit;
            add_event(h, enabled ? "fault injection enabled" : "fault injection disabled");
            return;
        }
    }
}

void harness_clear_faults(harness_t* h)
{
    h->active_injected_faults = 0U;
    can_bus_set_faults(&h->bus, (can_faults_t){0});
}

void harness_step(harness_t* h, uint32_t dt_ms)
{
    if (h == NULL || dt_ms == 0U || dt_ms > 1000U)
        return;
    h->time_ms += dt_ms;
    h->inputs.emergency_stop = injected(h, INJECT_ESTOP);
    h->inputs.hvil_closed = !injected(h, INJECT_HVIL_OPEN);
    h->inputs.motor_current_a =
        injected(h, INJECT_OVERCURRENT) ? 600.0F : h->inputs.motor_current_a;
    can_faults_t cf = h->bus.faults;
    cf.bus_off = injected(h, INJECT_BUS_OFF);
    cf.corrupt = injected(h, INJECT_BAD_CRC);
    cf.duplicate = injected(h, INJECT_REPLAY);
    cf.extra_latency_ms = injected(h, INJECT_BUS_OVERLOAD) ? 300U : 0U;
    can_bus_set_faults(&h->bus, cf);
    emit_bms(h);
    emit_inverter(h);
    emit_charger(h);
    emit_motor(h, 0U);
    emit_motor(h, 1U);
    can_bus_tick(&h->bus, h->time_ms);
    bool bms_ok = false, inverter_ok = false, charger_ok = false, bad = false;
    consume_frames(h, dt_ms, &bms_ok, &inverter_ok, &charger_ok, &bad);
    safety_inputs_t in = h->inputs;
    in.inverter_fault = injected(h, INJECT_INVERTER_FAULT);
    in.inverter_desat = injected(h, INJECT_INVERTER_DESAT);
    in.charger_fault = injected(h, INJECT_CHARGER_TIMEOUT) ? false : h->inputs.charger_fault;
    in.contactor_feedback_closed = injected(h, INJECT_CONTACTOR_WELDED);
    if (injected(h, INJECT_CONFLICTING_COMMAND))
    {
        in.drive_request = true;
        in.charge_request = true;
    }
    in.bms.valid = bms_ok && !injected(h, INJECT_BMS_TIMEOUT);
    safety_supervisor_evaluate(&in,
                               &h->safety,
                               h->bus.bus_off,
                               h->bms_age_ms > SIM_STALE_MS,
                               h->inverter_age_ms > SIM_STALE_MS,
                               h->charger_age_ms > SIM_STALE_MS,
                               bad,
                               false);
    dual_inputs_t di = {0};
    di.arm = in.arm;
    di.drive_request = in.drive_request;
    di.permission = !h->safety.latched && h->safety.severity < SAFETY_COMMUNICATION &&
                    in.bms.valid && in.precharge_ok && in.coolant_ok && in.insulation_ok &&
                    in.hvil_closed && !in.emergency_stop && in.bms.interlock_ok &&
                    in.bms.contactor_closed && !(in.brake && in.throttle_a_percent > 20.0F);
    di.bus_off = h->bus.bus_off;
    di.mode = h->dual_mode;
    di.request_percent = in.brake ? -20.0F : in.throttle_a_percent;
    di.discharge_limit_a = in.bms.discharge_limit_a;
    di.charge_limit_a = in.bms.charge_limit_a;
    di.pack_current_a = in.pack_current_a;
    di.motor[0] = h->motor_rx[0];
    di.motor[1] = h->motor_rx[1];
    dual_drive_evaluate(&h->dual_config, &di, h->time_ms, &h->dual);
    h->safety.torque_authorized =
        di.arm && di.drive_request && di.permission && h->dual.enabled && !in.charge_request;
    if (h->safety.torque_authorized)
        h->safety.torque_percent =
            fabsf(h->dual.command_percent[0]) > fabsf(h->dual.command_percent[1])
                ? h->dual.command_percent[0]
                : h->dual.command_percent[1];
    else
        h->safety.torque_percent = 0.0F;
    if (h->dual.latched)
    {
        h->safety.contactor_command = false;
        h->safety.latched = true;
        h->safety.severity = SAFETY_CRITICAL;
        h->safety.faults |= FAULT_INVERTER;
        (void)snprintf(h->safety.fault_text,
                       sizeof(h->safety.fault_text),
                       "dual-motor inhibit; inspect dual fault bits");
    }
    h->applied_torque_percent = h->safety.torque_percent;
    h->applied_charge_current_a = h->safety.charge_current_command_a;
    if (h->safety.faults != FAULT_NONE)
        add_event(h, h->safety.fault_text);
}

bool harness_run_scenario(harness_t* h, const char* name)
{
    harness_reset(h);
    if (strcmp(name, "nominal_drive") == 0)
    {
        h->inputs.arm = true;
        h->inputs.drive_request = true;
        h->inputs.throttle_a_percent = 30.0F;
        h->inputs.throttle_b_percent = 30.0F;
    }
    else if (strcmp(name, "bms_timeout") == 0)
    {
        h->inputs.arm = true;
        h->inputs.drive_request = true;
        h->inputs.throttle_a_percent = 30.0F;
        h->inputs.throttle_b_percent = 30.0F;
        harness_set_fault(h, "bms_timeout", true);
    }
    else if (strcmp(name, "charger_limit") == 0)
    {
        h->inputs.arm = true;
        h->inputs.charge_request = true;
        h->inputs.ac_present = true;
        h->inputs.bms.charge_limit_a = 7.0F;
    }
    else if (strcmp(name, "mg1_only") == 0 || strcmp(name, "combined_drive") == 0 ||
             strcmp(name, "combined_regen") == 0 || strcmp(name, "mg1_loss") == 0)
    {
        h->dual_mode = strcmp(name, "mg1_only") == 0 ? DUAL_MG1_ONLY : DUAL_COMBINED;
        h->inputs.arm = true;
        h->inputs.drive_request = true;
        h->inputs.throttle_a_percent = 30.0F;
        h->inputs.throttle_b_percent = 30.0F;
        if (strcmp(name, "combined_regen") == 0)
        {
            h->inputs.brake = true;
            h->inputs.throttle_a_percent = 0.0F;
            h->inputs.throttle_b_percent = 0.0F;
        }
        if (strcmp(name, "mg1_loss") == 0)
            harness_set_fault(h, "mg1_timeout", true);
    }
    else
        return false;
    for (uint32_t i = 0U; i < 40U; ++i)
        harness_step(h, 10U);
    return true;
}

const char* harness_fault_name(uint32_t fault)
{
    return safety_fault_name(fault);
}

size_t harness_state_json(const harness_t* h, char* out, size_t out_size)
{
    if (h == NULL || out == NULL || out_size == 0U)
        return 0U;
    int n = snprintf(
        out,
        out_size,
        "{\"time_ms\":%llu,\"torque_authorized\":%s,\"torque_percent\":%.2f,\"charger_authorized\":%s,\"charge_current_a\":%.2f,\"latched\":%s,\"faults\":%u,\"fault_text\":\"%s\",\"bms_age_ms\":%u,\"inverter_age_ms\":%u,\"charger_age_ms\":%u,\"bus_off\":%s,\"bus_load_percent\":%u,\"bms_pack_voltage_v\":%.1f,\"bms_charge_limit_a\":%.1f,\"bms_discharge_limit_a\":%.1f}",
        (unsigned long long)h->time_ms,
        h->safety.torque_authorized ? "true" : "false",
        h->safety.torque_percent,
        h->safety.charger_authorized ? "true" : "false",
        h->safety.charge_current_command_a,
        h->safety.latched ? "true" : "false",
        h->safety.faults,
        h->safety.fault_text,
        h->bms_age_ms,
        h->inverter_age_ms,
        h->charger_age_ms,
        h->bus.bus_off ? "true" : "false",
        h->bus.nominal_load_percent,
        h->inputs.bms.pack_voltage_v,
        h->inputs.bms.charge_limit_a,
        h->inputs.bms.discharge_limit_a);
    if (n < 1 || (size_t)n >= out_size)
        return 0U;
    const size_t used = (size_t)n - 1U;
    const int extra = snprintf(
        out + used,
        out_size - used,
        ",\"dual\":{\"mode\":%u,\"enabled\":%s,\"latched\":%s,\"faults\":%u,\"latched_faults\":%u,"
        "\"mg1_command_percent\":%.2f,\"mg2_command_percent\":%.2f,\"mg1_allocated_pack_a\":%.2f,\"mg2_allocated_pack_a\":%.2f,"
        "\"mg1_age_ms\":%llu,\"mg2_age_ms\":%llu}}",
        (unsigned)h->dual_mode,
        h->dual.enabled ? "true" : "false",
        h->dual.latched ? "true" : "false",
        h->dual.faults,
        h->dual.latched_faults,
        h->dual.command_percent[0],
        h->dual.command_percent[1],
        h->dual.allocated_pack_a[0],
        h->dual.allocated_pack_a[1],
        (unsigned long long)(h->time_ms - h->motor_rx[0].received_ms),
        (unsigned long long)(h->time_ms - h->motor_rx[1].received_ms));
    return extra < 0 || (size_t)extra >= out_size - used ? 0U : used + (size_t)extra;
}

size_t harness_events_json(const harness_t* h, char* out, size_t out_size)
{
    if (h == NULL || out == NULL || out_size == 0U)
        return 0U;
    size_t used = 0U;
    int n = snprintf(out, out_size, "[");
    if (n < 0)
        return 0U;
    used = (size_t)n;
    for (size_t i = 0U; i < h->event_count && used + 160U < out_size; ++i)
    {
        n = snprintf(out + used,
                     out_size - used,
                     "%s{\"time_ms\":%llu,\"faults\":%u,\"text\":\"%s\"}",
                     i == 0U ? "" : ",",
                     (unsigned long long)h->events[i].time_ms,
                     h->events[i].faults,
                     h->events[i].text);
        if (n < 0)
            return 0U;
        used += (size_t)n;
    }
    if (used + 2U > out_size)
        return 0U;
    out[used++] = ']';
    out[used] = '\0';
    return used;
}

size_t harness_can_trace_json(const harness_t* h, char* out, size_t out_size)
{
    if (h == NULL || out == NULL || out_size == 0U)
        return 0U;
    size_t used = 0U;
    int n = snprintf(out, out_size, "[");
    if (n < 0)
        return 0U;
    used = (size_t)n;
    size_t start = h->bus.trace_count > 32U ? h->bus.trace_count - 32U : 0U;
    for (size_t i = start; i < h->bus.trace_count && used + 160U < out_size; ++i)
    {
        const can_frame_t* f = &h->bus.trace[i];
        n = snprintf(out + used,
                     out_size - used,
                     "%s{\"t_ms\":%llu,\"id\":%u,\"source\":%u,\"seq\":%u,\"dlc\":%u}",
                     i == start ? "" : ",",
                     (unsigned long long)f->timestamp_ms,
                     f->id,
                     (unsigned)f->source,
                     (unsigned)f->sequence,
                     (unsigned)f->dlc);
        if (n < 0)
            return 0U;
        used += (size_t)n;
    }
    if (used + 2U > out_size)
        return 0U;
    out[used++] = ']';
    out[used] = '\0';
    return used;
}

size_t harness_scenarios_json(char* out, size_t out_size)
{
    const char* s =
        "[\"nominal_drive\",\"bms_timeout\",\"charger_limit\",\"mg1_only\",\"combined_drive\",\"combined_regen\",\"mg1_loss\"]";
    size_t n = strlen(s);
    if (n + 1U > out_size)
        return 0U;
    memcpy(out, s, n + 1U);
    return n;
}
