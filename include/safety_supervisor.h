#ifndef SAFETY_SUPERVISOR_H
#define SAFETY_SUPERVISOR_H

#include "leaf_bms_adapter.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SAFETY_OK = 0,
    SAFETY_DERATE = 1,
    SAFETY_COMMUNICATION = 2,
    SAFETY_CRITICAL = 3
} safety_severity_t;

enum
{
    FAULT_NONE = 0U,
    FAULT_BMS_STALE = 1U << 0,
    FAULT_INVERTER_STALE = 1U << 1,
    FAULT_CHARGER_STALE = 1U << 2,
    FAULT_BUS_OFF = 1U << 3,
    FAULT_BAD_FRAME = 1U << 4,
    FAULT_BMS_LIMIT = 1U << 5,
    FAULT_INVERTER = 1U << 6,
    FAULT_OVERCURRENT = 1U << 7,
    FAULT_OVERSPEED = 1U << 8,
    FAULT_HVIL = 1U << 9,
    FAULT_EMERGENCY_STOP = 1U << 10,
    FAULT_CONTACTOR = 1U << 11,
    FAULT_THROTTLE = 1U << 12,
    FAULT_CHARGER = 1U << 13,
    FAULT_CONTRADICTORY = 1U << 14,
    FAULT_CONFIG = 1U << 15
};

typedef struct
{
    bool arm;
    bool brake;
    bool emergency_stop;
    bool hvil_closed;
    bool insulation_ok;
    bool precharge_ok;
    bool contactor_feedback_closed;
    bool drive_request;
    bool charge_request;
    bool ac_present;
    bool coolant_ok;
    float throttle_a_percent;
    float throttle_b_percent;
    float speed_rpm;
    float pack_voltage_v;
    float pack_current_a;
    float motor_current_a;
    float motor_temp_c;
    float inverter_temp_c;
    bool inverter_ready;
    bool inverter_fault;
    bool inverter_desat;
    bool inverter_rotor_valid;
    bool charger_ready;
    bool charger_fault;
    leaf_bms_status_t bms;
} safety_inputs_t;

typedef struct
{
    bool torque_authorized;
    bool charger_authorized;
    bool contactor_command;
    float torque_percent;
    float charge_current_command_a;
    uint32_t faults;
    safety_severity_t severity;
    bool latched;
    char fault_text[128];
} safety_outputs_t;

void safety_supervisor_evaluate(const safety_inputs_t* in,
                                safety_outputs_t* out,
                                bool bus_off,
                                bool bms_stale,
                                bool inverter_stale,
                                bool charger_stale,
                                bool bad_frame,
                                bool reset_request);

const char* safety_fault_name(uint32_t fault);

#endif
