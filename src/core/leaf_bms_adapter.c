// Decode the synthetic host BMS profile only. IDs/scales/placeholders are not Nissan LBC telemetry.
#include "leaf_bms_adapter.h"
#include "can_bus_model.h"
#include <string.h>

#define SIM_BMS_ID 0x180U

bool leaf_bms_decode(const can_frame_t* frame,
                     uint64_t now_ms,
                     uint8_t* expected_sequence,
                     leaf_bms_status_t* out)
{
    if (frame == NULL || expected_sequence == NULL || out == NULL)
        return false;
    if (frame->id != SIM_BMS_ID || frame->dlc != 8U)
        return false;
    if (frame->data[7] != can_crc8(frame->data, 7U))
        return false;
    if (*expected_sequence != 0xFFU && frame->sequence != *expected_sequence)
        return false;

    memset(out, 0, sizeof(*out));
    out->valid = true;
    out->fresh = true;
    out->contactor_closed = (frame->data[0] & 0x01U) != 0U;
    out->interlock_ok = (frame->data[0] & 0x02U) != 0U;
    out->charge_limit_a = (float)frame->data[1];
    out->discharge_limit_a = (float)frame->data[2];
    out->min_temp_c = (float)frame->data[3] - 40.0F;
    out->max_temp_c = (float)frame->data[4] - 40.0F;
    out->pack_voltage_v = (float)(((uint16_t)frame->data[5] << 8U) | frame->data[6]) * 2.0F;
    out->cell_delta_mv = 0.0F;
    out->soc_percent = 50.0F;
    out->soh_percent = 90.0F;
    out->sequence = frame->sequence;
    out->age_ms = (uint32_t)(now_ms - frame->timestamp_ms);
    out->profile = "generic_oem_leaf_lbc";
    *expected_sequence = (uint8_t)(frame->sequence + 1U);
    return true;
}
