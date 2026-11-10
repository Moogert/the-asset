// Read-only adapter from current supervisor outputs to diagnostic context; never clears or rearms control.
#ifndef CAPSTONE_SUPERVISOR_DIAGNOSTIC_BRIDGE_H
#define CAPSTONE_SUPERVISOR_DIAGNOSTIC_BRIDGE_H
#include "diagnostics.h"
#include <control/control_runtime.h>
#include <control/release_control.h>
#include "detail.h"
// Optional acquisition-side bridge. No CAN transmitter and no changes to traction decisions.
namespace Civic
{
// Fixed-point diagnostic snapshot; invalid/out-of-range values never become plausible readings.
inline Faultdetail diagnostic_detail(const Capstone::Faultrecord& record)
{
    Faultdetail d = {};
    d.identity = record.identity;
    d.component = static_cast<uint8_t>(record.component);
    d.severity = static_cast<uint8_t>(record.severity);
    d.active = record.active;
    d.source_ms = static_cast<uint32_t>(record.latest_ms);
    d.valid = record.valid && record.measured >= -1000000.0F && record.measured <= 1000000.0F &&
              record.threshold >= -1000000.0F && record.threshold <= 1000000.0F;
    if (d.valid)
    {
        d.measured_milli = static_cast<int32_t>(record.measured * 1000.0F);
        d.threshold_milli = static_cast<int32_t>(record.threshold * 1000.0F);
    }
    return d;
}
inline Telemetry diagnostic_snapshot(uint32_t legacy_faults,
                                     const Capstone::Cycle& cycle,
                                     const Context& measured,
                                     bool parked,
                                     bool armed,
                                     bool mg1_reported_fault,
                                     bool mg2_reported_fault)
{
    Telemetry t = {};
    t.legacy = legacy_faults;
    t.runtime = cycle.faults;
    t.context = measured;
    t.flags = static_cast<uint8_t>((parked ? 1 : 0) | (armed ? 2 : 0));
    t.channel_faults =
        static_cast<uint8_t>((mg1_reported_fault ? 1 : 0) | (mg2_reported_fault ? 2 : 0));
    return t;
}
} // namespace Civic
#endif
