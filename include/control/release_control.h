#ifndef CAPSTONE_RELEASE_CONTROL_H
#define CAPSTONE_RELEASE_CONTROL_H
#include <control/control_runtime.h>

namespace Capstone
{
// Component indices are stable diagnostic identities, not physical connector numbers.
enum Component
{
    mg1_motor,
    mg2_motor,
    mg1_inverter,
    mg2_inverter,
    oil,
    coolant,
    shared,
    transport
};
enum Severity
{
    advisory,
    derating,
    shutdown
};
const unsigned temperature_count = 6U;
const unsigned record_capacity = 64U;
const uint32_t measurement_fault = 1U << 11U;
const uint32_t dispatch_fault = 1U << 22U;
struct Measurement
{
    Real value;
    Real uncertainty;
    Timestamp acquired_ms;
    bool valid;
    uint16_t sensor_id; // Acquisition-source identity; temperature profiles require an exact match.
};
struct Temperaturelimit
{
    bool required;
    enum Location
    {
        unspecified,
        winding,
        case_sensor,
        oil_sensor,
        coolant_sensor
    } location;
    uint16_t sensor_id;
    Real derate_c, stop_c, hysteresis_c;
    uint32_t stale_ms, filter_ms;
};
struct Releaseconfig
{
    Runtimeconfig runtime;
    Temperaturelimit temperature[temperature_count];
    bool circulation_required;
    Real opposing_w;
    uint32_t power_stale_ms, power_skew_ms, confirmation_ms;
    uint32_t command_age_ms, pair_skew_ms;
    bool production;
    bool hardware_verified;
};
struct Releaseinput
{
    Snapshot snapshot;
    Measurement temperature[temperature_count];
    // Independently measured signed DC power, including declared total uncertainty in watts.
    Measurement power[2];
};
struct Faultrecord
{
    uint32_t identity;
    Component component;
    Severity severity;
    bool active, valid;
    Timestamp first_ms, latest_ms;
    Real measured, threshold;
    uint32_t occurrences;
};
struct Monitorstate
{
    bool initialized[temperature_count], hot[temperature_count];
    Real filtered[temperature_count];
    Timestamp sampled[temperature_count], evaluated_ms, opposing_since;
    bool opposing;
    Faultrecord records[record_capacity];
    unsigned count;
    bool overflow;
};
struct Monitorresult
{
    uint32_t faults;
    Real factor[2];
    bool circulation_suspected, power_available;
    uint8_t cooling_percent;
};
Releaseconfig demonstration_release_config();
struct Faultvalue
{
    Real measured, threshold;
    bool valid;
};
Faultvalue faultvalue(Real measured, Real threshold, bool valid);
void recordfault(Monitorstate& state,
                 uint32_t identity,
                 Component component,
                 Severity severity,
                 bool active,
                 Timestamp now,
                 const Faultvalue& value);
Monitorresult
monitor(const Releaseconfig& cfg, const Releaseinput& input, Timestamp now, Monitorstate& state);

struct Transmission
{
    bool complete, failed;
    Timestamp completed_ms;
    // Observational only. No generic CAN acknowledgment proves controller application.
    bool controller_observed;
};
class Releaseport : public Hardwareport
{
  public:
    virtual bool physicaloutputs() const = 0;
    // Bind each send to a local generation; wire payload remains unchanged.
    virtual bool beginpair(uint32_t generation) = 0;
    virtual Transmission transmission(unsigned channel, uint32_t generation) = 0;
    // Must prevent obsolete hardware/software queued frames from later transmitting.
    virtual bool discardpending() = 0;
};
struct Releasestate
{
    Runtimestate runtime;
    Monitorstate monitor;
    uint32_t generation, latched_faults;
    bool pending, released;
    Timestamp queued_ms;
    Transmission last_transmission[2];
};
struct Releasecycle
{
    Cycle runtime;
    Monitorresult monitoring;
    uint32_t faults;
    bool pending, released;
};
bool initialize_release(uint32_t clock_ms, Releasestate& state, Releaseport& port);
Releasecycle run_release(const Releaseconfig& cfg,
                         const Releaseinput& input,
                         uint32_t clock_ms,
                         Releasestate& state,
                         Releaseport& port);
} // namespace Capstone
#endif
