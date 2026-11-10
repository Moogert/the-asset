#ifndef CAPSTONE_DUAL_SUPERVISOR_H
#define CAPSTONE_DUAL_SUPERVISOR_H

#include <stdint.h>
#include <limits.h>

// Fixed-size C++03 supervisor interface. Units: ms, rpm, C, A, percent.
// Platform integer widths and floating-point behavior require target review.
namespace Capstone
{
typedef float Real;
typedef uint64_t Timestamp;
typedef char Bytewidth[(CHAR_BIT == 8) ? 1 : -1];
typedef char Wordwidth[(sizeof(uint32_t) == 4U) ? 1 : -1];
typedef char Timewidth[(sizeof(Timestamp) == 8U) ? 1 : -1];
typedef char Realwidth[(sizeof(Real) == 4U) ? 1 : -1];

enum Mode
{
    mg2_only = 0,
    mg1_only = 1,
    combined = 2
};

const uint32_t config_fault = 1U << 0U;
const uint32_t stale_fault = 1U << 1U;
const uint32_t feedback_fault = 1U << 2U;
const uint32_t channel_fault = 1U << 3U;
const uint32_t speed_fault = 1U << 4U;
const uint32_t thermal_fault = 1U << 5U;
const uint32_t pack_fault = 1U << 6U;
const uint32_t circulation_fault = 1U << 7U;
const uint32_t permission_fault = 1U << 8U;
const uint32_t scheduler_fault = 1U << 9U;
const uint32_t kinematics_fault = 1U << 10U;

struct Motorfeedback
{
    bool valid;
    bool ready;
    bool rotor_valid;
    bool fault;
    Timestamp received_ms;
    Real rpm;
    Real motor_temp_c;
    Real inverter_temp_c;
    Real dc_current_a;
};

struct Config
{
    uint32_t stale_ms;
    Real max_rpm;
    Real derate_c;
    Real stop_c;
    Real pack_current_tolerance_a;
    Real circulation_threshold_a;
    Real draw_a[2];
    Real regen_a[2];
    bool revised_monitoring; // Only the release layer supplies validated component factors.
};

struct Inputs
{
    bool arm;
    bool drive_request;
    bool permission;
    bool bus_off;
    bool reset_request;
    Mode mode;
    Real request_percent;
    Real discharge_limit_a;
    Real charge_limit_a;
    Real pack_current_a;
    Motorfeedback motor[2];
    Real effort_factor[2]; // Used only with revised_monitoring; otherwise legacy behavior.
};

// Value-initialize once before the first evaluate call. Caller owns all state.
struct State
{
    bool latched;
    uint32_t latched_faults;
    bool has_step;
    Timestamp last_step_ms;
    Mode previous_mode;
};

struct Outputs
{
    bool enabled;
    uint32_t faults;
    Real command_percent[2];
    Real allocated_pack_a[2];
};

Config simulationconfig();
Outputs evaluate(const Config& cfg, const Inputs& input, Timestamp now_ms, State& state);
} // namespace Capstone

#endif
