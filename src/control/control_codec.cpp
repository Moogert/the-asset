#include <control/control_runtime.h>

namespace Capstone
{
namespace
{
// Emulate STM32 word-oriented CRC arithmetic for the pinned OpenInverter payload layout.
uint32_t checksum(const uint32_t words[2])
{
    uint32_t crc = 0XFFFFFFFFU;
    for (unsigned word = 0U; word < 2U; ++word)
    {
        crc ^= words[word];
        for (unsigned bit = 0U; bit < 32U; ++bit)
        {
            crc = ((crc & 0X80000000U) != 0U) ? ((crc << 1U) ^ 0X04C11DB7U) : (crc << 1U);
        }
    }
    return crc;
}
} // namespace

// Validate raw digit ranges before packing; no newton-metre or throttle calibration is inferred.
bool encode(const Rawcommand& command, uint8_t counter, Frame& frame)
{
    const bool valid = (command.throttle_a <= 4095U) && (command.throttle_b <= 4095U) &&
                       (command.io_bits <= 63U) && (counter <= 3U) &&
                       (command.regen_percent <= 100U) && ((command.io_bits & 24U) != 24U);
    for (unsigned byte = 0U; byte < 8U; ++byte)
    {
        frame.data[byte] = 0U;
    }
    if (valid)
    {
        uint32_t words[2] = {0U, 0U};
        words[0] = static_cast<uint32_t>(command.throttle_a) |
                   (static_cast<uint32_t>(command.throttle_b) << 12U) |
                   (static_cast<uint32_t>(command.io_bits) << 24U) |
                   (static_cast<uint32_t>(counter) << 30U);
        words[1] = (static_cast<uint32_t>(counter) << 14U) |
                   (static_cast<uint32_t>(command.regen_percent) << 16U);
        words[1] |= (checksum(words) & 0XFFU) << 24U;
        for (unsigned word = 0U; word < 2U; ++word)
        {
            for (unsigned byte = 0U; byte < 4U; ++byte)
            {
                frame.data[4U * word + byte] = static_cast<uint8_t>(words[word] >> (8U * byte));
            }
        }
    }
    return valid;
}
} // namespace Capstone
