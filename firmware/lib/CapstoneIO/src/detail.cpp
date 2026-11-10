#include "detail.h"
#include <string.h>
#include <stdio.h>
namespace Civic
{
static void put32(uint8_t* out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        out[i] = static_cast<uint8_t>(value >> (8 * i));
}
static uint32_t get32(const uint8_t* in)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= static_cast<uint32_t>(in[i]) << (8 * i);
    return value;
}
static int32_t signed32(uint32_t bits)
{
    // Avoid implementation-defined unsigned-to-signed conversion outside INT32_MAX.
    return bits <= 0x7fffffffUL ? static_cast<int32_t>(bits)
                                : -1 - static_cast<int32_t>(0xffffffffUL - bits);
}
void encode_detail(const Faultdetail& d, uint8_t counter, Canframe output[3])
{
    uint8_t bytes[24] = {};
    bytes[0] = 1;
    bytes[1] = counter;
    bytes[2] = d.component;
    bytes[3] = d.severity;
    bytes[4] = static_cast<uint8_t>((d.active ? 1 : 0) | (d.valid ? 2 : 0));
    put32(bytes + 6, d.identity);
    put32(bytes + 10, static_cast<uint32_t>(d.measured_milli));
    put32(bytes + 14, static_cast<uint32_t>(d.threshold_milli));
    put32(bytes + 18, d.source_ms);
    const uint16_t crc = crc16(bytes, 22);
    bytes[22] = static_cast<uint8_t>(crc);
    bytes[23] = static_cast<uint8_t>(crc >> 8);
    for (unsigned i = 0; i < 3; ++i)
    {
        output[i].id = static_cast<uint16_t>(0x660 + i);
        output[i].length = 8;
        memcpy(output[i].data, bytes + 8 * i, 8);
    }
}
Detailreceiver::Detailreceiver() : bytes_(), stage_(0), counter_(0), start_(0), seen_(false)
{
}
bool Detailreceiver::receive(const Canframe& f, uint32_t now, Faultdetail& out)
{
    if (f.length != 8 || f.id < 0x660 || f.id > 0x662)
    {
        stage_ = 0;
        return false;
    }
    if (f.id == 0x660)
    {
        stage_ = 0;
        start_ = now;
    }
    if (f.id != static_cast<uint16_t>(0x660U + stage_) || static_cast<uint32_t>(now - start_) > 20)
    {
        stage_ = 0;
        return false;
    }
    memcpy(bytes_ + stage_ * 8, f.data, 8);
    if (++stage_ != 3)
        return false;
    stage_ = 0;
    const uint16_t crc =
        static_cast<uint16_t>(bytes_[22] | (static_cast<uint16_t>(bytes_[23]) << 8));
    const uint8_t advance = static_cast<uint8_t>(bytes_[1] - counter_);
    if (bytes_[0] != 1 || bytes_[2] > 7 || bytes_[3] > 2 || (bytes_[4] & ~3) || bytes_[5] ||
        crc != crc16(bytes_, 22) || (seen_ && (!advance || advance >= 128)))
        return false;
    counter_ = bytes_[1];
    seen_ = true;
    out.component = bytes_[2];
    out.severity = bytes_[3];
    out.active = bytes_[4] & 1;
    out.valid = bytes_[4] & 2;
    out.identity = get32(bytes_ + 6);
    out.measured_milli = signed32(get32(bytes_ + 10));
    out.threshold_milli = signed32(get32(bytes_ + 14));
    out.source_ms = get32(bytes_ + 18);
    return true;
}
size_t detail_record(
    const Faultdetail& d, const Indicationstatus& s, bool synthetic, char* out, size_t capacity)
{
    const int n = snprintf(
        out,
        capacity,
        "{\"v\":1,\"type\":\"component\",\"evidence\":\"%s\",\"identity\":%lu,\"component\":%u,"
        "\"severity\":%u,\"active\":%u,\"valid\":%u,\"measured_milli\":%ld,\"threshold_milli\":%ld,"
        "\"source_boot_ms\":%lu,\"mapping_seq\":%u,\"mapping_revision\":%u,\"requested\":%u,\"applied\":%u,\"unavailable\":%u}\n",
        synthetic ? "synthetic_demo" : "physical_unverified",
        static_cast<unsigned long>(d.identity),
        static_cast<unsigned>(d.component),
        static_cast<unsigned>(d.severity),
        d.active ? 1U : 0U,
        d.valid ? 1U : 0U,
        static_cast<long>(d.measured_milli),
        static_cast<long>(d.threshold_milli),
        static_cast<unsigned long>(d.source_ms),
        static_cast<unsigned>(s.sequence),
        static_cast<unsigned>(s.revision),
        static_cast<unsigned>(s.requested),
        static_cast<unsigned>(s.applied),
        static_cast<unsigned>(s.unavailable));
    return n > 0 && static_cast<size_t>(n) < capacity ? static_cast<size_t>(n) : 0;
}
} // namespace Civic
