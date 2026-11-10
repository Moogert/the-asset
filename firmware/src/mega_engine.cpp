#include <Arduino.h>
#include <SPI.h>
#include <avr/wdt.h>
#include <util/atomic.h>
#include <stdio.h>
#include "engine.h"
#include "vehicle_profile.h"
#include "emulator_pins.h"
using namespace Civic;
static Engine engine(Vehicleprofile::engine_profile());
static Indicationreceiver indication(Vehicleprofile::indication_profile());
static Lines usb_lines, gateway_lines;
static const Outputprofile output_profile = Vehicleprofile::output_profile();
static Outputplan previous_output = {};
static bool io_fault = false;
static const uint8_t gate_pin = Emulatorpins::global_gate,
                     enable_contact = Emulatorpins::enable_contact,
                     bench_contact = Emulatorpins::bench_contact,
                     watchdog_pin = Emulatorpins::watchdog;
static const uint8_t* const dac_cs = Emulatorpins::dac_cs;
// volatile publishes ISR-owned state; ATOMIC_BLOCK is still required for multi-byte access.
static volatile uint32_t vss_half_us = 0;
static volatile bool vss_level = false;
static uint32_t last_status = 0, last_sensors = 0;
static bool was_enabled = false;
static volatile uint32_t injection_rise = 0, injection_at = 0;
static volatile uint16_t injection_width = 0;
static volatile bool injection_started = false;
// ISR: capture one conditioned injector pulse; shared multi-byte values require atomic foreground access.
static void injection_edge()
{
    uint32_t now = micros();
    if (digitalRead(Emulatorpins::injector_observation) == HIGH)
    {
        injection_rise = now;
        injection_started = true;
    }
    else if (injection_started)
    {
        injection_started = false;
        uint32_t width = now - injection_rise;
        if (width > 0 && width <= 20000)
        {
            injection_width = static_cast<uint16_t>(width);
            injection_at = millis();
        }
    }
}
// ISR: schedule position channels on PORTA bits 0..3 while preserving the VSS bit.
ISR(TIMER1_COMPA_vect)
{
    uint8_t levels = 0;
    if (engine.next_edge(micros(), levels))
        PORTA = static_cast<uint8_t>((PORTA & 0xf0) | (levels & 15));
    uint32_t wait = engine.wait_us(micros());
    if (wait > 30000)
        wait = 30000;
    if (wait < 8)
        wait = 8;
    OCR1A = static_cast<uint16_t>(wait * 2 - 1); // 16 MHz / 8 = 0.5 us timer tick
    if (!engine.armed())
    {
        PORTA &= 0xf0;
        digitalWrite(gate_pin, LOW);
    }
}
// ISR: drive VSS on PORTA bit 4; use bounded timer chunks for very slow pulse trains.
ISR(TIMER3_COMPA_vect)
{
    // VSS timer wakes in bounded chunks, allowing low-frequency signals.
    static uint32_t remaining = 0;
    uint32_t half = vss_half_us;
    if (!engine.armed() || !half)
    {
        PORTA &= static_cast<uint8_t>(~16);
        remaining = 0;
        vss_level = false;
        OCR3A = 1999;
        return;
    }
    if (remaining == 0)
    {
        vss_level = !vss_level;
        if (vss_level)
            PORTA |= 16;
        else
            PORTA &= static_cast<uint8_t>(~16);
        remaining = half;
    }
    uint32_t interval = remaining > 30000 ? 30000 : remaining;
    if (interval < 8)
        interval = 8;
    remaining = remaining > interval ? remaining - interval : 0;
    OCR3A = static_cast<uint16_t>(interval * 2 - 1);
}
// Output planner supplies bounded 12-bit codes using the profile reference; calibrate the actual DAC.
static void dac_write(uint8_t ch, uint16_t value)
{
    // MCP4922 command: active output, unity gain, channel select and 12-bit sample.
    uint16_t word = static_cast<uint16_t>(0x3000 | ((ch & 1) ? 0x8000 : 0) | value);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(dac_cs[ch / 2], LOW);
    SPI.transfer16(word);
    digitalWrite(dac_cs[ch / 2], HIGH);
    SPI.endTransaction();
}
// Route bounded serial commands; VSS is in millihertz and protected from 32-bit ISR tearing.
static void command(const char* line, Stream& reply)
{
    bool bench = digitalRead(bench_contact) == LOW;
    if (!strcmp(line, "ARM VEHICLE") && !indication.status(millis()).fresh)
    {
        reply.println(F("ERR fresh category handshake required"));
        return;
    }
    if (!strncmp(line, "CAT ", 4))
    {
        char acknowledgment[96];
        if (indication.receive(
                line, millis(), bench, engine, acknowledgment, sizeof(acknowledgment)))
            reply.print(acknowledgment);
        else
            reply.println(F("ERR category transaction"));
        return;
    }
    unsigned long frequency;
    char extra;
    if (sscanf(line, "VSS %lu %c", &frequency, &extra) == 1)
    {
        if (frequency <= 1000000 && (bench || Vehicleprofile::honda_verified))
        {
            const uint32_t half_period = frequency ? 500000000UL / frequency : 0;
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                // Half-period in us = 1e9 / (2 * frequency_mHz); zero disables VSS.
                vss_half_us = half_period;
            }
            reply.println(F("OK"));
            return;
        }
        reply.println(F("ERR"));
        return;
    }
    if (!strcmp(line, "STATUS"))
    {
        bool armed;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            armed = engine.armed();
        }
        reply.print(F("SIM armed="));
        reply.print(armed ? 1 : 0);
        reply.print(F(" io_fault="));
        reply.print(io_fault ? 1 : 0);
        reply.print(F(" profile="));
        reply.println(output_profile.synthetic ? F("synthetic") : F("vehicle"));
        return;
    }
    Engineinstruction instruction = {};
    if (!engine.prepare_command(line, bench, instruction))
    {
        reply.println(F("ERR"));
        return;
    }
    const uint32_t now = millis();
    bool ok;
    // String parsing/profile scanning occur above, while pulse timer interrupts remain enabled.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        ok = engine.apply_command(instruction, now);
    }
    reply.println(ok ? F("OK") : F("ERR"));
}
// Force output gates low before configuring timers, serial ports or watchdog supervision.
void setup()
{
    MCUSR = 0;
    wdt_disable();
    pinMode(gate_pin, OUTPUT);
    digitalWrite(gate_pin, LOW);
    pinMode(enable_contact, INPUT_PULLUP);
    pinMode(bench_contact, INPUT_PULLUP);
    pinMode(watchdog_pin, OUTPUT);
    pinMode(Emulatorpins::injector_observation, INPUT);
    pinMode(Emulatorpins::heater_observation, INPUT);
    attachInterrupt(
        digitalPinToInterrupt(Emulatorpins::injector_observation), injection_edge, CHANGE);
    for (uint8_t p = 22; p <= 26; ++p)
    {
        pinMode(p, OUTPUT);
        digitalWrite(p, LOW);
    }
    for (uint8_t p = 34; p <= 47; ++p)
    {
        pinMode(p, OUTPUT);
        digitalWrite(p, LOW);
    }
    for (uint8_t i = 0; i < 4; ++i)
    {
        pinMode(dac_cs[i], OUTPUT);
        digitalWrite(dac_cs[i], HIGH);
    }
    SPI.begin();
    Serial.begin(115200);
    Serial1.begin(115200);
    for (uint8_t i = 0; i < 8; ++i)
        dac_write(i, 0);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        // Timer1/3 CTC with prescaler eight: 0.5 us ticks at the board clock of 16 MHz.
        TCCR1A = 0;
        TCCR1B = _BV(WGM12) | _BV(CS11);
        OCR1A = 1999;
        TIMSK1 = _BV(OCIE1A);
        TCCR3A = 0;
        TCCR3B = _BV(WGM32) | _BV(CS31);
        OCR3A = 1999;
        TIMSK3 = _BV(OCIE3A);
    }
    wdt_enable(WDTO_250MS);
    Serial.println(F(
        "SYNTHETIC PROFILE; Honda vehicle outputs unavailable. ARM BENCH requires jumper + enable contact."));
}
// Perform bounded foreground work; hardware interrupts own pulse timing, not USB service intervals.
void loop()
{
    // Bound each UART's work per loop so flooding cannot starve supervision.
    for (unsigned i = 0; i < 32 && Serial.available(); ++i)
        if (usb_lines.feed(static_cast<char>(Serial.read())))
            command(usb_lines.text(), Serial);
    for (unsigned i = 0; i < 32 && Serial1.available(); ++i)
        if (gateway_lines.feed(static_cast<char>(Serial1.read())))
            command(gateway_lines.text(), Serial1);
    uint32_t now = millis();
    bool on;
    Sensors sensors;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        indication.tick(now, engine);
        if (injection_at && static_cast<uint32_t>(now - injection_at) <= 2000)
            engine.observe_actuators(injection_width,
                                     digitalRead(Emulatorpins::heater_observation) == HIGH,
                                     injection_at);
        engine.update(now, digitalRead(enable_contact) == LOW);
        if (output_profile.synthetic && digitalRead(bench_contact) != LOW)
            engine.stop();
        if (io_fault)
            engine.stop();
        on = engine.armed();
    }
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        on = engine.armed();
        digitalWrite(gate_pin, (on && previous_output.enabled) ? HIGH : LOW);
    }
    if (!on && was_enabled)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            PORTA &= 0xe0;
            vss_half_us = 0;
        }
        for (uint8_t p = 34; p <= 41; ++p)
            digitalWrite(p, LOW);
    }
    was_enabled = on;
    if (static_cast<uint32_t>(now - last_sensors) >= 20)
    {
        last_sensors = now;
        Sensorstate sensor_state;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            sensor_state = engine.capture_sensors();
        }
        sensors = calculate_sensors(sensor_state, now);
        const Outputplan next =
            plan_outputs(sensors, output_profile, digitalRead(bench_contact) == LOW);
        io_fault = next.fault;
        // Only resistance selection changes need an open-before-switch transaction.
        for (uint8_t i = 0; i < 8; ++i)
        {
            const bool ladder_change = (i == ect_channel || i == iat_channel) &&
                                       next.resistance_index[i - ect_channel] !=
                                           previous_output.resistance_index[i - ect_channel];
            if (!next.connect[i] || ladder_change)
                digitalWrite(Emulatorpins::channel_connect[i], LOW);
            dac_write(i, next.dac_code[i]);
        }
        for (uint8_t bit = 0; bit < 3; ++bit)
        {
            digitalWrite(Emulatorpins::ect_select[bit],
                         (next.resistance_index[0] & (1 << bit)) ? HIGH : LOW);
            digitalWrite(Emulatorpins::iat_select[bit],
                         (next.resistance_index[1] & (1 << bit)) ? HIGH : LOW);
        }
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            if (next.fault || (!next.enabled && engine.armed()))
                engine.stop();
            on = engine.armed();
            digitalWrite(gate_pin, (on && next.enabled) ? HIGH : LOW);
        }
        for (uint8_t i = 0; i < 8; ++i)
            digitalWrite(Emulatorpins::channel_connect[i], (on && next.connect[i]) ? HIGH : LOW);
        previous_output = next;
    }
    if (static_cast<uint32_t>(now - last_status) >= 100)
    {
        last_status = now;
        bool timing;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            timing = engine.timing_fault() || io_fault;
        }
        Serial1.print(F("EMU "));
        Serial1.print(on ? 1 : 0);
        Serial1.print(' ');
        Serial1.println(timing ? 1 : 0);
        digitalWrite(watchdog_pin, !digitalRead(watchdog_pin));
    }
    wdt_reset();
}
