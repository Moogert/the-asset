#include <Arduino.h>
#include <EEPROM.h>
#include <FlexCAN_T4.h>
#include <stdio.h>
#include <string.h>
#include "indication.h"
#include "detail.h"
#include "journal.h"
#include "vehicle_profile.h"
using namespace Civic;
// Vehicle image has no reader CAN, Honda K-line master or MIL driver.
static FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> ev_can;
static Store faults;
static Telemetryreceiver receiver;
static Detailreceiver detail_receiver;
static Faultdetail details[64] = {};
static uint32_t detail_received[64] = {};
static unsigned detail_count = 0;
static bool detail_overflow = false;
static Telemetry latest = {};
static Lines usb_lines, emulator_lines;
static Indicationstatus mapping = {};
static const uint8_t watchdog_pin = 10, bench_pin = 12, park_pin = 4, armed_pin = 5;
static uint32_t ev_at = 0, emulator_at = 0, ack_at = 0, sent_at = 0, loop_at = 0;
static bool ev_seen = false, emulator_seen = false, emulator_healthy = false, synthetic = false;
static uint16_t sequence = 0;
static uint8_t sent_mask = 0;
static bool command_sent = false;
class Eepromport : public Nvport
{
  public:
    uint8_t read(unsigned address) override
    {
        return EEPROM.read(address);
    }
    bool write(unsigned address, uint8_t value) override
    {
        EEPROM.update(address, value);
        return EEPROM.read(address) == value;
    }
};
static Eepromport nv;
static Journal journal(nv);
static bool storage_ok = true;
static uint32_t saved_revision = 0, writing_revision = 0, persisted_at = 0;
class Usbtransport : public Diagnostictransport
{
  public:
    Usbtransport() : size_(0), offset_(0)
    {
    }
    bool write_record(const char* bytes, size_t count) override
    {
        if (size_ || count > sizeof(buffer_))
            return false;
        memcpy(buffer_, bytes, count);
        size_ = count;
        offset_ = 0;
        return true;
    }
    void tick()
    {
        if (!size_ || !Serial)
            return;
        const int available = Serial.availableForWrite();
        if (available <= 0)
            return;
        size_t count = size_ - offset_;
        if (count > 32)
            count = 32;
        if (count > static_cast<size_t>(available))
            count = static_cast<size_t>(available);
        offset_ += Serial.write(reinterpret_cast<const uint8_t*>(buffer_ + offset_), count);
        if (offset_ == size_)
        {
            size_ = 0;
            offset_ = 0;
        }
    }

