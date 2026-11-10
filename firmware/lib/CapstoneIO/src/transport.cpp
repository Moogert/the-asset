#include "transport.h"
#include <string.h>
namespace Civic
{
Isotp::Isotp()
    : payload_(), sequence_(1), block_(0), block_sent_(0), waits_(0), size_(0), offset_(0),
      separation_(0), last_(0), timeout_(0), active_(false), waiting_(false), failed_(false)
{
}
// Abandon the active transfer without changing fault history or traction state.
void Isotp::cancel()
{
    active_ = false;
    waiting_ = false;
}
// Use a single frame for up to seven bytes; larger responses await tester flow control.
bool Isotp::begin(const uint8_t* p, size_t n, uint32_t now, Canframe& f)
{
    if (active_ || !n || n > sizeof(payload_))
        return false;
    failed_ = false;
    f = Canframe{};
    f.id = 0x7e8;
    f.length = 8;
    if (n <= 7)
    {
        f.data[0] = static_cast<uint8_t>(n);
        memcpy(f.data + 1, p, n);
        return true;
    }
    memcpy(payload_, p, n);
    size_ = static_cast<uint16_t>(n);
    offset_ = 6;
    sequence_ = 1;
    block_sent_ = 0;
    waits_ = 0;
    f.data[0] = static_cast<uint8_t>(0x10 | (n >> 8));
    f.data[1] = static_cast<uint8_t>(n);
    memcpy(f.data + 2, p, 6);
    active_ = true;
    waiting_ = true;
    timeout_ = now;
    return true;
}
// Honor flow-control status, block size and STmin; reserved encodings terminate the transfer.
void Isotp::flow(const Canframe& f, uint32_t now)
{
    if (!active_ || !waiting_ || f.id != 0x7e0 || f.length < 3 || f.length > 8 ||
        (f.data[0] & 0xf0) != 0x30)
        return;
    uint8_t type = f.data[0] & 15;
    if (type == 2)
    {
        failed_ = true;
        cancel();
        return;
    }
    if (type == 1)
    {
        if (++waits_ > 3)
        {
            failed_ = true;
            cancel();
        }
        else
            timeout_ = now;
        return;
    }
    if (type != 0)
    {
        failed_ = true;
        cancel();
        return;
    }
    uint8_t s = f.data[2];
    if (s <= 0x7f)
        separation_ = static_cast<uint32_t>(s) * 1000;
    else if (s >= 0xf1 && s <= 0xf9)
        separation_ = static_cast<uint32_t>(s - 0xf0) * 100;
    else
    {
        failed_ = true;
        cancel();
        return;
    }
    block_ = f.data[1];
    block_sent_ = 0;
    waiting_ = false;
    last_ = now;
    timeout_ = now;
}
// Emit one consecutive frame per call; a stalled tester expires after one second.
bool Isotp::next(uint32_t now, Canframe& f)
{
    if (!active_)
        return false;
    if (static_cast<uint32_t>(now - timeout_) >= 1000000)
    {
        failed_ = true;
        cancel();
        return false;
    }
    if (waiting_ || static_cast<uint32_t>(now - last_) < separation_)
        return false;
    f = Canframe{};
    f.id = 0x7e8;
    f.length = 8;
    f.data[0] = static_cast<uint8_t>(0x20 | sequence_);
    unsigned n = size_ - offset_;
    if (n > 7)
        n = 7;
    memcpy(f.data + 1, payload_ + offset_, n);
    offset_ = static_cast<uint16_t>(offset_ + n);
    sequence_ = static_cast<uint8_t>((sequence_ + 1) & 15);
    last_ = now;
    timeout_ = now;
    if (offset_ >= size_)
        cancel();
    else if (block_ && ++block_sent_ >= block_)
        waiting_ = true;
    return true;
}
// Accept only bounded single-frame application requests on the gateway diagnostic addresses.
bool obd_request(const Canframe& f, uint8_t* p, size_t& n, bool& functional)
{
    if ((f.id != 0x7df && f.id != 0x7e0) || !f.length || f.length > 8)
        return false;
    uint8_t len = f.data[0];
    if (!len || len > 7 || len + 1 > f.length)
        return false;
    n = len;
    functional = f.id == 0x7df;
    memcpy(p, f.data + 1, len);
    return true;
}
// Discard a partial reply before beginning a different diagnostic transaction.
void Hondaparser::reset()
{
    used_ = 0;
    overflow_ = false;
}
// Bound receive storage and separate frames using the template inter-byte timeout.
void Hondaparser::feed(uint8_t b, uint32_t now)
{
    if (used_ && static_cast<uint32_t>(now - last_) > 25)
    {
        used_ = 0;
        overflow_ = false;
    }
    last_ = now;
    if (used_ < sizeof(bytes_))
        bytes_[used_++] = b;
    else
        overflow_ = true;
}
// Require matching service/header and additive checksum after an end-of-frame quiet interval.
bool Hondaparser::finish(uint32_t now, uint8_t expected, Hondaresult& out)
{
    if (!used_ || static_cast<uint32_t>(now - last_) < 25)
        return false;
    uint8_t n = used_;
    used_ = 0;
    if (overflow_)
    {
        overflow_ = false;
        return false;
    }
    uint8_t sum = 0;
    for (unsigned i = 0; i + 1 < static_cast<unsigned>(n); ++i)
        sum = static_cast<uint8_t>(sum + bytes_[i]);
    // Template ISO 9141 OBD response header. Exact ECU/profile must be verified before connection.
    if (n < 5 || bytes_[0] != 0x48 || bytes_[1] != 0x6b || bytes_[2] != 0x10 ||
        sum != bytes_[n - 1] || bytes_[3] != expected + 0x40)
        return false;
    out = Hondaresult{};
    out.mode = expected;
    if (expected == 1)
    {
        if (n != 10 || bytes_[4] != 1)
            return false;
        out.mil = (bytes_[5] & 0x80) != 0;
        return true;
    }
    if (expected != 3 && expected != 7)
        return false;
    if ((n - 5) % 2)
        return false;
    for (unsigned i = 4; i + 2 < static_cast<unsigned>(n); i += 2)
    {
        uint16_t code =
            static_cast<uint16_t>((static_cast<uint16_t>(bytes_[i]) << 8) | bytes_[i + 1]);
        if (code && out.count < 24)
            out.codes[out.count++] = code;
    }
    return true;
}
// Build only the three implemented read services; no Honda clear/control request is generated.
size_t honda_request(uint8_t mode, uint8_t* out, size_t cap)
{
    size_t n = mode == 1 ? 6 : 5;
    if ((mode != 1 && mode != 3 && mode != 7) || cap < n)
        return 0;
    out[0] = 0x68;
    out[1] = 0x6a;
    out[2] = 0xf1;
    out[3] = mode;
    if (mode == 1)
        out[4] = 1;
    uint8_t sum = 0;
    for (unsigned i = 0; i < n - 1; ++i)
        sum = static_cast<uint8_t>(sum + out[i]);
    out[n - 1] = sum;
    return n;
}
} // namespace Civic
