#ifndef CIVIC_EMULATOR_PINS_H
#define CIVIC_EMULATOR_PINS_H
#include <stdint.h>
// Board-side logic only. Every vehicle-facing signal passes through external conditioning.
namespace Emulatorpins
{
constexpr uint8_t injector_observation = 2, heater_observation = 3;
constexpr uint8_t global_gate = 8, enable_contact = 9, bench_contact = 10, watchdog = 11;
constexpr uint8_t gateway_tx = 18, gateway_rx = 19;
constexpr uint8_t ckp = 22, tdc = 23, cyp = 24, extra_position = 25, vss = 26;
constexpr uint8_t dac_cs[4] = {30, 31, 32, 33};
constexpr uint8_t channel_connect[8] = {34, 35, 36, 37, 38, 39, 40, 41};
constexpr uint8_t ect_select[3] = {42, 43, 44}, iat_select[3] = {45, 46, 47};
constexpr uint8_t spi_mosi = 51, spi_sck = 52;
// The timer ISR uses PORTA directly; moving these pins requires an ISR redesign.
static_assert(ckp == 22 && tdc == 23 && cyp == 24 && extra_position == 25 && vss == 26,
              "Position/VSS pin assignment must match Mega PORTA bits 0..4");
} // namespace Emulatorpins
#endif