  private:
    char buffer_[640];
    size_t size_, offset_;
};
static Usbtransport usb;
// Unbound until Bluetooth hardware, voltage levels and nonblocking adapter are reviewed.
static Diagnostictransport* bluetooth = 0;
static unsigned dump_index = 0;
static bool dumping = false;
static void command_reply(const char* line)
{
    // Queue whole responses; direct Serial.println could split an in-flight JSON record.
    usb.write_record(line, strlen(line));
}
static void dump_tick(uint32_t now)
{
    if (!dumping)
        return;
    Indicationstatus status = mapping;
    status.fresh =
        command_sent && static_cast<uint32_t>(now - ack_at) <= 500 && status.acknowledged;
    char bytes[640];
    if (dump_index >= faults.size() + detail_count)
    {
        dumping = false;
        return;
    }
    Faultdetail detail = {};
    if (dump_index >= faults.size())
    {
        const unsigned index = dump_index - faults.size();
        detail = details[index];
        if (static_cast<uint32_t>(now - detail_received[index]) > 250)
            detail.valid = false;
    }
    const size_t count =
        dump_index < faults.size()
            ? diagnostic_record(faults.at(dump_index),
                                status,
                                emulator_seen && static_cast<uint32_t>(now - emulator_at) <= 500 &&
                                    emulator_healthy,
                                synthetic,
                                bytes,
                                sizeof(bytes))
            : detail_record(detail, status, synthetic, bytes, sizeof(bytes));
    if (count && publish_record(bytes, count, usb, bluetooth))
        ++dump_index;
}
static void console(const char* line, uint32_t now)
{
    if (!strcmp(line, "DUMP"))
    {
        dump_index = 0;
        dumping = true;
        return;
    }
    if (!strcmp(line, "CLEAR"))
    {
        const bool fresh = ev_seen && static_cast<uint32_t>(now - ev_at) <= 250;
        const bool parked = fresh && Vehicleprofile::service_inputs_verified &&
                            digitalRead(park_pin) == LOW && (latest.flags & 1) &&
                            (latest.context.valid & 1) && latest.context.speed_cms == 0;
        const bool disarmed = fresh && digitalRead(armed_pin) == HIGH && !(latest.flags & 2);
        command_reply(
            faults.clear(parked, disarmed)
                ? "{\"type\":\"command\",\"status\":\"inactive_EV_history_cleared\"}\n"
                : "{\"type\":\"command\",\"status\":\"parked_disarmed_evidence_required\"}\n");
        return;
    }
    unsigned long legacy, runtime;
    unsigned channels;
    char extra;
    if (digitalRead(bench_pin) == LOW &&
        sscanf(line, "SIM %lx %lx %u %c", &legacy, &runtime, &channels, &extra) == 3 &&
        channels <= 3)
    {
        latest = Telemetry{};
        latest.legacy = legacy;
        latest.runtime = runtime;
        latest.channel_faults = static_cast<uint8_t>(channels);
        synthetic = true;
        faults.update_masks(
            latest.legacy, latest.runtime, latest.channel_faults, now, latest.context);
        command_reply("{\"type\":\"command\",\"status\":\"synthetic_demo\"}\n");
        return;
    }
    command_reply("{\"type\":\"command\",\"status\":\"invalid_command\"}\n");
}
static void emulator_line(const char* line, uint32_t now)
{
    Indicationstatus candidate = {};
    if (command_sent &&
        indication_ack(
            line, Vehicleprofile::indication_profile().revision, sequence, sent_mask, candidate))
    {
        mapping = candidate;
        ack_at = now;
        return;
    }
    unsigned enabled, timing;
    char extra;
    if (sscanf(line, "EMU %u %u %c", &enabled, &timing, &extra) == 2 && enabled <= 1 && timing <= 1)
    {
        emulator_seen = true;
        emulator_at = now;
        emulator_healthy = enabled && !timing;
    }
}
static Canframe convert(const CAN_message_t& message)
{
    Canframe frame = {};
    frame.id = static_cast<uint16_t>(message.id);
    frame.length = message.len;
    memcpy(frame.data, message.buf, 8);
    return frame;
}
static void send_categories(uint32_t now, bool fresh)
{
    if (static_cast<uint32_t>(now - sent_at) < 100)
        return;
    uint8_t mask = fault_categories(latest);
    if (!fresh)
        mask |= static_cast<uint8_t>(1U << communication_category);
    if (!storage_ok || detail_overflow)
        mask |= static_cast<uint8_t>(1U << configuration_category);
    const uint16_t next = static_cast<uint16_t>(sequence + 1);
    char bytes[96];
    const size_t count = indication_command(
        Vehicleprofile::indication_profile().revision, next, mask, bytes, sizeof(bytes));
    if (!count || Serial2.availableForWrite() < static_cast<int>(count + 5))
        return;
    Serial2.write(reinterpret_cast<const uint8_t*>(bytes), count);
    Serial2.println("PING");
    if (mapping.requested != mask)
        mapping.acknowledged = false;
    sequence = next;
    sent_mask = mask;
    sent_at = now;
    command_sent = true;
    if (Vehicleprofile::honda_verified && fresh && Vehicleprofile::vss_pulses_per_km &&
        (latest.context.valid & 1))
    {
        const uint32_t mhz = static_cast<uint32_t>(
            (static_cast<uint64_t>(latest.context.speed_cms) * Vehicleprofile::vss_pulses_per_km) /
            100);
        char speed[32];
        const int n = snprintf(speed, sizeof(speed), "VSS %lu\n", static_cast<unsigned long>(mhz));
        if (n > 0 && Serial2.availableForWrite() >= n)
            Serial2.print(speed);
    }
    else if (Serial2.availableForWrite() >= 6)
        Serial2.println("VSS 0");
    digitalWrite(watchdog_pin, !digitalRead(watchdog_pin));
}
static void persist(uint32_t now)
{
    if (storage_ok && !journal.busy() && saved_revision != faults.revision() &&
        static_cast<uint32_t>(now - persisted_at) >= 5000)
    {
        writing_revision = faults.revision();
        storage_ok = journal.start(faults);
        persisted_at = now;
        if (!storage_ok)
            faults.storage_failure(now);
    }
    const bool writing = journal.busy();
    if (writing && !journal.tick())
    {
        storage_ok = false;
        faults.storage_failure(now);
    }
    else if (writing && !journal.busy())
        saved_revision = writing_revision;
}
void setup()
{
    pinMode(watchdog_pin, OUTPUT);
    digitalWrite(watchdog_pin, LOW);
    pinMode(bench_pin, INPUT_PULLUP);
    pinMode(park_pin, INPUT_PULLUP);
    pinMode(armed_pin, INPUT_PULLUP);
    Serial.begin(115200);
    Serial2.begin(115200);
    if (Vehicleprofile::ev_telemetry_verified)
    {
        ev_can.begin();
        ev_can.setBaudRate(Vehicleprofile::ev_can_bitrate);
    }
    storage_ok = journal.restore(faults);
    if (!storage_ok)
        faults.storage_failure(0);
    saved_revision = faults.revision();
    loop_at = millis();
    Serial.println(
        "Honda-mediated diagnostics: original ECU owns OBD/MIL; vehicle profiles unverified; Bluetooth unbound.");
}
void loop()
{
    const uint32_t now = millis();
    Context empty = {};
    faults.set(
        0x1f04, gateway_source, static_cast<uint32_t>(now - loop_at) > 100, now, empty, true);
    loop_at = now;
    if (Vehicleprofile::ev_telemetry_verified)
        for (unsigned i = 0; i < 32; ++i)
        {
            CAN_message_t message;
            if (!ev_can.read(message))
                break;
            if (message.flags.extended || message.flags.remote)
                continue;
            if (message.id >= 0x660 && message.id <= 0x662)
            {
                Faultdetail d;
                if (detail_receiver.receive(convert(message), now, d))
                {
                    unsigned index = 0;
                    while (index < detail_count && !(details[index].identity == d.identity &&
                                                     details[index].component == d.component &&
                                                     details[index].severity == d.severity))
                        ++index;
                    if (index < 64)
                    {
                        details[index] = d;
                        detail_received[index] = now;
                        if (index == detail_count)
                            ++detail_count;
                    }
                    else
                        detail_overflow = true;
                }
                continue;
            }
            if (message.id < 0x650 || message.id > 0x652)
                continue;
            Telemetry data;
            if (receiver.receive(convert(message), now, data))
            {
                latest = data;
                ev_seen = true;
                ev_at = now;
                synthetic = false;
                faults.update_masks(
                    data.legacy, data.runtime, data.channel_faults, now, data.context);
            }
        }
    for (unsigned i = 0; i < 32 && Serial.available(); ++i)
        if (usb_lines.feed(static_cast<char>(Serial.read())))
            console(usb_lines.text(), now);
    for (unsigned i = 0; i < 32 && Serial2.available(); ++i)
        if (emulator_lines.feed(static_cast<char>(Serial2.read())))
            emulator_line(emulator_lines.text(), now);
    const bool fresh = ev_seen && static_cast<uint32_t>(now - ev_at) <= 250;
    faults.set(0x1f00, gateway_source, !fresh, now, empty, true);
    faults.set(0x1f02,
               gateway_source,
               !emulator_seen || static_cast<uint32_t>(now - emulator_at) > 500 ||
                   !emulator_healthy || !mapping.acknowledged ||
                   static_cast<uint32_t>(now - ack_at) > 500,
               now,
               empty,
               true);
    faults.set(0x1f06,
               gateway_source,
               !Vehicleprofile::honda_verified || !Vehicleprofile::ev_telemetry_verified ||
                   mapping.unavailable != 0,
               now,
               empty,
               true);
    faults.set(0x1f07, gateway_source, detail_overflow, now, empty, true);
    send_categories(now, fresh);
    persist(now);
    dump_tick(now);
    usb.tick();
}
