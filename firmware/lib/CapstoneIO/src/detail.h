#ifndef CIVIC_DETAIL_H
#define CIVIC_DETAIL_H
#include "diagnostics.h"
#include "indication.h"
namespace Civic
{
// Component detail is an optional ordered CAN triplet, separate from legacy mask telemetry.
struct Faultdetail
{
    uint32_t identity, source_ms;
    int32_t measured_milli, threshold_milli;
    uint8_t component, severity;
    bool active, valid;
};
void encode_detail(const Faultdetail& detail, uint8_t counter, Canframe output[3]);
class Detailreceiver
{
  public:
    Detailreceiver();
    bool receive(const Canframe& frame, uint32_t now, Faultdetail& result);

  private:
    uint8_t bytes_[24], stage_, counter_;
    uint32_t start_;
    bool seen_;
};
size_t detail_record(const Faultdetail& detail,
                     const Indicationstatus& status,
                     bool synthetic,
                     char* output,
                     size_t capacity);
} // namespace Civic
#endif
