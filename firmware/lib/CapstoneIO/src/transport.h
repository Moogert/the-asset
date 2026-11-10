#ifndef CAPSTONE_TRANSPORT_H
#define CAPSTONE_TRANSPORT_H
// Classic-CAN response transport and bounded Honda parser; timers are microseconds unless named ms.
#include "diagnostics.h"
namespace Civic
{
class Isotp
{
  public:
    Isotp();
    bool begin(const uint8_t* payload, size_t count, uint32_t now_us, Canframe& first);
    void flow(const Canframe& frame, uint32_t now_us);
    bool next(uint32_t now_us, Canframe& output);
    bool busy() const
    {
        return active_;
    }
    bool failed() const
    {
        return failed_;
    }
    void cancel();

  private:
    uint8_t payload_[192], sequence_, block_, block_sent_, waits_;
    uint16_t size_, offset_;
    uint32_t separation_, last_, timeout_; // Microseconds; unsigned subtraction handles wrap.
    bool active_, waiting_, failed_;
};
// Validate an ISO-TP single-frame OBD request. Diagnostic requests here fit in seven bytes.
bool obd_request(const Canframe& frame, uint8_t* payload, size_t& size, bool& functional);
// A UART-level ISO 9141 response parser; physical slow initialization is handled by the board.
struct Hondaresult
{
    uint8_t mode;
    bool mil;
    uint16_t codes[24];
    uint8_t count;
};
class Hondaparser
{
  public:
    Hondaparser() : used_(0), last_(0), overflow_(false)
    {
    }
    void feed(uint8_t byte, uint32_t now_ms);
    // False means incomplete/invalid; caller also enforces a whole-request timeout.
    bool finish(uint32_t now_ms, uint8_t expected_mode, Hondaresult& result);
    void reset();

  private:
    uint8_t bytes_[64], used_;
    uint32_t last_;
    bool overflow_;
};
size_t honda_request(uint8_t mode, uint8_t* out, size_t capacity);
} // namespace Civic
#endif
