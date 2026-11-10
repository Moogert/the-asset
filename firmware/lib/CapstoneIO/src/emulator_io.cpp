#include "emulator_io.h"
namespace Civic
{
Outputprofile synthetic_output_profile()
{
    Outputprofile p = {};
    p.synthetic = true;
    p.dac_reference_mv = 5000;
    for (unsigned i = 0; i < 4; ++i)
    {
        p.voltage[i].fitted = true;
        p.voltage[i].normal_max_mv = i < 2 ? 5000 : 1000;
        p.voltage[i].high_fault_mv = p.voltage[i].normal_max_mv;
    }
    for (unsigned i = 0; i < 2; ++i)
    {
        p.resistance[i].fitted = true;
        p.resistance[i].low_fault_index = 0;
        p.resistance[i].high_fault_index = 7;
        // Ohms deliberately unknown; bench examples select indices, not specified resistors.
    }
    return p;
}
bool valid_output_profile(const Outputprofile& p)
{
    if (p.synthetic == p.verified || p.dac_reference_mv < 1000 || p.dac_reference_mv > 5500)
        return false;
    for (unsigned i = 0; i < 8; ++i)
    {
        const Voltageprofile& v = p.voltage[i];
        if (!v.fitted)
            continue;
        if (i == ect_channel || i == iat_channel || v.normal_min_mv > v.normal_max_mv ||
            v.normal_max_mv > p.dac_reference_mv || v.low_fault_mv > p.dac_reference_mv ||
            v.high_fault_mv > p.dac_reference_mv)
            return false;
    }
    for (unsigned i = 0; i < 2; ++i)
    {
        const Resistanceprofile& r = p.resistance[i];
        if (!r.fitted)
            continue;
        if (r.low_fault_index > 7 || r.high_fault_index > 7 ||
            r.low_fault_index == r.high_fault_index)
            return false;
        if (p.verified)
            for (unsigned j = 0; j < 8; ++j)
                if (!r.ohms[j] || r.ohms[j] > 10000000UL)
                    return false;
    }
    return true;
}
static bool voltage(const Sensors& s, const Outputprofile& p, unsigned i, Outputplan& out)
{
    const Voltageprofile& v = p.voltage[i];
    if (!v.fitted || s.fault[i] == open_circuit)
        return true;
    uint16_t mv = s.millivolts[i];
    if (s.fault[i] == stuck_low)
        mv = v.low_fault_mv;
    else if (s.fault[i] == stuck_high)
        mv = v.high_fault_mv;
    else if (mv < v.normal_min_mv || mv > v.normal_max_mv)
        return false;
    // MCP4922 unity-gain transfer is Vref * code / 4096; full scale stops at code 4095.
    const uint32_t code = (static_cast<uint32_t>(mv) * 4096UL) / p.dac_reference_mv;
    out.dac_code[i] = static_cast<uint16_t>(code > 4095UL ? 4095UL : code);
    out.connect[i] = true;
    return true;
}
static bool resistance(const Sensors& s, const Outputprofile& p, unsigned i, Outputplan& out)
{
    const unsigned channel = ect_channel + i;
    const Resistanceprofile& r = p.resistance[i];
    if (!r.fitted || s.fault[channel] == open_circuit)
        return true;
    uint8_t index = s.resistance_index[i];
    if (s.fault[channel] == stuck_low)
        index = r.low_fault_index;
    else if (s.fault[channel] == stuck_high)
        index = r.high_fault_index;
    if (index > 7)
        return false;
    out.resistance_index[i] = index;
    out.connect[channel] = true;
    return true;
}
Outputplan plan_outputs(const Sensors& s, const Outputprofile& p, bool bench)
{
    Outputplan out = {};
    if (!valid_output_profile(p))
    {
        out.fault = true;
        return out;
    }
    if (!s.enabled || (p.synthetic && !bench))
        return out;
    bool valid = true;
    for (unsigned i = 0; i < 8; ++i)
    {
        if (s.fault[i] < normal || s.fault[i] > stuck_high)
            valid = false;
        if (!voltage(s, p, i, out))
            valid = false;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (!resistance(s, p, i, out))
            valid = false;
    if (!valid)
    {
        out = Outputplan{};
        out.fault = true;
        return out;
    }
    out.enabled = true;
    return out;
}
} // namespace Civic
