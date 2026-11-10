#ifndef CAPSTONE_JOURNAL_H
#define CAPSTONE_JOURNAL_H
// Byte-oriented journal. Real flash erase/repacking power-loss behavior still needs hardware tests.
#include "diagnostics.h"
namespace Civic
{
class Nvport
{
  public:
    virtual ~Nvport()
    {
    }
    virtual uint8_t read(unsigned address) = 0;
    virtual bool write(unsigned address, uint8_t value) = 0;
};
// Two 2048-byte slots. Invalidate target magic first, write body, commit magic last.
class Journal
{
  public:
    explicit Journal(Nvport& port);
    bool restore(Store& store);
    bool start(const Store& store);
    bool tick(); // at most one NV byte write; false means hardware/readback failure
    bool busy() const
    {
        return busy_;
    }

  private:
    Nvport& port_;
    uint8_t image_[2048];
    size_t size_, offset_;
    uint32_t generation_;
    unsigned slot_;
    uint8_t phase_; // 0 invalidate magic, 1 write body/CRC, 2 commit magic last.
    bool busy_;
};
} // namespace Civic
#endif
