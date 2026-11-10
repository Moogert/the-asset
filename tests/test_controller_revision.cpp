// Independent synthetic cases; no CAN device, ECU or inverter is accessed.
#include <control/release_control.h>
#include "indication.h"
#include "detail.h"
#include <stdio.h>
#include <string.h>
#include <limits>
static unsigned checks = 0, failures = 0;
#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(condition))                                                                          \
        {                                                                                          \
            ++failures;                                                                            \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition);                                \
        }                                                                                          \
    } while (0)
using namespace Capstone;
static Releaseinput input_at(Timestamp now)
{
    Releaseinput in = Releaseinput();
    in.snapshot.bms_valid = true;
    in.snapshot.bms_received_ms = now;
    in.snapshot.transport_healthy = true;
    in.snapshot.watchdog_healthy = true;
    in.snapshot.energy_ready = true;
    Inputs& p = in.snapshot.policy;
    p.permission = true;
    p.mode = combined;
    p.request_percent = 0.0F;
    p.discharge_limit_a = 200.0F;
    p.charge_limit_a = 50.0F;
    for (unsigned i = 0; i < 2; ++i)
    {
        p.motor[i].valid = true;
        p.motor[i].ready = true;
        p.motor[i].rotor_valid = true;
        p.motor[i].received_ms = now;
        in.power[i].valid = true;
        in.power[i].value = 1000.0F;
        in.power[i].uncertainty = 10.0F;
        in.power[i].acquired_ms = now;
    }
    for (unsigned i = 0; i < temperature_count; ++i)
    {
        in.temperature[i].valid = true;
        in.temperature[i].sensor_id = static_cast<uint16_t>(i + 1);
        in.temperature[i].value = 25.0F;
        in.temperature[i].acquired_ms = now;
    }
    return in;
}
static void thermal_tests()
{
    Releaseconfig cfg = demonstration_release_config();
    Monitorstate s = Monitorstate();
    Releaseinput in = input_at(10);
    Monitorresult r = monitor(cfg, in, 10, s);
    CHECK(r.faults == 0);
    in = input_at(20);
    in.temperature[0].value = 100.0F;
    r = monitor(cfg, in, 20, s);
    CHECK(r.faults & thermal_fault);
    CHECK(r.factor[0] == 0);
    CHECK(r.factor[1] == 1);
    CHECK(s.filtered[0] > 31.81F && s.filtered[0] < 31.83F); // 25 + (100-25)*10/(100+10).
    in = input_at(30);
    in.temperature[0].value = 87.0F;
    CHECK(monitor(cfg, in, 30, s).faults & thermal_fault);
    in = input_at(40);
    in.temperature[0].value = 84.0F;
    CHECK(!(monitor(cfg, in, 40, s).faults & thermal_fault));
    s = Monitorstate();
    in = input_at(50);
    in.temperature[4].value = 80.0F;
    r = monitor(cfg, in, 50, s);
    CHECK(r.factor[0] == 0.5F && r.factor[1] == 0.5F);
    in = input_at(60);
    in.temperature[1].valid = false;
    CHECK(monitor(cfg, in, 60, s).faults & measurement_fault);
    s = Monitorstate();
    in = input_at(70);
    in.temperature[4].valid = false;
    CHECK(monitor(cfg, in, 70, s).faults == 0);
    CHECK(s.count != 0);
    in.temperature[0].value = std::numeric_limits<float>::quiet_NaN();
    CHECK(monitor(cfg, in, 70, s).faults & measurement_fault);
    s = Monitorstate();
    in = input_at(80);
    monitor(cfg, in, 80, s);
    in.temperature[0].acquired_ms = 79;
    CHECK(monitor(cfg, in, 81, s).faults & measurement_fault);
    CHECK(monitor(cfg, in, 79, s).faults & scheduler_fault);
    s = Monitorstate();
    in = input_at(100);
    in.temperature[0].value = 89.0F;
    in.temperature[0].uncertainty = 2;
    CHECK(monitor(cfg, in, 100, s).faults & thermal_fault);
    s = Monitorstate();
    in = input_at(100);
    in.temperature[0].acquired_ms = 20;
    CHECK(monitor(cfg, in, 100, s).faults & measurement_fault);
    s = Monitorstate();
    in = input_at(110);
    in.temperature[0].sensor_id = 2;
    CHECK(monitor(cfg, in, 110, s).faults & measurement_fault);
}
static void circulation_tests()
{
    Releaseconfig cfg = demonstration_release_config();
    Monitorstate s = Monitorstate();
    for (unsigned t = 10; t <= 110; t += 10)
    {
        Releaseinput in = input_at(t);
        in.power[1].value = -1000;
        const Monitorresult r = monitor(cfg, in, t, s);
        CHECK(r.circulation_suspected);
        CHECK(((r.faults & circulation_fault) != 0) == (t == 110));
    }
    Releaseinput in = input_at(120);
    in.power[1].value = -1000;
    in.power[1].acquired_ms = 110;
    Monitorresult r = monitor(cfg, in, 120, s);
    CHECK(r.faults & measurement_fault);
    CHECK(!(r.faults & circulation_fault));
    s = Monitorstate();
    for (unsigned t = 10; t <= 200; t += 10)
    {
        in = input_at(t);
        in.power[1].value = (t % 20) ? -1000.0F : 1000.0F;
        CHECK(!(monitor(cfg, in, t, s).faults & circulation_fault));
    }
    s = Monitorstate();
    in = input_at(210);
    in.power[1].value = -510;
    in.power[1].uncertainty = 20;
    CHECK(!monitor(cfg, in, 210, s).circulation_suspected);
    in.power[1].valid = false;
    cfg.circulation_required = false;
    CHECK(!(monitor(cfg, in, 210, s).faults & measurement_fault));
    s = Monitorstate();
    in = input_at(10);
    in.power[1].value = -1000;
    monitor(cfg, in, 10, s);
    in = input_at(200);
    in.power[1].value = -1000;
    CHECK(!(monitor(cfg, in, 200, s).faults & circulation_fault));
}
class Port : public Releaseport
{
  public:
    bool gated, purge_ok, inhibit_ok, begin_ok;
    unsigned sends, discard_count;
    uint32_t generation;
    int fail_send;
    Transmission tx[2];
    Port()
        : gated(true), purge_ok(true), inhibit_ok(true), begin_ok(true), sends(0), discard_count(0),
          generation(0), fail_send(-1), tx()
    {
    }
    bool send(const Frame&)
    {
        const unsigned index = sends++ % 2;
        return static_cast<int>(index) != fail_send;
    }
    bool physicaloutputs() const
    {
        return false;
    }
    bool setinhibit(bool value)
    {
        if (inhibit_ok)
            gated = value;
        return inhibit_ok;
    }
    bool discardpending()
    {
        ++discard_count;
        return purge_ok;
    }
    bool beginpair(uint32_t value)
    {
        generation = value;
        return begin_ok;
    }
    Transmission transmission(unsigned i, uint32_t value)
    {
        CHECK(value == generation);
        return tx[i];
    }
    bool mapcommand(unsigned, Real effort, Rawcommand& raw)
    {
        raw = Rawcommand();
        raw.throttle_a = static_cast<uint16_t>(effort > 0 ? effort : 0);
        raw.throttle_b = raw.throttle_a;
        return true;
    }
    void kickwatchdog()
    {
    }
};
static void start(Releasestate& state, Port& port, Releaseconfig& cfg)
{
    CHECK(initialize_release(0, state, port));
    Releaseinput in = input_at(10);
    CHECK(run_release(cfg, in, 10, state, port).faults == 0);
}
static Releaseinput driving(unsigned now)
{
    Releaseinput in = input_at(now);
    in.snapshot.policy.arm = true;
    in.snapshot.policy.drive_request = true;
    in.snapshot.policy.request_percent = 10;
    return in;
}
static void dispatch_tests()
{
    Releaseconfig cfg = demonstration_release_config();
    Releasestate s;
    Port p;
    start(s, p, cfg);
    Releasecycle c = run_release(cfg, driving(20), 20, s, p);
    CHECK(c.pending && !c.released && p.gated);
    CHECK(p.sends == 2);
    p.tx[0].complete = p.tx[1].complete = true;
    p.tx[0].completed_ms = 21;
    p.tx[1].completed_ms = 22;
    c = run_release(cfg, driving(22), 22, s, p);
    CHECK(c.released && !c.pending && !p.gated);
    c = run_release(cfg, input_at(23), 23, s, p);
    CHECK(p.gated && !c.released);
    s = Releasestate();
    p = Port();
    start(s, p, cfg);
    p.fail_send = 1;
    c = run_release(cfg, driving(20), 20, s, p);
    CHECK(c.faults & transport_fault);
    CHECK(p.gated);
    s = Releasestate();
    p = Port();
    cfg.production = false;
    start(s, p, cfg);
    run_release(cfg, driving(20), 20, s, p);
    p.tx[0].complete = p.tx[1].complete = true;
    p.tx[0].completed_ms = p.tx[1].completed_ms = 21;
    Releaseinput lost = driving(21);
    lost.snapshot.transport_healthy = false;
    c = run_release(cfg, lost, 21, s, p);
    CHECK(c.faults & transport_fault);
    CHECK(p.gated && !c.released);
    CHECK(p.discard_count > 1);
    s = Releasestate();
    p = Port();
    start(s, p, cfg);
    run_release(cfg, driving(20), 20, s, p);
    c = run_release(cfg, driving(25), 25, s, p);
    CHECK(c.faults & dispatch_fault);
    CHECK(p.gated);
    s = Releasestate();
    p = Port();
    start(s, p, cfg);
    run_release(cfg, driving(20), 20, s, p);
    p.tx[0].complete = p.tx[1].complete = true;
    p.tx[0].completed_ms = 20;
    p.tx[1].completed_ms = 24;
    c = run_release(cfg, driving(24), 24, s, p);
    CHECK(c.faults & dispatch_fault);
    CHECK(p.gated);
    Releaseinput reset = input_at(30);
    reset.snapshot.policy.reset_request = true;
    c = run_release(cfg, reset, 30, s, p);
    CHECK(c.faults == 0);
    CHECK(p.gated);
    p.purge_ok = false;
    reset = input_at(40);
    reset.snapshot.policy.reset_request = true;
    CHECK(run_release(cfg, reset, 40, s, p).faults & inhibit_fault);
    s = Releasestate();
    p = Port();
    cfg.production = true;
    CHECK(initialize_release(0, s, p));
    CHECK(run_release(cfg, input_at(10), 10, s, p).faults & runtime_config_fault);
    CHECK(p.gated);
}
static void indication_tests()
{
    using namespace Civic;
    Indicationprofile cfg = demonstration_indication_profile();
    Engine e(synthetic_profile());
    Indicationreceiver r(cfg);
    char command[96], ack[96];
    CHECK(indication_command(1, 1, 3, command, sizeof(command)) != 0);
    CHECK(r.receive(command, 10, true, e, ack, sizeof(ack)));
    Indicationstatus status = {};
    CHECK(indication_ack(ack, 1, 1, 3, status));
    CHECK(status.applied == 3);
    CHECK(!r.receive(command, 20, true, e, ack, sizeof(ack)));
    CHECK(e.command("ARM BENCH", 10, true));
    CHECK(e.sensors(10).fault[0] == open_circuit && e.sensors(10).fault[1] == open_circuit);
    CHECK(indication_command(1, 2, 2, command, sizeof(command)) != 0);
    CHECK(r.receive(command, 30, true, e, ack, sizeof(ack)));
    CHECK(e.sensors(30).fault[0] == normal && e.sensors(30).fault[1] == open_circuit);
    r.tick(531, e);
    CHECK(!e.armed());
    CHECK(!r.status(531).fresh);
    cfg.production = true;
    Indicationreceiver production(cfg);
    CHECK(production.receive(command, 10, false, e, ack, sizeof(ack)));
    CHECK(production.status(10).applied == 0 && production.status(10).unavailable == 2);
    cfg.mapping[1].electrical_verified = cfg.mapping[1].ecu_verified = true;
    cfg.mapping[1].observed_honda_code = 0x123;
    cfg.mapping[1].observed_delay_ms = 100;
    Indicationreceiver verified(cfg);
    CHECK(verified.receive(command, 10, false, e, ack, sizeof(ack)));
    CHECK(verified.status(10).applied == 2);
    cfg.mapping[1].sensor = cfg.mapping[0].sensor;
    CHECK(!valid_indication_profile(cfg));
    command[4] = '9';
    CHECK(!verified.receive(command, 20, false, e, ack, sizeof(ack)));
    Telemetry t = {};
    t.runtime = (1UL << 5) | (1UL << 7) | (1UL << 22);
    CHECK((fault_categories(t) & 0x23) == 0x23);
    Store store;
    Context context = {};
    store.update_masks(0, (1UL << 11) | (1UL << 22), 0, 0, context);
    bool unknown = false;
    for (unsigned i = 0; i < store.size(); ++i)
        if (store.at(i).code == 0x1f05 && store.at(i).active)
            unknown = true;
    CHECK(!unknown);
    CHECK(!verified.receive("CAT 65537 1 2 2 1", 30, false, e, ack, sizeof(ack)));
    CHECK(!verified.receive("CAT 1 1 2 999999999999 1", 30, false, e, ack, sizeof(ack)));
    CHECK(!verified.receive("CAT -1 1 2 2 1", 30, false, e, ack, sizeof(ack)));
    cfg = demonstration_indication_profile();
    Indicationreceiver wrap(cfg);
    CHECK(indication_command(1, 65535, 255, command, sizeof(command)) > 0);
    CHECK(wrap.receive(command, 0xfffffff0UL, true, e, ack, sizeof(ack)));
    CHECK(wrap.status(0xfffffff0UL).applied == 255);
    CHECK(indication_command(1, 0, 255, command, sizeof(command)) > 0);
    CHECK(wrap.receive(command, 20, true, e, ack, sizeof(ack)));
    CHECK(!wrap.receive(command, 30, true, e, ack, sizeof(ack)));
    wrap.tick(521, e);
    CHECK(!wrap.status(521).fresh && !e.armed());
    CHECK(indication_command(1, 1, 0, command, sizeof(command)) > 0);
    CHECK(wrap.receive(command, 522, true, e, ack, sizeof(ack)));
    CHECK(!e.armed());
}
class Sink : public Civic::Diagnostictransport
{
  public:
    char text[640];
    bool ready;
    Sink() : text(), ready(true)
    {
    }
    bool write_record(const char* bytes, size_t count)
    {
        if (!ready || count >= sizeof(text))
            return false;
        memcpy(text, bytes, count);
        text[count] = 0;
        return true;
    }
};
static void diagnostic_tests()
{
    using namespace Civic;
    Faultdetail d = {};
    d.identity = thermal_fault;
    d.component = 0;
    d.severity = 2;
    d.active = d.valid = true;
    d.measured_milli = -12345;
    d.threshold_milli = 90000;
    d.source_ms = 100;
    Canframe frames[3];
    encode_detail(d, 255, frames);
    Detailreceiver receiver;
    Faultdetail decoded = {};
    CHECK(!receiver.receive(frames[0], 10, decoded));
    CHECK(!receiver.receive(frames[1], 11, decoded));
    CHECK(receiver.receive(frames[2], 12, decoded));
    CHECK(decoded.measured_milli == -12345);
    encode_detail(d, 0, frames);
    CHECK(!receiver.receive(frames[0], 20, decoded));
    CHECK(!receiver.receive(frames[1], 21, decoded));
    CHECK(receiver.receive(frames[2], 22, decoded));
    frames[1].data[0] ^= 1;
    receiver.receive(frames[0], 30, decoded);
    receiver.receive(frames[1], 31, decoded);
    CHECK(!receiver.receive(frames[2], 32, decoded));
    encode_detail(d, 1, frames);
    receiver.receive(frames[0], 40, decoded);
    receiver.receive(frames[1], 41, decoded);
    CHECK(!receiver.receive(frames[2], 61, decoded));
    Fault fault = {};
    fault.code = 0x1f4b;
    fault.active = true;
    Indicationstatus status = {};
    char text[640];
    const size_t n = diagnostic_record(fault, status, false, true, text, sizeof(text));
    CHECK(n > 0 && strstr(text, "synthetic_demo"));
    Sink usb, bluetooth;
    CHECK(publish_record(text, n, usb, &bluetooth));
    CHECK(!strcmp(usb.text, bluetooth.text));
    bluetooth.ready = false;
    CHECK(!publish_record(text, n, usb, &bluetooth));
    CHECK(!diagnostic_record(fault, status, false, true, text, 10));
    Monitorstate records = Monitorstate();
    for (unsigned i = 0; i < record_capacity + 1; ++i)
        recordfault(records, i + 1, shared, shutdown, true, 10, faultvalue(1, 2, true));
    CHECK(records.overflow && records.count == record_capacity);
}
int main()
{
    thermal_tests();
    circulation_tests();
    dispatch_tests();
    indication_tests();
    diagnostic_tests();
    printf("controller revision: %u checks, %u failures (synthetic host evidence only)\n",
           checks,
           failures);
    return failures ? 1 : 0;
}
