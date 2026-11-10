// Historical isolated bench demonstration. The default vehicle image never compiles this file.
#ifndef CAPSTONE_LEGACY_OBD_DEMO
#error "Build only with the explicitly isolated legacy demonstration environment"
#endif
#include <Arduino.h>
#include <EEPROM.h>
#include <FlexCAN_T4.h>
#include <stdio.h>
#include <string.h>
#include "engine.h"
#include "transport.h"
#include "journal.h"
#include "vehicle_profile.h"
using namespace Civic;
// The reader and EV buses are physically separate; no diagnostic request is forwarded.
static FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> diagnostic_can;
static FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> ev_can;
static Store faults;
static Obd obd(faults);
static Isotp transport;
static Telemetryreceiver telemetry_receiver;
static Telemetry latest = {};
static Lines console_lines, emulator_lines;
static const uint8_t lamp_pin = 9, watchdog_pin = 10, bench_pin = 12, park_pin = 4, armed_pin = 5,
                     honda_mil_pin = 6;
static uint32_t ev_at = 0, emulator_at = 0, honda_at = 0, last_ping = 0, last_persist = 0,
                last_loop = 0;
static bool ev_seen = false, emulator_seen = false, emulator_healthy = false, honda_seen = false,
            honda_mil = false, storage_ok = true, simulated = false;
