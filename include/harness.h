#ifndef HARNESS_H
#define HARNESS_H

#include "can_bus_model.h"
#include "safety_supervisor.h"
#include "dual_drive.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HARNESS_EVENT_MAX 128U

typedef struct
{
    uint64_t time_ms;
    char text[128];
    uint32_t faults;
} harness_event_t;

typedef struct
{
    uint64_t time_ms;
    safety_inputs_t inputs;
    safety_outputs_t safety;
    can_bus_t bus;
    uint32_t active_injected_faults;
    uint32_t bms_age_ms;
    uint32_t inverter_age_ms;
    uint32_t charger_age_ms;
    uint8_t bms_sequence;
    uint8_t bms_rx_expected;
    uint8_t inverter_sequence;
    uint8_t charger_sequence;
    float applied_torque_percent;
    float applied_charge_current_a;
    dual_config_t dual_config;
    dual_outputs_t dual;
    dual_feedback_t motor_sim[2];
    dual_feedback_t motor_rx[2];
    dual_mode_t dual_mode;
    uint8_t motor_sequence[2], motor_expected[2];
    harness_event_t events[HARNESS_EVENT_MAX];
    size_t event_count;
} harness_t;

enum
{
    INJECT_BMS_TIMEOUT = 1U << 0,
    INJECT_INVERTER_TIMEOUT = 1U << 1,
    INJECT_CHARGER_TIMEOUT = 1U << 2,
    INJECT_BUS_OFF = 1U << 3,
    INJECT_BAD_CRC = 1U << 4,
    INJECT_ZERO_LIMITS = 1U << 5,
    INJECT_INVERTER_FAULT = 1U << 6,
    INJECT_INVERTER_DESAT = 1U << 7,
    INJECT_OVERCURRENT = 1U << 8,
    INJECT_ESTOP = 1U << 9,
    INJECT_HVIL_OPEN = 1U << 10,
    INJECT_CONTACTOR_WELDED = 1U << 11,
    INJECT_REPLAY = 1U << 12,
    INJECT_BUS_OVERLOAD = 1U << 13,
    INJECT_CONFLICTING_COMMAND = 1U << 14,
    INJECT_MG1_TIMEOUT = 1U << 15,
    INJECT_MG2_TIMEOUT = 1U << 16,
    INJECT_MG1_FAULT = 1U << 17,
    INJECT_MG2_FAULT = 1U << 18,
    INJECT_MG1_RESOLVER = 1U << 19,
    INJECT_MG2_RESOLVER = 1U << 20
};

void harness_init(harness_t* h);
void harness_reset(harness_t* h);
void harness_step(harness_t* h, uint32_t dt_ms);
void harness_set_fault(harness_t* h, const char* name, bool enabled);
void harness_clear_faults(harness_t* h);
bool harness_run_scenario(harness_t* h, const char* name);
const char* harness_fault_name(uint32_t fault);
size_t harness_state_json(const harness_t* h, char* out, size_t out_size);
size_t harness_events_json(const harness_t* h, char* out, size_t out_size);
size_t harness_can_trace_json(const harness_t* h, char* out, size_t out_size);
size_t harness_scenarios_json(char* out, size_t out_size);

#endif
