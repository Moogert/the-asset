#ifndef CIVIC_EMULATOR_IO_H
#define CIVIC_EMULATOR_IO_H
#include "engine.h"
namespace Civic
{
enum Sensorchannel
{
    tps_channel,
    map_channel,
    oxygen_upstream,
    oxygen_downstream,
    ect_channel,
    iat_channel,
    spare_6,
    spare_7
};
struct Voltageprofile
{
    bool fitted;
    uint16_t normal_min_mv, normal_max_mv, low_fault_mv, high_fault_mv;
};
struct Resistanceprofile
{
    bool fitted;
    uint8_t low_fault_index, high_fault_index;
    uint32_t
        ohms[8]; // Production requires actual measured ladder values, not selector assumptions.
};
struct Outputprofile
{
    bool synthetic, verified;
    uint16_t dac_reference_mv;
    Voltageprofile voltage[8];
    Resistanceprofile resistance[2];
};
struct Outputplan
{
    bool enabled, fault;
    uint16_t dac_code[8];
    bool connect[8];
    uint8_t resistance_index[2];
};
Outputprofile synthetic_output_profile();
bool valid_output_profile(const Outputprofile& profile);
Outputplan plan_outputs(const Sensors& sensors, const Outputprofile& profile, bool bench);
} // namespace Civic
#endif
