#ifndef CAPSTONE_DIAGNOSTICS_H
#define CAPSTONE_DIAGNOSTICS_H
// Bounded diagnostic state and application payloads; no traction output or reset interface.
#include <stdint.h>
#include <stddef.h>
namespace Civic
{
enum Source
{
    gateway_source = 0,
    supervisor_source = 1,
    runtime_source = 2,
    honda_source = 3,
    mg1_source = 4,
    mg2_source = 5
};
struct Context
{
    uint16_t speed_cms, pack_decivolts; // cm/s and 0.1 V; valid bits govern use.
    int16_t temperature_decic;          // Signed 0.1 degree C.
    uint8_t valid;                      // Bit 0 speed, bit 1 voltage, bit 2 temperature.
};
struct Fault
{
    uint16_t code, occurrences;
    uint8_t source;
    bool active, pending, stored; // Current source assertion, pending status, retained history.
    uint32_t first_ms, last_ms, onset_ms; // Boot-relative milliseconds, not wall-clock time.
    Context first_context; // Preserve the first event; absent channels retain clear valid bits.
};
const unsigned fault_capacity = 64;
class Store
{
  public:
    Store();
    void set(uint16_t code,
             Source source,
             bool present,
             uint32_t now,
             const Context& context,
             bool confirmed = false,
             bool pending_only = false);
    void update_masks(uint32_t legacy,
                      uint32_t runtime,
                      uint8_t channel_faults,
                      uint32_t now,
                      const Context& context);
    bool clear(bool parked, bool disarmed);
    bool mil() const;
    unsigned codes(bool pending, uint16_t* output, unsigned capacity) const;
    unsigned size() const
    {
        return count_;
    }
    // Caller supplies i < size(); USB preserves source identity lost by a generic reader.
    const Fault& at(unsigned i) const
    {
        return faults_[i];
    }
    uint32_t revision() const
    {
        return revision_;
    }
    // save returns zero when too small; load validates completely before replacing state.
    size_t save(uint8_t* bytes, size_t capacity, uint32_t generation) const;
    bool load(const uint8_t* bytes, size_t size, uint32_t& generation);
    void storage_failure(uint32_t now);

  private:
    Fault faults_[fault_capacity];
    unsigned count_;
    uint32_t revision_;
};
void format_code(uint16_t code, char output[6]);
const char* code_description(uint16_t code);
uint16_t crc16(const uint8_t* bytes, size_t count);
struct Telemetry
{
    uint32_t legacy, runtime;
    uint8_t flags, channel_faults;
    Context context;
};
struct Canframe
{
    uint16_t id;    // Standard identifier; the board rejects extended/remote frames.
    uint8_t length; // Payload byte count, checked against the eight-byte frame capacity.
    uint8_t data[8];
};
void encode_telemetry(const Telemetry& data, uint8_t counter, Canframe output[3]);
// Project protocol: 650/651/652, ordered triplet, CRC, counter and 20 ms assembly deadline.
class Telemetryreceiver
{
  public:
    Telemetryreceiver();
    bool receive(const Canframe& frame, uint32_t now, Telemetry& result);

  private:
    uint8_t bytes_[24], stage_, last_counter_;
    bool have_counter_;
    uint32_t start_;
};
class Obd
{
  public:
    explicit Obd(Store& store) : store_(store), parked_(false), disarmed_(false)
    {
    }
    void service_permission(bool parked, bool disarmed)
    {
        parked_ = parked;
        disarmed_ = disarmed;
    }
    // Raw OBD application payload, without ISO-TP header. Unsupported functional requests stay silent.
    size_t request(const uint8_t* in, size_t count, bool functional, uint8_t* out, size_t capacity);

  private:
    Store& store_;
    bool parked_, disarmed_;
};
} // namespace Civic
#endif
