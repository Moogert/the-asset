// Offline encoder for the pinned upstream control format; this module never transmits a frame.
#include "prius_inverter_adapter.h"
#include <stddef.h>

uint32_t oi_crc_words(const uint32_t* words, unsigned count)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (unsigned i = 0U; i < count; ++i)
    {
        crc ^= words[i];
        for (unsigned bit = 0U; bit < 32U; ++bit)
            crc = (crc & 0x80000000U) ? (crc << 1U) ^ 0x04C11DB7U : crc << 1U;
    }
    return crc;
}

bool oi_encode_control(const oi_control_t* c, uint8_t data[8])
{
    if (c == NULL || data == NULL || c->throttle_a > 4095U || c->throttle_b > 4095U ||
        c->io_bits > 63U || c->counter > 3U || c->regen_percent > 100U || (c->io_bits & 24U) == 24U)
        return false;
    uint32_t words[2];
    words[0] = (uint32_t)c->throttle_a | ((uint32_t)c->throttle_b << 12U) |
               ((uint32_t)c->io_bits << 24U) | ((uint32_t)c->counter << 30U);
    words[1] = ((uint32_t)c->counter << 14U) | ((uint32_t)c->regen_percent << 16U);
    words[1] |= (oi_crc_words(words, 2U) & 0xFFU) << 24U;
    for (unsigned w = 0U; w < 2U; ++w)
        for (unsigned b = 0U; b < 4U; ++b)
            data[4U * w + b] = (uint8_t)(words[w] >> (8U * b));
    return true;
}
