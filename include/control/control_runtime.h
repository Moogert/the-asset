#ifndef CAPSTONE_CONTROL_RUNTIME_H
#define CAPSTONE_CONTROL_RUNTIME_H

#include <control/dual_supervisor.h>

namespace Capstone
{
const uint32_t runtime_config_fault = 1U << 16U;
const uint32_t runtime_timing_fault = 1U << 17U;
const uint32_t bms_fault = 1U << 18U;
const uint32_t transport_fault = 1U << 19U;
const uint32_t mapping_fault = 1U << 20U;
const uint32_t inhibit_fault = 1U << 21U;

struct Frame
{
    uint16_t identifier;
    uint8_t data[8];
};

struct Rawcommand
{
    uint16_t throttle_a;
    uint16_t throttle_b;
    uint8_t io_bits;
    uint8_t regen_percent;
};

bool encode(const Rawcommand& command, uint8_t counter, Frame& frame);

// No concrete hardware implementation is supplied. All calls must be bounded.
// send=true means accepted locally, not delivery/acceptance by the inverter.
// setinhibit=true acknowledges the driver request, not measured physical gating.
class Hardwareport
{
  public:
    virtual ~Hardwareport()
    {
    }
    virtual bool send(const Frame& frame) = 0;
    virtual bool setinhibit(bool asserted) = 0;
    virtual void kickwatchdog() = 0;
    // Must map calibrated effort, including zero, independently per channel.
    // Runtime owns counter/CRC. No default linear effort-to-digit mapping exists.
    virtual bool mapcommand(unsigned channel, Real effort_percent, Rawcommand& raw) = 0;
};

struct Runtimeconfig
{
    Config policy;
    uint32_t period_ms;
    uint32_t deadline_ms;
    uint32_t bms_stale_ms;
    uint32_t inhibit_budget_ms;
    uint16_t control_id[2];
    Real rise_percent_per_second;
};

struct Snapshot
{
    Inputs policy;
    bool bms_valid;
    Timestamp bms_received_ms;
    bool transport_healthy;
    bool watchdog_healthy;
    bool energy_ready;
};

struct Runtimestate
{
    State policy;
    bool initialized;
    bool disarmed_seen;
    bool inhibit_requested;
    uint32_t last_clock_ms;
    Timestamp monotonic_ms;
    uint32_t latched_faults;
    uint8_t counter[2];
    Real previous_command[2];
};

struct Cycle
{
    Outputs policy;
    bool due;
    bool dispatched;
    bool inhibit_requested;
    bool watchdog_kicked;
    uint32_t faults;
};

// Call initialize before acquisition/scheduling. Feedback timestamps use
// state.monotonic_ms plus unsigned elapsed time from last_clock_ms.
bool initialize(uint32_t clock_ms, Runtimestate& state, Hardwareport& port);
Cycle run(const Runtimeconfig& cfg,
          const Snapshot& snapshot,
          uint32_t clock_ms,
          Runtimestate& state,
          Hardwareport& port);
} // namespace Capstone

#endif
