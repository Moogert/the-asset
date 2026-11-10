#include "journal.h"
#include <string.h>
namespace Civic
{
Journal::Journal(Nvport& p)
    : port_(p), image_(), size_(0), offset_(0), generation_(0), slot_(1), phase_(0), busy_(false)
{
}
// Check both slots and choose the newest valid generation; erased and corrupt storage differ.
bool Journal::restore(Store& store)
{
    bool found = false, pristine = true;
    uint32_t newest = 0;
    unsigned selected = 0;
    for (unsigned s = 0; s < 2; ++s)
    {
        for (unsigned i = 0; i < 2048; ++i)
        {
            image_[i] = port_.read(s * 2048 + i);
            if (image_[i] != 0xff)
                pristine = false;
        }
        size_t n = static_cast<size_t>(image_[6]) | (static_cast<size_t>(image_[7]) << 8);
        uint32_t g = 0;
        Store candidate;
        if (n <= 2048 && candidate.load(image_, n, g) &&
            (!found || static_cast<int32_t>(g - newest) > 0))
        {
            found = true;
            newest = g;
            selected = s;
        }
    }
    // A never-written device is acceptable; damaged data without a fallback is a storage fault.
    if (!found)
        return pristine;
    for (unsigned i = 0; i < 2048; ++i)
        image_[i] = port_.read(selected * 2048 + i);
    size_t n = static_cast<size_t>(image_[6]) | (static_cast<size_t>(image_[7]) << 8);
    if (!store.load(image_, n, generation_))
        return false;
    slot_ = selected;
    return true;
}
// Snapshot into RAM and select the alternate slot; writing proceeds incrementally in tick().
bool Journal::start(const Store& store)
{
    if (busy_)
        return false;
    size_ = store.save(image_, sizeof(image_), generation_ + 1);
    if (!size_)
        return false;
    slot_ = 1 - slot_;
    offset_ = 0;
    phase_ = 0;
    busy_ = true;
    return true;
}
// Write/verify at most one byte: invalidate magic, write body/CRC, then commit magic last.
bool Journal::tick()
{
    if (!busy_)
        return true;
    // Phase 0 clears four magic bytes; phase 1 skips them; phase 2 restores them last.
    unsigned index = phase_ == 0   ? static_cast<unsigned>(offset_)
                     : phase_ == 1 ? static_cast<unsigned>(offset_ + 4)
                                   : static_cast<unsigned>(offset_);
    uint8_t value = phase_ == 0 ? 0 : image_[index];
    unsigned address = slot_ * 2048 + index;
    if (!port_.write(address, value) || port_.read(address) != value)
    {
        busy_ = false;
        return false;
    }
    ++offset_;
    if (phase_ == 0 && offset_ == 4)
    {
        phase_ = 1;
        offset_ = 0;
    }
    else if (phase_ == 1 && offset_ == size_ - 4)
    {
        phase_ = 2;
        offset_ = 0;
    }
    else if (phase_ == 2 && offset_ == 4)
    {
        busy_ = false;
        ++generation_;
    }
    return true;
}
} // namespace Civic
