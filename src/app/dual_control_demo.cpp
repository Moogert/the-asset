// Console demonstration using mocked hardware ports; printed inhibit requests are not physical measurements.
// Host-only first-review demonstration. No hardware, CAN or contactor output.
#include <control/control_runtime.h>
#include <stdio.h>

class Demoport : public Capstone::Hardwareport
{
  public:
    virtual bool send(const Capstone::Frame& frame)
    {
        printf("  simulated frame id=0x%03X counter=%u\n",
               static_cast<unsigned>(frame.identifier),
               static_cast<unsigned>(frame.data[3] >> 6U));
        return true;
    }
    virtual bool setinhibit(bool)
    {
        return true;
    }
    virtual void kickwatchdog()
    {
    }
    virtual bool mapcommand(unsigned, float effort, Capstone::Rawcommand& raw)
    {
        // Arbitrary DEMO digits only. Never substitute this for calibration.
        raw.throttle_a = static_cast<uint16_t>(1000.0F + effort);
        raw.throttle_b = raw.throttle_a;
        raw.io_bits = 8U;
        raw.regen_percent = 0U;
        return true;
    }
};

int main()
{
    puts("SIMULATION ONLY - C++ dual-motor supervisor; no hardware outputs");
    Capstone::Runtimeconfig cfg = Capstone::Runtimeconfig();
    cfg.policy = Capstone::simulationconfig();
    cfg.policy.stale_ms = 80U;
    cfg.period_ms = 10U;
    cfg.deadline_ms = 10U;
    cfg.bms_stale_ms = 80U;
    cfg.inhibit_budget_ms = 10U;
    cfg.control_id[0] = 0x03FU;
    cfg.control_id[1] = 0x040U;
    cfg.rise_percent_per_second = 1000.0F;
    Capstone::Runtimestate state = Capstone::Runtimestate();
    Demoport port;
    Capstone::initialize(0U, state, port);
    const char* stages[7] = {"disarmed startup",
                             "combined drive",
                             "combined drive",
                             "MG1 feedback lost",
                             "feedback restored; remains latched",
                             "deliberate disarmed reset",
                             "combined drive rearmed"};
    for (unsigned step = 0U; step < 7U; ++step)
    {
        const uint32_t now = (step + 1U) * 10U;
        Capstone::Snapshot snapshot = Capstone::Snapshot();
        snapshot.policy.arm = (step != 0U) && (step != 5U);
        snapshot.policy.drive_request = snapshot.policy.arm;
        snapshot.policy.request_percent = snapshot.policy.arm ? 100.0F : 0.0F;
        snapshot.policy.permission = true;
        snapshot.policy.reset_request = step == 5U;
        snapshot.policy.mode = Capstone::combined;
        snapshot.policy.discharge_limit_a = 200.0F;
        snapshot.policy.charge_limit_a = 20.0F;
        snapshot.bms_valid = true;
        snapshot.bms_received_ms = now;
        snapshot.transport_healthy = true;
        snapshot.watchdog_healthy = true;
        snapshot.energy_ready = true;
        for (unsigned motor = 0U; motor < 2U; ++motor)
        {
            snapshot.policy.motor[motor].valid = !((step == 3U) && (motor == 0U));
            snapshot.policy.motor[motor].ready = true;
            snapshot.policy.motor[motor].rotor_valid = true;
            snapshot.policy.motor[motor].received_ms = now;
            snapshot.policy.motor[motor].motor_temp_c = 25.0F;
            snapshot.policy.motor[motor].inverter_temp_c = 25.0F;
        }
        printf("%u ms: %s\n", now, stages[step]);
        const Capstone::Cycle result = Capstone::run(cfg, snapshot, now, state, port);
        printf("  MG1=%.1f%% MG2=%.1f%% inhibit-request=%s faults=0x%08X\n",
               static_cast<double>(result.policy.command_percent[0]),
               static_cast<double>(result.policy.command_percent[1]),
               result.inhibit_requested ? "ON" : "OFF",
               result.faults);
    }
    return 0;
}
