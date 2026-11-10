#include "diagnostics.h"
#include <string.h>
namespace Civic
{
// CRC-16/CCITT-FALSE: polynomial 0x1021, initial 0xFFFF, no reflection or final XOR.
uint16_t crc16(const uint8_t* p, size_t n)
{
    uint16_t c = 0xffff;
    while (n--)
    {
        c ^= static_cast<uint16_t>(*p++) << 8;
        for (unsigned b = 0; b < 8; ++b)
            c = static_cast<uint16_t>((c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1);
    }
    return c;
}
// Serialize little-endian explicitly; never depend on MCU struct padding or byte order.
static void put16(uint8_t* p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
// Serialize a 32-bit value as four little-endian bytes.
static void put32(uint8_t* p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i)
        p[i] = static_cast<uint8_t>(v >> (8 * i));
}
// Reconstruct a little-endian field without unaligned pointer casts.
static uint16_t get16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
// Widen bytes before shifting so 16-bit AVR integer promotion cannot truncate the value.
static uint32_t get32(const uint8_t* p)
{
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i)
        v |= static_cast<uint32_t>(p[i]) << (8 * i);
    return v;
}
Store::Store() : faults_(), count_(0), revision_(0)
{
}
// Track each (code, source) independently and preserve first-event context across reassertions.
void Store::set(uint16_t code,
                Source source,
                bool present,
                uint32_t now,
                const Context& ctx,
                bool confirmed,
                bool pending_only)
{
    if (!code)
        return;
    unsigned i = 0;
    for (; i < count_; ++i)
        if (faults_[i].code == code && faults_[i].source == source)
            break;
    if (i == count_)
    {
        if (!present)
            return;
        // Reclaim inactive never-confirmed entries, reserving one slot for overflow reporting.
        if (count_ >= fault_capacity - 1 && code != 0x1f07)
        {
            for (i = 0; i < count_; ++i)
                if (!faults_[i].active && !faults_[i].stored)
                    break;
            if (i == count_)
            {
                set(0x1f07, gateway_source, true, now, ctx, true);
                return;
            }
        }
        else if (count_ < fault_capacity)
            ++count_;
        else
            return;
        faults_[i] = Fault{};
        faults_[i].code = code;
        faults_[i].source = static_cast<uint8_t>(source);
        faults_[i].first_ms = now;
        faults_[i].first_context = ctx;
        ++revision_;
    }
    Fault& f = faults_[i];
    if (present)
    {
        if (!f.active)
        {
            f.onset_ms = now;
            if (f.occurrences < 65535)
                ++f.occurrences;
            ++revision_;
        }
        f.last_ms = now;
        f.active = true;
        bool stored = f.stored;
        // Project EV confirmation is time-based; original Honda pending codes never use this rule.
        if (confirmed || (!pending_only && static_cast<uint32_t>(now - f.onset_ms) >= 500))
            f.stored = true;
        f.pending = !f.stored;
        if (stored != f.stored)
            ++revision_;
    }
    else if (f.active || f.pending)
    {
        f.active = false;
        f.pending = false;
        ++revision_;
    }
}
// Translate only defined project fault bits; unknown bits raise a mapping fault instead of disappearing.
void Store::update_masks(uint32_t a, uint32_t b, uint8_t channels, uint32_t now, const Context& c)
{
    for (unsigned i = 0; i < 16; ++i)
        set(static_cast<uint16_t>(0x1f10 + i), supervisor_source, (a & (1UL << i)) != 0, now, c);
    const unsigned bits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19, 20, 21, 22};
    uint32_t known = 0;
    for (unsigned i = 0; i < sizeof(bits) / sizeof(bits[0]); ++i)
    {
        unsigned bit = bits[i];
        known |= 1UL << bit;
        set(static_cast<uint16_t>(0x1f40 + bit), runtime_source, (b & (1UL << bit)) != 0, now, c);
    }
    set(0x1f60, mg1_source, (channels & 1) != 0, now, c);
    set(0x1f61, mg2_source, (channels & 2) != 0, now, c);
    set(0x1f05,
        gateway_source,
        (a & 0xffff0000UL) != 0 || (b & ~known) != 0 || (channels & ~3) != 0,
        now,
        c,
        true);
}
// Clear inactive diagnostic history only; this routine cannot change traction-controller state.
bool Store::clear(bool parked, bool disarmed)
{
    if (!parked || !disarmed)
        return false;
    for (unsigned i = 0; i < count_; ++i)
    {
        Fault& f = faults_[i];
        // Clearing history does not make a currently active fault disappear.
        if (!f.active)
        {
            f.stored = false;
            f.pending = false;
        }
    }
    ++revision_;
    return true;
}
// Conservative project warning policy includes retained history, not just current assertions.
bool Store::mil() const
{
    for (unsigned i = 0; i < count_; ++i)
        if (faults_[i].active || faults_[i].stored)
            return true;
    return false;
}
// Generic OBD loses source identity; emit each numeric code once within the caller buffer.
unsigned Store::codes(bool pending, uint16_t* out, unsigned cap) const
{
    unsigned n = 0;
    for (unsigned i = 0; i < count_; ++i)
    {
        const Fault& f = faults_[i];
        if (pending ? !f.pending : !f.stored)
            continue;
        bool duplicate = false;
        for (unsigned j = 0; j < n; ++j)
            if (out[j] == f.code)
                duplicate = true;
        if (!duplicate && n < cap)
            out[n++] = f.code;
    }
    return n;
}
// Record storage loss in RAM even when that fault itself cannot be persisted.
void Store::storage_failure(uint32_t now)
{
    Context c = {};
    set(0x1f03, gateway_source, true, now, c, true);
}
// Versioned serialization: 12-byte header, 26 bytes/record, two-byte CRC. No native struct layout.
// Create a bounded versioned snapshot with a trailing CRC; return zero without writing if too small.
size_t Store::save(uint8_t* p, size_t cap, uint32_t generation) const
{
    size_t n = 14 + count_ * 26;
    if (cap < n)
        return 0;
    memcpy(p, "CVD1", 4);
    p[4] = 1;
    p[5] = static_cast<uint8_t>(count_);
    put16(p + 6, static_cast<uint16_t>(n));
    put32(p + 8, generation);
    for (unsigned i = 0; i < count_; ++i)
    {
        const Fault& f = faults_[i];
        uint8_t* q = p + 12 + i * 26;
        put16(q, f.code);
        q[2] = f.source;
        q[3] = static_cast<uint8_t>((f.stored ? 1 : 0) | (f.active ? 2 : 0));
        put16(q + 4, f.occurrences);
        put32(q + 6, f.first_ms);
        put32(q + 10, f.last_ms);
        put16(q + 14, f.first_context.speed_cms);
        put16(q + 16, f.first_context.pack_decivolts);
        put16(q + 18, static_cast<uint16_t>(f.first_context.temperature_decic));
        q[20] = f.first_context.valid;
        memset(q + 21, 0, 5);
    }
    put16(p + n - 2, crc16(p, n - 2));
    return n;
}
// Validate the complete snapshot first; reject corruption before replacing any live fault records.
bool Store::load(const uint8_t* p, size_t n, uint32_t& generation)
{
    if (n < 14 || memcmp(p, "CVD1", 4) || p[4] != 1 || p[5] > fault_capacity || get16(p + 6) != n ||
        n != 14 + static_cast<size_t>(p[5]) * 26 || get16(p + n - 2) != crc16(p, n - 2))
        return false;
    for (unsigned i = 0; i < p[5]; ++i)
    {
        const uint8_t* q = p + 12 + i * 26;
        if (!get16(q) || q[2] > mg2_source || (q[3] & ~3) || (q[20] & ~7))
            return false;
        for (unsigned j = 0; j < i; ++j)
            if (get16(p + 12 + j * 26) == get16(q) && p[12 + j * 26 + 2] == q[2])
                return false;
    }
    count_ = p[5];
    generation = get32(p + 8);
    for (unsigned i = 0; i < count_; ++i)
    {
        const uint8_t* q = p + 12 + i * 26;
        Fault& f = faults_[i];
        f = Fault{};
        f.code = get16(q);
        f.source = q[2];
        f.stored = (q[3] & 1) != 0;
        // Active state is unknown after restart. A previously active event remains stored evidence.
        f.stored = f.stored || (q[3] & 2) != 0;
        f.occurrences = get16(q + 4);
        f.first_ms = get32(q + 6);
        f.last_ms = get32(q + 10);
        f.first_context =
            Context{get16(q + 14), get16(q + 16), static_cast<int16_t>(get16(q + 18)), q[20]};
    }
    ++revision_;
    return true;
}
// Convert the two-byte DTC encoding to P/C/B/U plus four printable code characters.
void format_code(uint16_t c, char out[6])
{
    const char* h = "0123456789ABCDEF";
    out[0] = "PCBU"[(c >> 14) & 3];
    out[1] = static_cast<char>('0' + ((c >> 12) & 3));
    out[2] = h[(c >> 8) & 15];
    out[3] = h[(c >> 4) & 15];
    out[4] = h[c & 15];
    out[5] = 0;
}
// Return project meanings only; original Honda codes require their own service definitions.
const char* code_description(uint16_t c)
{
    static const char* special[] = {"EV telemetry unavailable/stale",
                                    "Honda diagnostics unavailable/stale",
                                    "Engine emulator unavailable/fault",
                                    "Diagnostic storage failure",
                                    "Gateway timing failure",
                                    "Unknown telemetry fault bits",
                                    "Unverified vehicle interface profile",
                                    "Fault table capacity exceeded"};
    static const char* legacy[] = {"BMS stale",
                                   "Inverter stale",
                                   "Charger stale",
                                   "Control bus off",
                                   "Invalid control frame",
                                   "BMS limit",
                                   "Inverter fault",
                                   "Overcurrent",
                                   "Overspeed",
                                   "HV interlock",
                                   "Emergency stop",
                                   "Contactor/precharge fault",
                                   "Pedal plausibility",
                                   "Charger fault",
                                   "Contradictory request",
                                   "Supervisor configuration"};
    static const char* runtime[] = {"Controller configuration",
                                    "Motor feedback stale",
                                    "Invalid motor feedback",
                                    "Motor channel fault",
                                    "Motor overspeed",
                                    "Motor thermal fault",
                                    "Pack current/limit fault",
                                    "Motor circulation",
                                    "Drive permission",
                                    "Scheduler fault",
                                    "Gearset kinematics"};
    if (c >= 0x1f00 && c <= 0x1f07)
        return special[c - 0x1f00];
    if (c >= 0x1f10 && c <= 0x1f1f)
        return legacy[c - 0x1f10];
    if (c >= 0x1f40 && c <= 0x1f4a)
        return runtime[c - 0x1f40];
    switch (c)
    {
    case 0x1f4b:
        return "Required component measurement unavailable";
    case 0x1f56:
        return "Dual-channel transmission timing failure";
    case 0x1f50:
        return "Runtime configuration";
    case 0x1f51:
        return "Runtime deadline";
    case 0x1f52:
        return "BMS freshness/validity";
    case 0x1f53:
        return "Control transport";
    case 0x1f54:
        return "Effort mapping";
    case 0x1f55:
        return "Inhibit driver";
    case 0x1f60:
        return "MG1 source reports channel fault";
    case 0x1f61:
        return "MG2 source reports channel fault";
    default:
        return "Honda original DTC; consult verified Honda service information";
    }
}
Telemetryreceiver::Telemetryreceiver()
    : bytes_(), stage_(0), last_counter_(0), have_counter_(false), start_(0)
{
}
// Pack the three-frame project snapshot and CRC; this function never sends a control-bus frame.
void encode_telemetry(const Telemetry& t, uint8_t counter, Canframe frames[3])
{
    uint8_t b[24] = {};
    b[0] = 1;
    b[1] = counter;
    b[2] = t.flags;
    put16(b + 3, t.context.speed_cms);
    b[5] = t.context.valid;
    put32(b + 8, t.legacy);
    put32(b + 12, t.runtime);
    put16(b + 16, t.context.pack_decivolts);
    put16(b + 18, static_cast<uint16_t>(t.context.temperature_decic));
    b[20] = t.channel_faults;
    put16(b + 22, crc16(b, 22));
    for (unsigned i = 0; i < 3; ++i)
    {
        frames[i] = Canframe{};
        frames[i].id = static_cast<uint16_t>(0x650 + i);
        frames[i].length = 8;
        memcpy(frames[i].data, b + i * 8, 8);
    }
}
// Accept only a complete ordered/counter-checked triplet; invalid traffic never refreshes source age.
bool Telemetryreceiver::receive(const Canframe& f, uint32_t now, Telemetry& out)
{
    if (f.length != 8)
    {
        stage_ = 0;
        return false;
    }
    if (stage_ && static_cast<uint32_t>(now - start_) > 20)
        stage_ = 0;
    if (f.id == 0x650)
    {
        memcpy(bytes_, f.data, 8);
        stage_ = 1;
        start_ = now;
        return false;
    }
    if (f.id == 0x651 && stage_ == 1)
    {
        memcpy(bytes_ + 8, f.data, 8);
        stage_ = 2;
        return false;
    }
    if (f.id != 0x652 || stage_ != 2)
    {
        stage_ = 0;
        return false;
    }
    stage_ = 0;
    memcpy(bytes_ + 16, f.data, 8);
    if (bytes_[0] != 1 || get16(bytes_ + 22) != crc16(bytes_, 22) || (bytes_[2] & ~3) ||
        (bytes_[5] & ~7) || bytes_[6] || bytes_[7] || bytes_[21])
        return false;
    uint8_t counter = bytes_[1];
    uint8_t delta = static_cast<uint8_t>(counter - last_counter_);
    if (have_counter_ && (delta == 0 || delta > 127))
        return false;
    have_counter_ = true;
    last_counter_ = counter;
    out.flags = bytes_[2];
    out.context.speed_cms = get16(bytes_ + 3);
    out.context.valid = bytes_[5];
    out.legacy = get32(bytes_ + 8);
    out.runtime = get32(bytes_ + 12);
    out.context.pack_decivolts = get16(bytes_ + 16);
    out.context.temperature_decic = static_cast<int16_t>(get16(bytes_ + 18));
    out.channel_faults = bytes_[20];
    return true;
}
// Implement the advertised OBD subset; functional unsupported queries stay silent.
size_t Obd::request(const uint8_t* in, size_t n, bool functional, uint8_t* out, size_t cap)
{
    if (!n || cap < 3)
        return 0;
    uint8_t mode = in[0];
    if (mode == 1 && n == 2)
    {
        if (in[1] == 0 && cap >= 6)
        {
            const uint8_t r[] = {0x41, 0, 0x80, 0, 0, 0};
            memcpy(out, r, 6);
            return 6;
        }
        if (in[1] == 1 && cap >= 6)
        {
            uint16_t codes[fault_capacity];
            unsigned count = store_.codes(false, codes, fault_capacity);
            out[0] = 0x41;
            out[1] = 1;
            out[2] = static_cast<uint8_t>((store_.mil() ? 0x80 : 0) | count);
            // No combustion monitor completion is asserted by the EV gateway.
            out[3] = out[4] = out[5] = 0;
            return 6;
        }
    }
    else if ((mode == 3 || mode == 7) && n == 1)
    {
        uint16_t codes[fault_capacity];
        unsigned count = store_.codes(mode == 7, codes, fault_capacity);
        if (cap < 2 + count * 2)
            return 0;
        out[0] = static_cast<uint8_t>(mode + 0x40);
        out[1] = static_cast<uint8_t>(count);
        for (unsigned i = 0; i < count; ++i)
        {
            out[2 + i * 2] = static_cast<uint8_t>(codes[i] >> 8);
            out[3 + i * 2] = static_cast<uint8_t>(codes[i]);
        }
        return 2 + count * 2;
    }
    else if (mode == 4 && n == 1)
    {
        if (store_.clear(parked_, disarmed_))
        {
            out[0] = 0x44;
            return 1;
        }
        // Refusal must be visible even for functional clear requests.
        // Service refusal is explicit; a reader must not mistake a moving/armed clear for success.
        out[0] = 0x7f;
        out[1] = mode;
        out[2] = 0x22;
        return 3;
    }
    if (functional)
        return 0;
    out[0] = 0x7f;
    out[1] = mode;
    out[2] = (mode == 1 ? 0x12 : 0x11);
    return 3;
}
} // namespace Civic
