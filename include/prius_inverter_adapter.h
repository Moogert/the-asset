#ifndef PRIUS_INVERTER_ADAPTER_H
#define PRIUS_INVERTER_ADAPTER_H
#include <stdbool.h>
#include <stdint.h>

/* OFFLINE encoder, no CAN transmission. Values are throttle digits, never Nm. */
typedef struct
{
    uint16_t throttle_a, throttle_b;
    uint8_t io_bits, counter, regen_percent;
} oi_control_t;
bool oi_encode_control(const oi_control_t* control, uint8_t data[8]);
/* STM32 CRC peripheral: polynomial 0x04C11DB7, init FFFFFFFF, two words. */
uint32_t oi_crc_words(const uint32_t* words, unsigned count);
#endif
