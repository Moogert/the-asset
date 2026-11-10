// Compile-time release gates. False/zero defaults prevent an unidentified ECU from receiving vehicle signals.
#ifndef CIVIC_VEHICLE_PROFILE_H
#define CIVIC_VEHICLE_PROFILE_H
#include "engine.h"
#include "indication.h"
#include "emulator_io.h"
// THIS IS AN UNVERIFIED PROFILE. No Honda connector cavities are assigned.
// Approval requires the worksheet, measured electrical interfaces and matching ECU evidence.
namespace Vehicleprofile
{
constexpr bool honda_verified = false;
constexpr bool engine_model_verified = false;
constexpr bool ev_telemetry_verified = false;
constexpr bool lamp_hardware_verified = false;
constexpr bool service_inputs_verified = false;
constexpr bool output_interfaces_verified = false;
inline Civic::Outputprofile output_profile()
{
    Civic::Outputprofile p = Civic::synthetic_output_profile();
    if (honda_verified)
    {
        p.synthetic = false;
        p.verified = output_interfaces_verified;
        // Enter measured voltage limits, reference and ladder ohms before release.
    }
    return p;
}
constexpr unsigned long ev_can_bitrate = 500000; // project example, not a vehicle setting
constexpr unsigned long vss_pulses_per_km = 0;   // unknown, disabled
constexpr bool honda_requires_l_init = false;    // UNKNOWN; exact profile must resolve this
constexpr unsigned long honda_uart_baud = 10400; // ISO 9141 template only
inline Civic::Indicationprofile indication_profile()
{
    Civic::Indicationprofile p = Civic::demonstration_indication_profile();
    p.production = honda_verified;
    const Civic::Outputprofile outputs = output_profile();
    for (unsigned i = 0; i < Civic::category_count; ++i)
    {
        const unsigned sensor = p.mapping[i].sensor;
        p.mapping[i].available = sensor == Civic::ect_channel || sensor == Civic::iat_channel
                                     ? outputs.resistance[sensor - Civic::ect_channel].fitted
                                     : (sensor < 8 && outputs.voltage[sensor].fitted);
    }
    return p; // Every mapping remains unverified until actual ECU test evidence is entered.
}
// Replace only from a verified factory pattern and measured conditioning design.
// Zero count deliberately makes the production profile invalid.
constexpr Civic::Edge production_edges[] = {{0, 0}};
constexpr uint8_t production_edge_count = 0;
constexpr uint8_t production_channel_mask = 0;
constexpr uint16_t production_max_rpm = 0;
inline Civic::Engineprofile engine_profile()
{
    if (!honda_verified)
        return Civic::synthetic_profile();
    Civic::Engineprofile p = {};
    p.verified = honda_verified && engine_model_verified;
    p.count = production_edge_count;
    p.channel_mask = production_channel_mask;
    p.max_rpm = production_max_rpm;
    if (p.count <= sizeof(production_edges) / sizeof(production_edges[0]) && p.count <= 128)
        for (unsigned i = 0; i < p.count; ++i)
            p.edges[i] = production_edges[i];
    else
        p.count = 0;
    return p;
}
} // namespace Vehicleprofile
#endif
