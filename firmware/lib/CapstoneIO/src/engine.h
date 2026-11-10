#ifndef CAPSTONE_ENGINE_H
#define CAPSTONE_ENGINE_H
// Portable teaching model; the board adapter owns I/O, interrupt exclusion and watchdog hardware.
#include <stdint.h>
#include <stddef.h>
namespace Civic
{
struct Edge
{
    uint16_t angle; // Tenths of a crank degree within one 720-degree cycle.
    uint8_t levels; // Complete channel level mask after the edge, not a toggle mask.
};
struct Engineprofile
{
    Edge edges[128];
    uint8_t count;
    uint8_t channel_mask;
    bool verified;  // Released physical profile; mutually exclusive with synthetic.
    bool synthetic; // Artificial pattern requiring the separate bench contact.
    uint16_t max_rpm;
};
Engineprofile synthetic_profile();
bool valid_profile(const Engineprofile& p);
enum Enginemode
{
    stopped,
    cranking,
    running,
    warming
};
enum Inject
{
    normal,
    open_circuit,
    stuck_low,
    stuck_high
};
struct Sensors
{
    uint16_t millivolts[8];      // Voltage channels 0..3; 4/5 are resistance roles, 6/7 spare.
    uint8_t resistance_index[2]; // ECT, IAT: synthetic relay-ladder selections
    Inject fault[8];
    bool enabled;
    bool actuator_feedback_valid;
};
struct Sensorstate
{
    bool enabled, heater, actuator_seen;
    Enginemode mode;
    uint8_t throttle, load;
    uint16_t injection_us;
    uint32_t mode_start_ms, actuator_ms;
    Inject fault[8];
};
Sensors calculate_sensors(const Sensorstate& state, uint32_t now_ms);
enum Engineoperation
{
    invalid_operation,
    disarm_operation,
    ping_operation,
    arm_operation,
    state_operation,
    rpm_operation,
    throttle_operation,
    load_operation,
    drop_operation,
    fault_operation
};
struct Engineinstruction
{
    Engineoperation operation;
    uint16_t value;
    uint8_t channel;
    Inject condition;
    bool arm_permitted; // Set only by profile/bench validation in prepare_command.
};
// All physical ranges in the synthetic model are teaching examples, not Honda calibration.
class Engine
{
  public:
    explicit Engine(const Engineprofile& profile);
    bool command(const char* text, uint32_t now_ms, bool bench_jumper);
    // Parse outside interrupt exclusion; apply only successfully prepared instructions atomically.
    bool prepare_command(const char* text, bool bench_jumper, Engineinstruction& instruction) const;
    bool apply_command(const Engineinstruction& instruction, uint32_t now_ms);
    void update(uint32_t now_ms, bool enable_contact);
    // ISR call: true means a new mask is supplied; false means no edge is due.
    bool next_edge(uint32_t now_us, uint8_t& levels);
    uint32_t wait_us(uint32_t now_us) const;
    // Commands only: no returned field is a measured Honda sensor value.
    Sensors sensors(uint32_t now_ms) const;
    Sensorstate capture_sensors() const;
    void observe_actuators(uint16_t injector_pulse_us, bool oxygen_heater, uint32_t now_ms);
    void stop();
    // Applied in an interrupt-protected foreground transaction by the board adapter.
    void apply_indication(const Inject conditions[8]);
    bool armed() const
    {
        return enabled_;
    }
    uint16_t rpm() const
    {
        return mode_ == stopped ? 0 : rpm_;
    }
    Enginemode mode() const
    {
        return mode_;
    }
    bool timing_fault() const
    {
        return timing_fault_;
    }
    uint8_t drop_mask() const
    {
        return drop_;
    }

  private:
    Engineprofile profile_;
    Enginemode mode_;
    uint16_t rpm_;
    uint8_t throttle_, load_, drop_, edge_;
    bool enabled_, scheduled_, timing_fault_;
    // Heartbeat/mode_start use milliseconds; deadline/cycle_start/cycle_us use microseconds.
    uint32_t heartbeat_, mode_start_, deadline_, cycle_start_, cycle_us_;
    Inject faults_[8];
    Inject indication_[8];
    uint16_t injection_us_;
    bool oxygen_heater_, actuator_seen_;
    uint32_t actuator_at_;
};
// Fixed-length, overflow-discarding serial line assembler. No heap or String.
class Lines
{
  public:
    Lines() : used_(0), discard_(false)
    {
        text_[0] = 0;
    }
    bool feed(char c);
    const char* text() const
    {
        return text_;
    }

  private:
    char text_[96];
    uint8_t used_;
    bool discard_;
};
} // namespace Civic
#endif
