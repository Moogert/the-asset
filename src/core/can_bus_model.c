// Deterministic in-memory CAN simulation and fault injection; no physical CAN driver is called.
#include "can_bus_model.h"
#include <string.h>

static uint32_t next_random(can_bus_t* bus)
{
    bus->prng_state = bus->prng_state * 1664525U + 1013904223U;
    return bus->prng_state;
}

uint8_t can_crc8(const uint8_t* data, size_t length)
{
    uint8_t crc = 0U;
    for (size_t i = 0U; i < length; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            crc = (crc & 0x80U) != 0U ? (uint8_t)((crc << 1U) ^ 0x1DU) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

void can_bus_init(can_bus_t* bus, uint32_t bitrate)
{
    memset(bus, 0, sizeof(*bus));
    bus->bitrate = bitrate == 0U ? 500000U : bitrate;
    bus->prng_state = 0xC0DEC0DEU;
}

void can_bus_set_faults(can_bus_t* bus, can_faults_t faults)
{
    bus->faults = faults;
    bus->bus_off = faults.bus_off;
}

static void trace_frame(can_bus_t* bus, const can_frame_t* frame)
{
    if (bus->trace_count < CAN_TRACE_MAX)
    {
        bus->trace[bus->trace_count++] = *frame;
    }
    else
    {
        memmove(&bus->trace[0], &bus->trace[1], (CAN_TRACE_MAX - 1U) * sizeof(bus->trace[0]));
        bus->trace[CAN_TRACE_MAX - 1U] = *frame;
    }
}

bool can_bus_transmit(can_bus_t* bus, const can_frame_t* frame, uint64_t now_ms)
{
    if (bus->bus_off || bus->faults.bus_off)
    {
        bus->error_count++;
        return false;
    }
    if (frame == NULL || frame->dlc > CAN_DATA_MAX)
    {
        bus->error_count++;
        return false;
    }

    can_frame_t tx = *frame;
    tx.timestamp_ms = now_ms;
    bus->frames_transmitted++;
    trace_frame(bus, &tx);

    if (bus->faults.drop ||
        (bus->faults.drop_percent > 0U && (next_random(bus) % 100U) < bus->faults.drop_percent))
    {
        bus->frames_dropped++;
        return true;
    }
    if (bus->faults.corrupt && tx.dlc > 0U)
    {
        tx.data[next_random(bus) % tx.dlc] ^= (uint8_t)(1U << (next_random(bus) % 8U));
        bus->frames_corrupted++;
    }

    const uint32_t bits = 47U + (uint32_t)tx.dlc * 8U;
    const uint64_t wire_delay = ((uint64_t)bits * 1000ULL) / bus->bitrate;
    const uint64_t deliver_at = now_ms + wire_delay + bus->faults.extra_latency_ms;
    for (size_t copy = 0U; copy < (bus->faults.duplicate ? 2U : 1U); ++copy)
    {
        size_t slot = CAN_PENDING_MAX;
        for (size_t i = 0U; i < CAN_PENDING_MAX; ++i)
        {
            if (!bus->pending[i].used)
            {
                slot = i;
                break;
            }
        }
        if (slot == CAN_PENDING_MAX)
        {
            bus->error_count++;
            return false;
        }
        bus->pending[slot].frame = tx;
        bus->pending[slot].deliver_at_ms = deliver_at + (uint64_t)copy;
        bus->pending[slot].used = true;
        if (copy != 0U)
            bus->duplicate_frames++;
    }
    return true;
}

void can_bus_tick(can_bus_t* bus, uint64_t now_ms)
{
    bus->bus_off = bus->faults.bus_off;
    /* CAN arbitration gives the numerically lowest identifier priority. */
    for (;;)
    {
        size_t selected = CAN_PENDING_MAX;
        for (size_t i = 0U; i < CAN_PENDING_MAX; ++i)
        {
            if (bus->pending[i].used && bus->pending[i].deliver_at_ms <= now_ms &&
                (selected == CAN_PENDING_MAX ||
                 bus->pending[i].frame.id < bus->pending[selected].frame.id))
            {
                selected = i;
            }
        }
        if (selected == CAN_PENDING_MAX)
            break;
        if (bus->delivered_count < CAN_PENDING_MAX)
        {
            bus->delivered[bus->delivered_count++] = bus->pending[selected].frame;
            bus->frames_delivered++;
        }
        else
            bus->error_count++;
        bus->pending[selected].used = false;
    }
    if (bus->bitrate != 0U)
    {
        const uint64_t bits = (uint64_t)bus->frames_transmitted * 111ULL;
        bus->nominal_load_percent =
            (uint32_t)((bits * 100ULL) / ((now_ms + 1ULL) * bus->bitrate / 1000ULL));
        if (bus->nominal_load_percent > 100U)
            bus->nominal_load_percent = 100U;
    }
}

bool can_bus_receive(can_bus_t* bus, can_frame_t* out)
{
    if (bus->delivered_count == 0U || out == NULL)
        return false;
    *out = bus->delivered[0];
    memmove(&bus->delivered[0],
            &bus->delivered[1],
            (bus->delivered_count - 1U) * sizeof(bus->delivered[0]));
    bus->delivered_count--;
    return true;
}