static uint32_t saved_revision = 0, writing_revision = 0;
static uint16_t honda_stored[24] = {}, honda_pending[24] = {};
static uint8_t honda_stored_count = 0, honda_pending_count = 0;
static uint32_t honda_query_at[3] = {};
static bool honda_query_seen[3] = {};
class Eepromport : public Nvport
{
  public:
    uint8_t read(unsigned a) override
    {
        return EEPROM.read(a);
    }
    bool write(unsigned a, uint8_t v) override
    {
        EEPROM.update(a, v);
        return EEPROM.read(a) == v;
    }
};
static Eepromport nv;
static Journal journal(nv);
// Nonblocking 5-baud initialization + ISO 9141 UART polling template.
// Disabled until actual ECU, protocol, headers, echo and physical layer are verified.
class Hondalink
{
  public:
    Hondalink()
        : state_(0), bit_(0), at_(0), last_(0), key_count_(0), mode_index_(0), tx_size_(0),
          tx_at_(0), tx_count_(0), echo_(0), got_(false), collect_count_(0)
    {
    }
    void tick(uint32_t now)
    {
        if (!Vehicleprofile::honda_verified)
            return;
        // State 0: drive the slow-initialization start bit; the exact ECU profile remains gated.
        if (state_ == 0)
        {
            Serial3.end();
            pinMode(14, OUTPUT);
            digitalWrite(14, LOW);
            pinMode(17, OUTPUT);
            digitalWrite(17, Vehicleprofile::honda_requires_l_init ? LOW : HIGH);
            at_ = now;
            bit_ = 0;
            state_ = 1;
            return;
        }
        // State 1: send the template address LSB-first at five baud, then restore UART operation.
        if (state_ == 1 && static_cast<uint32_t>(now - at_) >= 200)
        {
            at_ = now;
            ++bit_;
            if (bit_ <= 8)
            {
                bool high = (0x33 >> (bit_ - 1)) & 1;
                digitalWrite(14, high ? HIGH : LOW);
                if (Vehicleprofile::honda_requires_l_init)
                    digitalWrite(17, high ? HIGH : LOW);
            }
            else if (bit_ == 9)
            {
                digitalWrite(14, HIGH);
                digitalWrite(17, HIGH);
            }
            else
            {
                Serial3.begin(Vehicleprofile::honda_uart_baud);
                state_ = 2;
                key_count_ = 0;
            }
            return;
        }
        // State 2: collect sync/key bytes; timeout causes backoff, never a healthy status.
        if (state_ == 2)
        {
            while (Serial3.available() && key_count_ < 3)
                keys_[key_count_++] = static_cast<uint8_t>(Serial3.read());
            if (key_count_ == 3)
            {
                if (keys_[0] != 0x55)
                {
                    retry(now);
                    return;
                }
                state_ = 3;
                at_ = now;
            }
            else if (static_cast<uint32_t>(now - at_) > 400)
                retry(now);
            return;
        }
        // State 3: wait the template keyword response interval before sending its complement.
        if (state_ == 3 && static_cast<uint32_t>(now - at_) >= 30)
        {
            Serial3.write(static_cast<uint8_t>(keys_[2] ^ 0xff));
            state_ = 4;
            at_ = now;
            return;
        }
        // State 4: await the complemented address after any local transmit echo.
        if (state_ == 4)
        {
            // Discard local transmitted-key echo, await complemented initialization address.
            for (unsigned i = 0; i < 8 && Serial3.available(); ++i)
                if (Serial3.read() == 0xcc)
                {
                    state_ = 5;
                    at_ = now;
                    return;
                }
            if (static_cast<uint32_t>(now - at_) > 400)
                retry(now);
            return;
        }
        // State 8: retry backoff keeps a disconnected ECU from causing a tight initialization loop.
        if (state_ == 8)
        {
            if (static_cast<uint32_t>(now - at_) > 3000)
                state_ = 0;
            return;
        }
        // State 5: cycle through status, stored codes and pending codes after the inter-request gap.
        if (state_ == 5 && static_cast<uint32_t>(now - at_) >= 100)
        {
            static const uint8_t modes[] = {1, 3, 7};
            expected_ = modes[mode_index_];
            mode_index_ = static_cast<uint8_t>((mode_index_ + 1) % 3);
            tx_size_ = honda_request(expected_, tx_, sizeof(tx_));
            tx_count_ = 0;
            echo_ = 0;
            parser_.reset();
            got_ = false;
            collect_count_ = 0;
            state_ = 6;
            tx_at_ = now;
        }
        // State 6: pace outgoing UART bytes; the foreground task does not block on a full request.
        if (state_ == 6 && static_cast<uint32_t>(now - tx_at_) >= 6)
        {
            Serial3.write(tx_[tx_count_++]);
            tx_at_ = now;
            if (tx_count_ == tx_size_)
            {
                state_ = 7;
                at_ = now;
                last_ = now;
            }
        }
        // States 6/7: discard request echoes and collect bounded, checksum-validated reply frames.
        if (state_ == 6 || state_ == 7)
        {
            for (unsigned i = 0; i < 32 && Serial3.available(); ++i)
            {
                uint8_t b = static_cast<uint8_t>(Serial3.read());
                if (echo_ < tx_count_ && b == tx_[echo_])
                {
                    ++echo_;
                    continue;
                }
                echo_ = tx_size_;
                parser_.feed(b, now);
                last_ = now;
            }
            Hondaresult result;
            if (parser_.finish(now, expected_, result))
            {
                got_ = true;
                if (expected_ == 1)
                {
                    honda_mil = result.mil;
                    honda_query_at[0] = now;
                    honda_query_seen[0] = true;
                }
                else
                    for (unsigned i = 0; i < result.count; ++i)
                    {
                        bool duplicate = false;
                        for (unsigned j = 0; j < collect_count_; ++j)
                            if (collect_[j] == result.codes[i])
                                duplicate = true;
                        if (!duplicate && collect_count_ < 24)
                            collect_[collect_count_++] = result.codes[i];
                        else if (!duplicate)
                        {
                            Context c = {};
                            faults.set(0x1f07, gateway_source, true, now, c, true);
                        }
                    }
            }
            if (state_ == 7 && static_cast<uint32_t>(now - last_) > 100)
            {
                if (got_)
                {
                    commit(now);
                    state_ = 5;
                    at_ = now;
                }
                else if (static_cast<uint32_t>(now - at_) > 500)
                    retry(now);
            }
        }
    }

