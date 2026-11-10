#ifndef LEAF_BMS_ADAPTER_H
#define LEAF_BMS_ADAPTER_H

#include "can_bus_model.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool valid;
    bool fresh;
    bool contactor_closed;
    bool interlock_ok;
    float pack_voltage_v;
    float pack_current_a;
    float soc_percent;
    float soh_percent;
    float min_cell_v;
    float max_cell_v;
    float cell_delta_mv;
    float min_temp_c;
    float max_temp_c;
    float charge_limit_a;
    float discharge_limit_a;
    uint8_t sequence;
    uint32_t age_ms;
    const char* profile;
} leaf_bms_status_t;

bool leaf_bms_decode(const can_frame_t* frame,
                     uint64_t now_ms,
                     uint8_t* expected_sequence,
                     leaf_bms_status_t* out);

#endif
