#ifndef CIVIC_INDICATION_H
#define CIVIC_INDICATION_H
#include "engine.h"
#include "diagnostics.h"
namespace Civic
{
const uint8_t indication_version = 1;
const unsigned category_count = 8;
// Categories are project labels, never Honda DTC assignments.
enum Category
{
    thermal_category,
    circulation_category,
    feedback_category,
    channel_category,
    battery_category,
    communication_category,
    timing_category,
    configuration_category
};
struct Indicationmapping
{
    uint8_t sensor;
    Inject condition;
    bool available; // A fitted output interface exists; distinct from ECU/code verification.
    bool electrical_verified, ecu_verified;
    uint16_t observed_honda_code;
    uint32_t observed_delay_ms;
    const char* prerequisites;
};
struct Indicationprofile
{
    uint16_t revision;
    bool production;
    Indicationmapping mapping[category_count];
};
Indicationprofile demonstration_indication_profile();
bool valid_indication_profile(const Indicationprofile& profile);
uint8_t fault_categories(const Telemetry& telemetry);
struct Indicationstatus
{
    uint16_t sequence, revision;
    uint8_t requested, applied, unavailable;
    bool fresh, acknowledged;
};
// Entire category set is applied as one transaction; no per-signal transient clearing.
class Indicationreceiver
{
  public:
    explicit Indicationreceiver(const Indicationprofile& profile);
    bool receive(const char* line,
                 uint32_t now,
                 bool bench,
                 Engine& engine,
                 char* acknowledgment,
                 size_t capacity);
    void tick(uint32_t now, Engine& engine);
    Indicationstatus status(uint32_t now) const;

  private:
    Indicationprofile profile_;
    Indicationstatus status_;
    bool seen_;
    uint32_t received_ms_;
};
size_t indication_command(
    uint16_t revision, uint16_t sequence, uint8_t categories, char* output, size_t capacity);
bool indication_ack(const char* line,
                    uint16_t revision,
                    uint16_t sequence,
                    uint8_t requested,
                    Indicationstatus& status);
// Transport-neutral output contract: USB and a future Bluetooth adapter use identical bytes.
class Diagnostictransport
{
  public:
    virtual ~Diagnostictransport()
    {
    }
    virtual bool write_record(const char* bytes, size_t count) = 0;
};
size_t diagnostic_record(const Fault& fault,
                         const Indicationstatus& indication,
                         bool emulator_healthy,
                         bool synthetic,
                         char* output,
                         size_t capacity);
bool publish_record(const char* bytes,
                    size_t count,
                    Diagnostictransport& usb,
                    Diagnostictransport* bluetooth);
} // namespace Civic
#endif