  private:
    void retry(uint32_t now)
    {
        state_ = 8;
        at_ = now;
        Serial3.end();
        pinMode(14, OUTPUT);
        digitalWrite(14, HIGH);
        digitalWrite(17, HIGH);
    }
    void commit(uint32_t now)
    {
        if (expected_ != 3 && expected_ != 7)
            return;
        uint16_t* dest = expected_ == 3 ? honda_stored : honda_pending;
        uint8_t& count = expected_ == 3 ? honda_stored_count : honda_pending_count;
        count = collect_count_;
        memcpy(dest, collect_, count * sizeof(uint16_t));
        unsigned query = expected_ == 3 ? 1 : 2;
        honda_query_at[query] = now;
        honda_query_seen[query] = true;
        Context c = {};
        // Reconcile only after the complete valid response window, not after the first frame.
        unsigned n = faults.size();
        for (unsigned i = 0; i < n; ++i)
        {
            const Fault& f = faults.at(i);
            if (f.source != honda_source)
                continue;
            bool stored = false, pending = false;
            for (unsigned j = 0; j < honda_stored_count; ++j)
                if (honda_stored[j] == f.code)
                    stored = true;
            for (unsigned j = 0; j < honda_pending_count; ++j)
                if (honda_pending[j] == f.code)
                    pending = true;
            faults.set(f.code, honda_source, stored || pending, now, c, stored, !stored);
        }
        for (unsigned i = 0; i < honda_stored_count; ++i)
            faults.set(honda_stored[i], honda_source, true, now, c, true);
        for (unsigned i = 0; i < honda_pending_count; ++i)
            faults.set(honda_pending[i], honda_source, true, now, c, false, true);
    }
    Hondaparser parser_;
    uint8_t state_, bit_;
    uint32_t at_, last_;
    uint8_t keys_[3], key_count_, mode_index_, expected_;
    uint8_t tx_[8];
    size_t tx_size_;
    uint32_t tx_at_;
    size_t tx_count_, echo_;
    bool got_;
    uint16_t collect_[24];
    uint8_t collect_count_;
};
static Hondalink honda;
// Transmit on the separate diagnostic CAN controller only; never forward to the EV control bus.
static bool send_obd(const Canframe& f)
{
    CAN_message_t msg = {};
    msg.id = f.id;
    msg.len = f.length;
    memcpy(msg.buf, f.data, 8);
    return diagnostic_can.write(msg) != 0;
}
// Copy one classic-CAN payload after the caller rejects extended/remote frames.
static Canframe convert(const CAN_message_t& msg)
{
    Canframe f = {};
    f.id = static_cast<uint16_t>(msg.id);
    f.length = msg.len;
    memcpy(f.data, msg.buf, 8);
    return f;
}
static unsigned dump_index = 0;
static bool dumping = false;
// Emit at most one USB record when buffer space permits, avoiding a long synchronous dump.
static void dump_tick()
{
    if (!dumping || Serial.availableForWrite() < 384)
        return;
    if (dump_index == 0)
    {
        Serial.println(simulated ? "EVIDENCE=synthetic_demo"
                                 : "EVIDENCE=physical_sources_unverified_until_profile_release");
        Serial.print("persistence_busy=");
        Serial.println(journal.busy() ? 1 : 0);
    }
    unsigned i = dump_index++;
    if (i < faults.size())
    {
        const Fault& f = faults.at(i);
        char code[6];
        format_code(f.code, code);
        Serial.print(code);
        Serial.print(" source=");
        Serial.print(f.source);
        Serial.print(" active=");
        Serial.print(f.active);
        Serial.print(" stored=");
        Serial.print(f.stored);
        Serial.print(" pending=");
        Serial.print(f.pending);
        Serial.print(" first_boot_ms=");
        Serial.print(f.first_ms);
        Serial.print(" count=");
        Serial.print(f.occurrences);
        Serial.print(" context_valid=");
        Serial.print(f.first_context.valid);
        Serial.print(" speed_cms=");
        Serial.print(f.first_context.speed_cms);
        Serial.print(" pack_dV=");
        Serial.print(f.first_context.pack_decivolts);
        Serial.print(" temp_dC=");
        Serial.print(f.first_context.temperature_decic);
        Serial.print(' ');
        Serial.println(f.source == honda_source ? "Honda original code" : code_description(f.code));
    }
    if (dump_index >= faults.size())
        dumping = false;
}
// Allow synthetic fault injection only with the isolated bench contact physically asserted.
static void console(const char* s)
{
    if (!strcmp(s, "DUMP"))
    {
        dump_index = 0;
        dumping = true;
        return;
    }
    // Synthetic injection is physically gated and never represents a measured EV fault.
    if (digitalRead(bench_pin) != LOW)
    {
        Serial.println("ERR bench jumper required");
        return;
    }
    unsigned long legacy, runtime;
    unsigned channels;
    char extra;
    if (sscanf(s, "SIM %lx %lx %u %c", &legacy, &runtime, &channels, &extra) == 3 && channels <= 3)
    {
        simulated = true;
        latest = Telemetry{};
        latest.legacy = legacy;
        latest.runtime = runtime;
        latest.channel_faults = static_cast<uint8_t>(channels);
        faults.update_masks(
            latest.legacy, latest.runtime, latest.channel_faults, millis(), latest.context);
        Serial.println("OK synthetic_demo; real-source unavailable faults remain");
        return;
    }
    Serial.println("ERR DUMP or SIM <legacy_hex> <runtime_hex> <channels_0_to_3>");
}
// Keep the lamp driver disabled while restoring history and starting the isolated reader endpoint.
void setup()
{
    pinMode(lamp_pin, OUTPUT);
    digitalWrite(lamp_pin, LOW);
    pinMode(watchdog_pin, OUTPUT);
    pinMode(bench_pin, INPUT_PULLUP);
    pinMode(park_pin, INPUT_PULLUP);
    pinMode(armed_pin, INPUT_PULLUP);
    pinMode(honda_mil_pin, INPUT_PULLUP);
    Serial.begin(115200);
    Serial2.begin(115200);
    diagnostic_can.begin();
    diagnostic_can.setBaudRate(500000);
    if (Vehicleprofile::ev_telemetry_verified)
    {
        ev_can.begin();
        ev_can.setBaudRate(Vehicleprofile::ev_can_bitrate);
    }
    storage_ok = journal.restore(faults);
    if (!storage_ok)
        faults.storage_failure(0);
    saved_revision = faults.revision();
    last_loop = millis();
    Serial.println(
        "Gateway review build: Honda/EV/lamp vehicle profiles DISABLED; isolated OBD CAN endpoint enabled.");
}
// Service bounded receive batches, then freshness/lamp/persistence tasks; this is not a traction task.
void loop()
{
    uint32_t now = millis();
    Context empty = {};
    faults.set(
        0x1f04, gateway_source, static_cast<uint32_t>(now - last_loop) > 100, now, empty, true);
    last_loop = now;
    // Only accepted physical telemetry supplies production parked/armed/speed evidence.
    bool physical_ev_fresh = ev_seen && static_cast<uint32_t>(now - ev_at) <= 250;
    bool service_ok = Vehicleprofile::service_inputs_verified && physical_ev_fresh;
    obd.service_permission(service_ok && digitalRead(park_pin) == LOW && (latest.flags & 1) &&
                               (latest.context.valid & 1) && latest.context.speed_cms == 0,
                           service_ok && digitalRead(armed_pin) == HIGH && !(latest.flags & 2));
    // A grounded bench contact permits an isolated diagnostic-clear demonstration only.
    if (digitalRead(bench_pin) == LOW && !Vehicleprofile::service_inputs_verified)
        obd.service_permission(digitalRead(park_pin) == LOW, digitalRead(armed_pin) == HIGH);
    for (unsigned i = 0; i < 64; ++i)
    {
        CAN_message_t msg;
        if (!diagnostic_can.read(msg))
            break;
        if (msg.flags.extended || msg.flags.remote || msg.id > 0x7ff)
            continue;
        Canframe f = convert(msg);
        transport.flow(f, micros());
        uint8_t request[7];
        size_t n = 0;
        bool functional = false;
        if (obd_request(f, request, n, functional) && !transport.busy())
        {
            uint8_t response[192];
            size_t len = obd.request(request, n, functional, response, sizeof(response));
            Canframe first;
            if (len && transport.begin(response, len, micros(), first) && !send_obd(first))
            {
                transport.cancel();
                faults.set(0x1f04, gateway_source, true, now, empty, true);
            }
        }
    }
    Canframe next;
    if (transport.next(micros(), next) && !send_obd(next))
    {
        transport.cancel();
        faults.set(0x1f04, gateway_source, true, now, empty, true);
    }
    if (Vehicleprofile::ev_telemetry_verified)
    {
        for (unsigned i = 0; i < 32; ++i)
        {
            CAN_message_t m;
            if (!ev_can.read(m))
                break;
            if (m.flags.extended || m.flags.remote)
                continue;
            if (m.id < 0x650 || m.id > 0x652)
                continue;
            Telemetry t;
            if (telemetry_receiver.receive(convert(m), now, t))
            {
                latest = t;
                ev_seen = true;
                ev_at = now;
                simulated = false;
                faults.update_masks(t.legacy, t.runtime, t.channel_faults, now, t.context);
            }
        }
    }
    for (unsigned i = 0; i < 32 && Serial.available(); ++i)
        if (console_lines.feed(static_cast<char>(Serial.read())))
            console(console_lines.text());
    for (unsigned i = 0; i < 32 && Serial2.available(); ++i)
        if (emulator_lines.feed(static_cast<char>(Serial2.read())))
        {
            unsigned on, timing;
            char extra;
            if (sscanf(emulator_lines.text(), "EMU %u %u %c", &on, &timing, &extra) == 2 &&
                on <= 1 && timing <= 1)
            {
                emulator_seen = true;
                emulator_at = now;
                emulator_healthy = on && !timing;
            }
        }
    if (static_cast<uint32_t>(now - last_ping) >= 100)
    {
        last_ping = now;
        Serial2.println("PING");
        if (Vehicleprofile::honda_verified && physical_ev_fresh &&
            Vehicleprofile::vss_pulses_per_km && (latest.context.valid & 1))
        {
            uint32_t mhz = static_cast<uint32_t>((static_cast<uint64_t>(latest.context.speed_cms) *
                                                  Vehicleprofile::vss_pulses_per_km) /
                                                 100);
            Serial2.print("VSS ");
            Serial2.println(mhz);
        }
        else
            Serial2.println("VSS 0");
        digitalWrite(watchdog_pin, !digitalRead(watchdog_pin));
    }
    honda.tick(now);
    // All three query classes must be fresh; status alone cannot conceal failed code polling.
    honda_seen = true;
    for (unsigned i = 0; i < 3; ++i)
        if (!honda_query_seen[i] || static_cast<uint32_t>(now - honda_query_at[i]) > 2000)
            honda_seen = false;
    if (honda_seen)
        honda_at = now;
    faults.set(0x1f00, gateway_source, !physical_ev_fresh, now, empty, true);
    faults.set(0x1f01,
               gateway_source,
               !honda_seen || static_cast<uint32_t>(now - honda_at) > 2000,
               now,
               empty,
               true);
    faults.set(0x1f02,
               gateway_source,
               !emulator_seen || static_cast<uint32_t>(now - emulator_at) > 500 ||
                   !emulator_healthy,
               now,
               empty,
               true);
    faults.set(0x1f06,
               gateway_source,
               !Vehicleprofile::honda_verified || !Vehicleprofile::ev_telemetry_verified ||
                   !Vehicleprofile::lamp_hardware_verified ||
                   !Vehicleprofile::service_inputs_verified,
               now,
               empty,
               true);
    bool lamp = faults.mil() || honda_mil;
    if (Vehicleprofile::lamp_hardware_verified)
    {
        lamp = lamp || digitalRead(honda_mil_pin) == LOW;
        digitalWrite(lamp_pin, lamp ? HIGH : LOW);
    }
    if (storage_ok && !journal.busy() && saved_revision != faults.revision() &&
        static_cast<uint32_t>(now - last_persist) >= 5000)
    {
        writing_revision = faults.revision();
        storage_ok = journal.start(faults);
        last_persist = now;
    }
    // One verified NV byte per loop limits persistence work; commit completion updates the revision.
    bool writing = journal.busy();
    if (writing && !journal.tick())
    {
        storage_ok = false;
        faults.storage_failure(now);
    }
    else if (writing && !journal.busy())
        saved_revision = writing_revision;
    dump_tick();
    // External hardware watchdog must assert the separate failure warning on a stalled gateway.
}
