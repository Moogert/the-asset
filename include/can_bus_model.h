#ifndef CAN_BUS_MODEL_H
#define CAN_BUS_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAN_DATA_MAX 8U
#define CAN_PENDING_MAX 128U
#define CAN_TRACE_MAX 256U

typedef enum
{
    CAN_NODE_BMS = 0,
    CAN_NODE_INVERTER,
    CAN_NODE_CHARGER,
    CAN_NODE_SUPERVISOR,
    CAN_NODE_GUI,
    CAN_NODE_MG1,
    CAN_NODE_MG2
} can_node_t;

typedef struct
{
    uint32_t id;
    uint8_t dlc;
    uint8_t data[CAN_DATA_MAX];
    can_node_t source;
    uint8_t sequence;
    uint64_t timestamp_ms;
} can_frame_t;

typedef struct
{
    bool drop;
    bool duplicate;
    bool corrupt;
    bool replay;
    bool bus_off;
    uint8_t drop_percent;
    uint32_t extra_latency_ms;
} can_faults_t;

typedef struct
{
    can_frame_t frame;
    uint64_t deliver_at_ms;
    bool used;
} can_pending_t;

typedef struct
{
    uint32_t bitrate;
    uint32_t nominal_load_percent;
    uint32_t frames_transmitted;
    uint32_t frames_dropped;
    uint32_t frames_corrupted;
    uint32_t frames_delivered;
    uint32_t duplicate_frames;
    uint32_t error_count;
    bool bus_off;
    can_faults_t faults;
    uint32_t prng_state;
    can_pending_t pending[CAN_PENDING_MAX];
    can_frame_t delivered[CAN_PENDING_MAX];
    size_t delivered_count;
    can_frame_t trace[CAN_TRACE_MAX];
    size_t trace_count;
} can_bus_t;

void can_bus_init(can_bus_t* bus, uint32_t bitrate);
void can_bus_set_faults(can_bus_t* bus, can_faults_t faults);
bool can_bus_transmit(can_bus_t* bus, const can_frame_t* frame, uint64_t now_ms);
void can_bus_tick(can_bus_t* bus, uint64_t now_ms);
bool can_bus_receive(can_bus_t* bus, can_frame_t* out);
uint8_t can_crc8(const uint8_t* data, size_t length);

#endif
