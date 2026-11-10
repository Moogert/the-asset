// Synthetic monitoring replay; this executable has no hardware port or device access.
#include <control/release_control.h>
#include <stdio.h>
int main()
{
    const Capstone::Releaseconfig cfg = Capstone::demonstration_release_config();
    printf(
        "evidence,run_id,time_s,raw_temp_c,filtered_temp_c,mg1_factor,mg2_factor,power1_w,power2_w,suspected,faults\n");
    for (unsigned scenario = 0U; scenario < 4U; ++scenario)
    {
        Capstone::Monitorstate state = Capstone::Monitorstate();
        for (unsigned tick = 0U; tick <= 100U; ++tick)
        {
            const Capstone::Timestamp now = tick * 10U;
            Capstone::Releaseinput input = Capstone::Releaseinput();
            for (unsigned i = 0U; i < Capstone::temperature_count; ++i)
            {
                input.temperature[i].valid = true;
                input.temperature[i].sensor_id = static_cast<uint16_t>(i + 1U);
                input.temperature[i].value = 25.0F;
                input.temperature[i].acquired_ms = now;
            }
            for (unsigned channel = 0U; channel < 2U; ++channel)
            {
                input.power[channel].valid = true;
                input.power[channel].value = 1000.0F;
                input.power[channel].uncertainty = 10.0F;
                input.power[channel].acquired_ms = now;
            }
            const unsigned sensor = scenario == 1U ? 4U : 0U;
            if (scenario < 2U)
                input.temperature[sensor].value = 65.0F + 0.3F * static_cast<float>(tick);
            if (scenario == 2U && tick == 50U)
                input.temperature[0].value = 100.0F;
            if (scenario == 3U && tick >= 30U)
                input.power[1].value = -1000.0F;
            const Capstone::Monitorresult result = Capstone::monitor(cfg, input, now, state);
            printf("synthetic_demo,revision_%u,%.3f,%.3f,%.3f,%.4f,%.4f,%.1f,%.1f,%u,%lu\n",
                   scenario,
                   static_cast<double>(now) / 1000.0,
                   static_cast<double>(input.temperature[sensor].value),
                   static_cast<double>(state.filtered[sensor]),
                   static_cast<double>(result.factor[0]),
                   static_cast<double>(result.factor[1]),
                   static_cast<double>(input.power[0].value),
                   static_cast<double>(input.power[1].value),
                   result.circulation_suspected ? 1U : 0U,
                   static_cast<unsigned long>(result.faults));
        }
    }
    return 0;
}
