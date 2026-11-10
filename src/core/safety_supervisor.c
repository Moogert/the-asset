// Host safety-policy simulator. Fixed thresholds here are demonstration values, not hardware ratings.
#include "safety_supervisor.h"
#include <stdio.h>
#include <string.h>

static void
add_fault(safety_outputs_t* out, uint32_t bit, safety_severity_t severity, const char* text)
{
    out->faults |= bit;
    if (severity > out->severity)
        out->severity = severity;
    if (out->fault_text[0] == '\0' || severity >= SAFETY_CRITICAL)
    {
        (void)snprintf(out->fault_text, sizeof(out->fault_text), "%s", text);
    }
}

static bool finite_range(float value, float low, float high)
{
    return value == value && value >= low && value <= high;
}

void safety_supervisor_evaluate(const safety_inputs_t* in,
                                safety_outputs_t* out,
                                bool bus_off,
                                bool bms_stale,
                                bool inverter_stale,
                                bool charger_stale,
                                bool bad_frame,
                                bool reset_request)
{
    if (in == NULL || out == NULL)
        return;
    const bool was_latched = out->latched;
    memset(out, 0, sizeof(*out));
    out->severity = SAFETY_OK;
    out->fault_text[0] = '\0';
    if (was_latched && !reset_request)
        out->latched = true;

    if (bus_off)
        add_fault(out, FAULT_BUS_OFF, SAFETY_COMMUNICATION, "CAN bus-off");
    if (bad_frame)
        add_fault(out, FAULT_BAD_FRAME, SAFETY_COMMUNICATION, "invalid CAN frame");
    if (bms_stale || !in->bms.valid)
        add_fault(out, FAULT_BMS_STALE, SAFETY_COMMUNICATION, "BMS data stale");
    if (inverter_stale)
        add_fault(out, FAULT_INVERTER_STALE, SAFETY_COMMUNICATION, "inverter heartbeat stale");
    if (charger_stale)
        add_fault(out, FAULT_CHARGER_STALE, SAFETY_COMMUNICATION, "charger heartbeat stale");

    if (in->emergency_stop)
        add_fault(out, FAULT_EMERGENCY_STOP, SAFETY_CRITICAL, "emergency stop");
    if (!in->hvil_closed || !in->insulation_ok || !in->coolant_ok)
        add_fault(out, FAULT_HVIL, SAFETY_CRITICAL, "HV safety chain open");
    if (!finite_range(in->throttle_a_percent, 0.0F, 100.0F) ||
        !finite_range(in->throttle_b_percent, 0.0F, 100.0F) ||
        (in->throttle_a_percent - in->throttle_b_percent > 5.0F) ||
        (in->throttle_b_percent - in->throttle_a_percent > 5.0F))
        add_fault(out, FAULT_THROTTLE, SAFETY_CRITICAL, "throttle channels disagree");
    if (in->inverter_fault || in->inverter_desat || !in->inverter_rotor_valid)
        add_fault(out, FAULT_INVERTER, SAFETY_CRITICAL, "inverter fault");
    if (in->charger_fault)
        add_fault(out, FAULT_CHARGER, SAFETY_CRITICAL, "charger fault");
    if (in->drive_request && in->charge_request)
        add_fault(out, FAULT_CONTRADICTORY, SAFETY_CRITICAL, "drive and charge requested");
    if (in->motor_current_a > 500.0F || in->pack_current_a > 500.0F)
        add_fault(out, FAULT_OVERCURRENT, SAFETY_CRITICAL, "overcurrent");
    if (in->speed_rpm > 12000.0F)
        add_fault(out, FAULT_OVERSPEED, SAFETY_CRITICAL, "overspeed");
    if ((in->drive_request || in->charge_request) &&
        (!in->bms.interlock_ok || !in->bms.contactor_closed))
        add_fault(out, FAULT_CONTACTOR, SAFETY_CRITICAL, "BMS contactor/interlock not ready");
    if (in->contactor_feedback_closed && in->pack_voltage_v < 10.0F)
        add_fault(out, FAULT_CONTACTOR, SAFETY_CRITICAL, "contactor feedback inconsistent");
    if (in->contactor_feedback_closed && !in->drive_request && !in->charge_request)
        add_fault(out, FAULT_CONTACTOR, SAFETY_CRITICAL, "contactor appears welded");
    if (!finite_range(in->bms.charge_limit_a, 0.0F, 500.0F) ||
        !finite_range(in->bms.discharge_limit_a, 0.0F, 500.0F))
        add_fault(out, FAULT_BMS_LIMIT, SAFETY_CRITICAL, "invalid BMS limits");
    if (in->brake && in->throttle_a_percent > 20.0F)
        add_fault(out, FAULT_CONTRADICTORY, SAFETY_DERATE, "brake and throttle conflict");

    if (out->latched && reset_request && out->faults == FAULT_NONE)
        out->latched = false;
    if (out->severity >= SAFETY_CRITICAL)
        out->latched = true;

    const bool comms_ok = !bms_stale && !inverter_stale && !bus_off && !bad_frame && in->bms.valid;
    const bool hardware_ok = in->arm && in->hvil_closed && in->insulation_ok && in->precharge_ok &&
                             in->coolant_ok && !in->emergency_stop;
    if (!out->latched && out->severity < SAFETY_CRITICAL && comms_ok && hardware_ok)
    {
        out->contactor_command = in->drive_request || in->charge_request;
        if (in->drive_request && in->inverter_ready && in->bms.discharge_limit_a > 0.0F)
        {
            float limit = in->bms.discharge_limit_a / 5.0F;
            if (limit > 100.0F)
                limit = 100.0F;
            out->torque_percent = in->throttle_a_percent < limit ? in->throttle_a_percent : limit;
            out->torque_authorized = true;
        }
        if (in->charge_request && in->charger_ready && in->ac_present &&
            in->bms.charge_limit_a > 0.0F)
        {
            out->charge_current_command_a = in->bms.charge_limit_a;
            out->charger_authorized = true;
        }
    }
    if (!out->torque_authorized)
        out->torque_percent = 0.0F;
    if (!out->charger_authorized)
        out->charge_current_command_a = 0.0F;
}

const char* safety_fault_name(uint32_t fault)
{
    switch (fault)
    {
    case FAULT_BMS_STALE:
        return "BMS_STALE";
    case FAULT_INVERTER_STALE:
        return "INVERTER_STALE";
    case FAULT_CHARGER_STALE:
        return "CHARGER_STALE";
    case FAULT_BUS_OFF:
        return "BUS_OFF";
    case FAULT_BAD_FRAME:
        return "BAD_FRAME";
    case FAULT_BMS_LIMIT:
        return "BMS_LIMIT";
    case FAULT_INVERTER:
        return "INVERTER";
    case FAULT_OVERCURRENT:
        return "OVERCURRENT";
    case FAULT_OVERSPEED:
        return "OVERSPEED";
    case FAULT_HVIL:
        return "HVIL";
    case FAULT_EMERGENCY_STOP:
        return "EMERGENCY_STOP";
    case FAULT_CONTACTOR:
        return "CONTACTOR";
    case FAULT_THROTTLE:
        return "THROTTLE";
    case FAULT_CHARGER:
        return "CHARGER";
    case FAULT_CONTRADICTORY:
        return "CONTRADICTORY";
    case FAULT_CONFIG:
        return "CONFIG";
    default:
        return "UNKNOWN";
    }
}
