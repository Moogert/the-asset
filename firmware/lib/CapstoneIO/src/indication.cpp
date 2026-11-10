#include "indication.h"
#include <stdio.h>
#include <string.h>
namespace Civic
{
// Parse decimal fields without scanf integer narrowing on 16-bit AVR targets.
static bool fields(const char* line, const char* prefix, unsigned count, uint16_t* values)
{
    if (!line || strlen(line) >= 96 || strncmp(line, prefix, strlen(prefix)))
        return false;
    const char* cursor = line + strlen(prefix);
    for (unsigned i = 0; i < count; ++i)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        uint32_t value = 0;
        while (*cursor >= '0' && *cursor <= '9')
        {
            value = value * 10 + static_cast<unsigned>(*cursor++ - '0');
            if (value > 65535UL)
                return false;
        }
        values[i] = static_cast<uint16_t>(value);
        if (i + 1 < count && *cursor++ != ' ')
            return false;
    }
    while (*cursor == '\r' || *cursor == '\n')
        ++cursor;
    return !*cursor;
}
// CRC covers canonical binary fields; human-readable decimal UART text carries the same values.
static uint16_t command_crc(uint16_t revision, uint16_t sequence, uint8_t mask)
{
    const uint8_t data[] = {indication_version,
                            static_cast<uint8_t>(revision),
                            static_cast<uint8_t>(revision >> 8),
                            static_cast<uint8_t>(sequence),
                            static_cast<uint8_t>(sequence >> 8),
                            mask};
    return crc16(data, sizeof(data));
}
static uint16_t ack_crc(
    uint16_t revision, uint16_t sequence, uint8_t requested, uint8_t applied, uint8_t unavailable)
{
    const uint8_t data[] = {indication_version,
                            static_cast<uint8_t>(revision),
                            static_cast<uint8_t>(revision >> 8),
                            static_cast<uint8_t>(sequence),
                            static_cast<uint8_t>(sequence >> 8),
                            requested,
                            applied,
                            unavailable};
    return crc16(data, sizeof(data));
}
Indicationprofile demonstration_indication_profile()
{
    Indicationprofile p = {};
    p.revision = 1;
    for (unsigned i = 0; i < category_count; ++i)
    {
        p.mapping[i].sensor = static_cast<uint8_t>(i);
        p.mapping[i].available = true;
        p.mapping[i].condition = open_circuit;
        p.mapping[i].prerequisites =
            "unverified synthetic bench channel; no Honda code established";
    }
    return p;
}
bool valid_indication_profile(const Indicationprofile& p)
{
    if (!p.revision)
        return false;
    for (unsigned i = 0; i < category_count; ++i)
    {
        const Indicationmapping& m = p.mapping[i];
        if (m.sensor >= 8 || m.condition < open_circuit || m.condition > stuck_high ||
            !m.prerequisites || !m.prerequisites[0])
            return false;
        for (unsigned j = 0; j < i; ++j)
            if (p.mapping[j].sensor == m.sensor)
                return false;
    }
    return true;
}
uint8_t fault_categories(const Telemetry& t)
{
    uint8_t mask = 0;
    const uint32_t b = t.runtime;
    if (b & (1UL << 5))
        mask |= 1U << thermal_category;
    if (b & (1UL << 7))
        mask |= 1U << circulation_category;
    if (b & ((1UL << 2) | (1UL << 10) | (1UL << 11)))
        mask |= 1U << feedback_category;
    if ((b & ((1UL << 3) | (1UL << 4))) || t.channel_faults)
        mask |= 1U << channel_category;
    if (b & ((1UL << 6) | (1UL << 8) | (1UL << 18)))
        mask |= 1U << battery_category;
    if (b & ((1UL << 1) | (1UL << 19) | (1UL << 21) | (1UL << 22)))
        mask |= 1U << communication_category;
    if (b & ((1UL << 9) | (1UL << 17)))
        mask |= 1U << timing_category;
    const uint32_t known = 0x007f0fffUL;
    if ((b & ((1UL << 0) | (1UL << 16) | (1UL << 20))) || (b & ~known) || t.legacy)
        mask |= 1U << configuration_category;
    // Legacy masks remain fully recorded; no unverified semantic category translation is invented.
    return mask;
}
size_t indication_command(
    uint16_t revision, uint16_t sequence, uint8_t mask, char* output, size_t capacity)
{
    const int n = snprintf(output,
                           capacity,
                           "CAT 1 %u %u %u %u\n",
                           static_cast<unsigned>(revision),
                           static_cast<unsigned>(sequence),
                           static_cast<unsigned>(mask),
                           static_cast<unsigned>(command_crc(revision, sequence, mask)));
    return n > 0 && static_cast<size_t>(n) < capacity ? static_cast<size_t>(n) : 0;
}
Indicationreceiver::Indicationreceiver(const Indicationprofile& p)
    : profile_(p), status_(), seen_(false), received_ms_(0)
{
}
bool Indicationreceiver::receive(const char* line,
                                 uint32_t now,
                                 bool bench,
                                 Engine& engine,
                                 char* acknowledgment,
                                 size_t capacity)
{
    uint16_t values[5];
    if (!fields(line, "CAT ", 5, values))
        return false;
    const unsigned v = values[0], revision = values[1], sequence = values[2], mask = values[3],
                   crc = values[4];
    if (!valid_indication_profile(profile_) || v != indication_version ||
        revision != profile_.revision || sequence > 65535U || mask > 255U || crc > 65535U ||
        crc != command_crc(static_cast<uint16_t>(revision),
                           static_cast<uint16_t>(sequence),
                           static_cast<uint8_t>(mask)))
        return false;
    // Duplicate/out-of-order commands cannot renew the heartbeat. Wrap advances normally.
    if (seen_ && (engine.armed() || static_cast<uint32_t>(now - received_ms_) <= 500) &&
        (static_cast<uint16_t>(sequence - status_.sequence) == 0 ||
         static_cast<uint16_t>(sequence - status_.sequence) >= 32768U))
        return false;
    Inject conditions[8] = {};
    uint8_t applied = 0, unavailable = 0;
    for (unsigned i = 0; i < category_count; ++i)
    {
        if (!(mask & (1U << i)))
            continue;
        const Indicationmapping& m = profile_.mapping[i];
        const bool verified = profile_.production && m.electrical_verified && m.ecu_verified &&
                              m.observed_honda_code && m.observed_delay_ms;
        if (m.available && (verified || (!profile_.production && bench)))
        {
            conditions[m.sensor] = m.condition;
            applied |= static_cast<uint8_t>(1U << i);
        }
        else
            unavailable |= static_cast<uint8_t>(1U << i);
    }
    // No Honda history clearing occurs; normal conditions only remove current stimuli.
    engine.apply_indication(conditions);
    status_.sequence = static_cast<uint16_t>(sequence);
    status_.revision = profile_.revision;
    status_.requested = static_cast<uint8_t>(mask);
    status_.applied = applied;
    status_.unavailable = unavailable;
    status_.fresh = true;
    status_.acknowledged = true;
    received_ms_ = now;
    seen_ = true;
    const int n = snprintf(acknowledgment,
                           capacity,
                           "ACK 1 %u %u %u %u %u %u\n",
                           revision,
                           sequence,
                           mask,
                           static_cast<unsigned>(applied),
                           static_cast<unsigned>(unavailable),
                           static_cast<unsigned>(ack_crc(static_cast<uint16_t>(revision),
                                                         static_cast<uint16_t>(sequence),
                                                         static_cast<uint8_t>(mask),
                                                         applied,
                                                         unavailable)));
    return n > 0 && static_cast<size_t>(n) < capacity;
}
void Indicationreceiver::tick(uint32_t now, Engine& engine)
{
    if (seen_ && static_cast<uint32_t>(now - received_ms_) > 500)
    {
        engine.stop();
        status_.fresh = false;
        status_.acknowledged = false;
        // Preserve last requested/applied intent for diagnostics; physical gate is disabled.
    }
}
Indicationstatus Indicationreceiver::status(uint32_t now) const
{
    Indicationstatus s = status_;
    s.fresh = seen_ && static_cast<uint32_t>(now - received_ms_) <= 500;
    s.acknowledged = s.fresh && s.acknowledged;
    return s;
}
bool indication_ack(const char* line,
                    uint16_t revision,
                    uint16_t sequence,
                    uint8_t requested,
                    Indicationstatus& status)
{
    uint16_t values[7];
    if (!fields(line, "ACK ", 7, values))
        return false;
    const unsigned v = values[0], r = values[1], s = values[2], m = values[3], applied = values[4],
                   unavailable = values[5], crc = values[6];
    if (v != 1 || r != revision || s != sequence || m != requested || applied > 255U ||
        unavailable > 255U || (applied & unavailable) || (applied | unavailable) != requested ||
        crc != ack_crc(revision,
                       sequence,
                       requested,
                       static_cast<uint8_t>(applied),
                       static_cast<uint8_t>(unavailable)))
        return false;
    status.sequence = sequence;
    status.revision = revision;
    status.requested = requested;
    status.applied = static_cast<uint8_t>(applied);
    status.unavailable = static_cast<uint8_t>(unavailable);
    status.fresh = true;
    status.acknowledged = true;
    return true;
}
size_t diagnostic_record(const Fault& f,
                         const Indicationstatus& s,
                         bool healthy,
                         bool synthetic,
                         char* out,
                         size_t capacity)
{
    // JSON Lines v1; values are integer engineering units, never interpolated untrusted strings.
    const int n = snprintf(
        out,
        capacity,
        "{\"v\":1,\"evidence\":\"%s\",\"id\":%u,\"source\":%u,\"active\":%u,\"stored\":%u,\"pending\":%u,"
        "\"first_boot_ms\":%lu,\"last_boot_ms\":%lu,\"count\":%u,\"context_valid\":%u,"
        "\"speed_cms\":%u,\"pack_dV\":%u,\"temp_dC\":%d,\"mapping_seq\":%u,\"mapping_revision\":%u,"
        "\"requested\":%u,\"applied\":%u,\"unavailable\":%u,\"ack_fresh\":%u,\"emulator_healthy\":%u}\n",
        synthetic ? "synthetic_demo" : "physical_unverified",
        static_cast<unsigned>(f.code),
        static_cast<unsigned>(f.source),
        f.active ? 1U : 0U,
        f.stored ? 1U : 0U,
        f.pending ? 1U : 0U,
        static_cast<unsigned long>(f.first_ms),
        static_cast<unsigned long>(f.last_ms),
        static_cast<unsigned>(f.occurrences),
        static_cast<unsigned>(f.first_context.valid),
        static_cast<unsigned>(f.first_context.speed_cms),
        static_cast<unsigned>(f.first_context.pack_decivolts),
        static_cast<int>(f.first_context.temperature_decic),
        static_cast<unsigned>(s.sequence),
        static_cast<unsigned>(s.revision),
        static_cast<unsigned>(s.requested),
        static_cast<unsigned>(s.applied),
        static_cast<unsigned>(s.unavailable),
        s.fresh && s.acknowledged ? 1U : 0U,
        healthy ? 1U : 0U);
    return n > 0 && static_cast<size_t>(n) < capacity ? static_cast<size_t>(n) : 0;
}
bool publish_record(const char* bytes,
                    size_t count,
                    Diagnostictransport& usb,
                    Diagnostictransport* bluetooth)
{
    const bool first = usb.write_record(bytes, count);
    const bool second = !bluetooth || bluetooth->write_record(bytes, count);
    return first && second;
}
} // namespace Civic
