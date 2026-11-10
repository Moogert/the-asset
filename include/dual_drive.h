#ifndef DUAL_DRIVE_H
#define DUAL_DRIVE_H

#include <stdbool.h>
#include <stdint.h>

/* Portable simulation/prototype policy. No hardware outputs or calibrated motor model. */
typedef enum
{
    DUAL_MG2_ONLY = 0,
    DUAL_MG1_ONLY = 1,
    DUAL_COMBINED = 2
} dual_mode_t;
enum
{
    DUAL_CONFIG = 1U << 0,
    DUAL_STALE = 1U << 1,
    DUAL_FEEDBACK = 1U << 2,
    DUAL_CHANNEL = 1U << 3,
    DUAL_SPEED = 1U << 4,
    DUAL_THERMAL = 1U << 5,
    DUAL_PACK = 1U << 6,
    DUAL_CIRCULATION = 1U << 7,
    DUAL_PERMISSION = 1U << 8,
    DUAL_SCHEDULER = 1U << 9,
    DUAL_KINEMATICS = 1U << 10
};
typedef struct
{
    bool valid, ready, rotor_valid, fault;
    uint64_t received_ms;
    float rpm, motor_temp_c, inverter_temp_c, dc_current_a;
} dual_feedback_t;
typedef struct
{
    uint32_t stale_ms;
    float max_rpm, derate_c, stop_c;
    float pack_current_tolerance_a, circulation_threshold_a;
    /* Conservative SIMULATION full-command DC envelopes, not phase-current limits. */
    float draw_a[2], regen_a[2];
} dual_config_t;
typedef struct
{
    bool arm, drive_request, permission, bus_off, reset_request;
    dual_mode_t mode;
    float request_percent; /* positive = motoring; negative = regeneration */
    float discharge_limit_a, charge_limit_a, pack_current_a;
    dual_feedback_t motor[2]; /* [0] MG1, [1] MG2 */
} dual_inputs_t;
typedef struct
{
    bool enabled, latched;
    uint32_t faults, latched_faults;
    float command_percent[2], allocated_pack_a[2];
    uint64_t last_step_ms;
    bool has_step;
    dual_mode_t previous_mode;
} dual_outputs_t;

dual_config_t dual_simulation_config(void);
void dual_drive_evaluate(const dual_config_t* cfg,
                         const dual_inputs_t* in,
                         uint64_t now_ms,
                         dual_outputs_t* out);
#endif
