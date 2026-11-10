#include "engine.h"
#include "transport.h"
#include "journal.h"
#include <diagnostics/supervisor_bridge.h>
#include <stdio.h>
#include <string.h>
#include <vector>
using namespace Civic;
static unsigned checks = 0, failures = 0;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(x))                                                                                  \
        {                                                                                          \
            ++failures;                                                                            \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                   \
        }                                                                                          \
    } while (0)
static void engine_tests()
{
    Engineprofile p = synthetic_profile();
    CHECK(valid_profile(p));
    CHECK(p.count == 48);
    Engineprofile bad = p;
    bad.edges[1].angle = 0;
    CHECK(!valid_profile(bad));
    bad = p;
    bad.verified = true;
    CHECK(!valid_profile(bad));
    Engine e(p);
    CHECK(!e.command("ARM VEHICLE", 0, true));
    CHECK(!e.command("ARM BENCH", 0, false));
    CHECK(e.command("ARM BENCH", 0, true));
    CHECK(e.command("RPM 1000", 0, true));
    CHECK(e.command("STATE RUN", 0, true));
    uint32_t now = 0xfffffff0UL;
    uint8_t levels = 0, previous = 0;
    unsigned rises[3] = {};
    // Independent expected 24/4/1 rising transitions per 120 ms cycle at 1000 rpm.
    for (unsigned i = 0; i < p.count; ++i)
    {
        CHECK(e.next_edge(now, levels));
        for (unsigned b = 0; b < 3; ++b)
            if ((levels & (1 << b)) && !(previous & (1 << b)))
                ++rises[b];
        previous = levels;
        now += e.wait_us(now);
    }
    CHECK(rises[0] == 24);
    CHECK(rises[1] == 4);
    CHECK(rises[2] == 1);
    CHECK(now == static_cast<uint32_t>(0xfffffff0UL + 120000UL));
    CHECK(e.next_edge(now, levels));
    CHECK(e.wait_us(now) == 2500);
    CHECK(!e.command("RPM 0", 0, true));
    CHECK(!e.command("RPM 8001", 0, true));
    CHECK(!e.command("RPM 1000junk", 0, true));
    CHECK(!e.command("FAULT 8 OPEN", 0, true));
    CHECK(!e.command("STATE TURBO", 0, true));
    CHECK(e.command("FAULT 0 OPEN", 0, true));
    CHECK(e.sensors(0).fault[0] == open_circuit);
    CHECK(e.command("THROTTLE 100", 0, true));
    CHECK(e.sensors(0).millivolts[0] == 4500);
    e.observe_actuators(4000, true, 20);
    CHECK(e.sensors(21).actuator_feedback_valid);
    CHECK(e.sensors(21).millivolts[2] == 800);
    e.observe_actuators(1000, true, 30);
    CHECK(e.sensors(31).millivolts[2] == 200);
    CHECK(!e.sensors(2100).actuator_feedback_valid);
    e.update(501, true);
    CHECK(!e.armed());
    CHECK(e.command("ARM BENCH", 0xfffffff0UL, true));
    e.update(100, true);
    CHECK(e.armed());
    e.update(600, true);
    CHECK(!e.armed());
    CHECK(e.command("STATE RUN", 0, true));
    CHECK(e.command("ARM BENCH", 0, true));
    e.next_edge(0, levels);
    e.next_edge(10000, levels);
    CHECK(!e.armed());
    CHECK(e.timing_fault());
    CHECK(e.command("ARM BENCH", 0, true));
    e.update(0, false);
    CHECK(!e.armed());
    Lines lines;
    for (unsigned i = 0; i < 130; ++i)
        CHECK(!lines.feed('X'));
    CHECK(!lines.feed('\n'));
    CHECK(!lines.feed('O'));
    CHECK(!lines.feed('K'));
    CHECK(lines.feed('\n'));
    CHECK(!strcmp(lines.text(), "OK"));
    for (unsigned i = 0; i < 2000; ++i)
    {
        char command[40];
        snprintf(command, sizeof(command), "RPM %u garbage", i);
        CHECK(!e.command(command, 0, true));
    }
}
struct Memory : Nvport
{
    uint8_t b[4096];
    bool fail;
    Memory() : b(), fail(false)
    {
        memset(b, 0xff, sizeof(b));
    }
    uint8_t read(unsigned a) override
    {
        return b[a];
    }
    bool write(unsigned a, uint8_t v) override
    {
        if (fail)
            return false;
        b[a] = v;
        return true;
    }
};
static void store_tests()
{
    Store s;
    Context c = {123, 3500, 750, 7};
    s.set(0x1f10, supervisor_source, true, 10, c);
    CHECK(s.mil());
    uint16_t codes[64];
    CHECK(s.codes(true, codes, 64) == 1);
    CHECK(s.codes(false, codes, 64) == 0);
    s.set(0x1f10, supervisor_source, true, 510, c);
    CHECK(s.codes(false, codes, 64) == 1);
    CHECK(s.at(0).occurrences == 1);
    CHECK(!s.clear(false, true));
    CHECK(s.clear(true, true));
    CHECK(s.codes(false, codes, 64) == 1);
    s.set(0x1f10, supervisor_source, false, 600, c);
    CHECK(s.mil());
    CHECK(s.clear(true, true));
    CHECK(!s.mil());
    s.set(0x133, honda_source, true, 1000, c, false, true);
    s.set(0x133, honda_source, true, 2000, c, false, true);
    CHECK(s.codes(true, codes, 64) == 1);
    CHECK(s.codes(false, codes, 64) == 0);
    s.set(0x133, honda_source, true, 2100, c, true);
    CHECK(s.codes(false, codes, 64) == 1);
    s.set(0x133, supervisor_source, true, 2200, c, true);
    CHECK(s.codes(false, codes, 64) == 1); // duplicate code, distinct USB source records
    uint8_t bytes[2048];
    size_t n = s.save(bytes, sizeof(bytes), 42);
    CHECK(n > 0);
    Store restored;
    uint32_t g = 0;
    CHECK(restored.load(bytes, n, g));
    CHECK(g == 42);
    CHECK(restored.at(0).first_context.pack_decivolts == 3500);
    bytes[20] ^= 1;
    CHECK(!restored.load(bytes, n, g));
    char text[6];
    format_code(0x1f55, text);
    CHECK(!strcmp(text, "P1F55"));
    format_code(0xc123, text);
    CHECK(!strcmp(text, "U0123"));
    CHECK(strcmp(code_description(0x1f55), "Inhibit driver") == 0);
    Store all;
    all.update_masks(0xffff, 0x003f07ff, 3, 0, c);
    all.update_masks(0xffff, 0x003f07ff, 3, 500, c);
    CHECK(all.codes(false, codes, 64) == 35); // 16 legacy + 17 runtime + 2 channel
    all.update_masks(0x80000000, 0, 0, 600, c);
    CHECK(all.at(all.size() - 1).code == 0x1f05);
    Store capacity;
    for (unsigned i = 1; i < 100; ++i)
        capacity.set(static_cast<uint16_t>(i), honda_source, true, 0, c, true);
    CHECK(capacity.size() == 64);
    CHECK(capacity.at(63).code == 0x1f07);
    // Interrupted writes preserve the older slot. Exercise every interrupted byte position.
    Memory mem;
    Journal journal(mem);
    Store initial;
    CHECK(journal.restore(initial));
    initial.set(0x1f10, supervisor_source, true, 0, c, true);
    CHECK(journal.start(initial));
    while (journal.busy())
        CHECK(journal.tick());
    uint8_t baseline[4096];
    memcpy(baseline, mem.b, 4096);
    for (unsigned stop = 0; stop < 75; ++stop)
    {
        memcpy(mem.b, baseline, 4096);
        Store loaded;
        Journal j(mem);
        CHECK(j.restore(loaded));
        loaded.set(0x1f11, supervisor_source, true, 0, c, true);
        CHECK(j.start(loaded));
        for (unsigned i = 0; i < stop && j.busy(); ++i)
            CHECK(j.tick());
        Store after;
        Journal reboot(mem);
        CHECK(reboot.restore(after));
        CHECK(after.size() == 1 || after.size() == 2);
    }
    memcpy(mem.b, baseline, 4096);
    Journal j(mem);
    Store load;
    CHECK(j.restore(load));
    CHECK(j.start(load));
    mem.fail = true;
    CHECK(!j.tick());
    Memory corrupt;
    corrupt.b[0] = 0;
    Journal corrupt_j(corrupt);
    Store missing;
    CHECK(!corrupt_j.restore(missing));
}
static void obd_tests()
{
    Store s;
    Context c = {};
    Obd o(s);
    uint8_t out[192];
    uint8_t supported[] = {1, 0};
    CHECK(o.request(supported, 2, true, out, sizeof(out)) == 6);
    CHECK(out[2] == 0x80 && out[3] == 0);
    uint8_t query[] = {3};
    CHECK(o.request(query, 1, true, out, sizeof(out)) == 2);
    CHECK(out[0] == 0x43 && out[1] == 0);
    for (unsigned i = 0; i < 8; ++i)
        s.set(static_cast<uint16_t>(0x1f10 + i), supervisor_source, true, 0, c, true);
    CHECK(o.request(query, 1, true, out, sizeof(out)) == 18);
    CHECK(out[1] == 8 && out[2] == 0x1f && out[3] == 0x10);
    uint8_t status[] = {1, 1};
    CHECK(o.request(status, 2, true, out, sizeof(out)) == 6);
    CHECK(out[2] == 0x88);
    CHECK(out[3] == 0 && out[4] == 0 && out[5] == 0);
    uint8_t clear[] = {4};
    CHECK(o.request(clear, 1, true, out, sizeof(out)) == 3);
    CHECK(out[2] == 0x22);
    o.service_permission(true, true);
    CHECK(o.request(clear, 1, true, out, sizeof(out)) == 1);
    CHECK(out[0] == 0x44);
    CHECK(s.mil());
    uint8_t unknown[] = {0x0a};
    CHECK(o.request(unknown, 1, true, out, sizeof(out)) == 0);
    CHECK(o.request(unknown, 1, false, out, sizeof(out)) == 3);
    uint8_t truncated[] = {1};
    CHECK(o.request(truncated, 1, true, out, sizeof(out)) == 0);
    Canframe f = {};
    f.id = 0x7df;
    f.length = 8;
    f.data[0] = 2;
    f.data[1] = 1;
    f.data[2] = 1;
    uint8_t p[7];
    size_t n = 0;
    bool functional;
    CHECK(obd_request(f, p, n, functional) && functional && n == 2);
    f.data[0] = 0x10;
    CHECK(!obd_request(f, p, n, functional));
    for (unsigned len = 0; len < 256; ++len)
    {
        f.length = static_cast<uint8_t>(len);
        f.data[0] = 7;
        bool valid = obd_request(f, p, n, functional);
        CHECK(valid == (len == 8));
    }
}
static void transport_tests()
{
    uint8_t payload[130];
    for (unsigned i = 0; i < 130; ++i)
        payload[i] = static_cast<uint8_t>(i);
    Isotp tx;
    Canframe f;
    uint32_t now = 0xffffff00UL;
    CHECK(tx.begin(payload, 130, now, f));
    CHECK(f.data[0] == 0x10 && f.data[1] == 130);
    CHECK(!tx.next(now, f));
    Canframe fc = {};
    fc.id = 0x7e0;
    fc.length = 8;
    fc.data[0] = 0x30;
    fc.data[1] = 0;
    fc.data[2] = 0xf1;
    tx.flow(fc, now);
    std::vector<uint8_t> recovered(payload, payload + 6);
    unsigned seq = 1;
    while (tx.busy())
    {
        now += 100;
        CHECK(tx.next(now, f));
        CHECK(f.data[0] == (0x20 | (seq++ & 15)));
        for (unsigned j = 1; j < 8 && recovered.size() < 130; ++j)
            recovered.push_back(f.data[j]);
    }
    CHECK(!memcmp(recovered.data(), payload, 130));
    CHECK(tx.begin(payload, 20, 0, f));
    CHECK(!tx.next(1000000, f));
    CHECK(tx.failed());
    CHECK(tx.begin(payload, 20, 0, f));
    fc.data[2] = 0x80;
    tx.flow(fc, 0);
    CHECK(tx.failed() && !tx.busy());
    CHECK(tx.begin(payload, 20, 0, f));
    fc.data[2] = 0;
    fc.data[1] = 1;
    tx.flow(fc, 0);
    CHECK(tx.next(0, f));
    CHECK(!tx.next(100, f));
    tx.flow(fc, 100);
    CHECK(tx.next(100, f));
    CHECK(!tx.busy());
    CHECK(tx.begin(payload, 20, 0, f));
    fc.data[0] = 0x31;
    for (unsigned i = 0; i < 4; ++i)
        tx.flow(fc, 0);
    CHECK(tx.failed());
    Hondaparser parser;
    Hondaresult result;
    uint8_t h[] = {0x48, 0x6b, 0x10, 0x43, 0x01, 0x33, 0};
    uint8_t sum = 0;
    for (unsigned i = 0; i < 6; ++i)
        sum = static_cast<uint8_t>(sum + h[i]);
    h[6] = sum;
    for (unsigned i = 0; i < 7; ++i)
        parser.feed(h[i], i);
    CHECK(!parser.finish(20, 3, result));
    CHECK(parser.finish(40, 3, result));
    CHECK(result.count == 1 && result.codes[0] == 0x133);
    h[6] ^= 1;
    for (unsigned i = 0; i < 7; ++i)
        parser.feed(h[i], 100 + i);
    CHECK(!parser.finish(140, 3, result));
    for (unsigned i = 0; i < 100; ++i)
        parser.feed(0, 200 + i);
    CHECK(!parser.finish(350, 3, result));
}
static void telemetry_tests()
{
    Capstone::Cycle cycle = {};
    cycle.faults = Capstone::inhibit_fault | Capstone::speed_fault;
    Context context = {0, 3500, 600, 7};
    Telemetry source = diagnostic_snapshot(1, cycle, context, true, false, true, false);
    Canframe encoded[3];
    encode_telemetry(source, 7, encoded);
    Telemetryreceiver bridge_receiver;
    Telemetry bridged;
    CHECK(!bridge_receiver.receive(encoded[0], 0, bridged));
    CHECK(!bridge_receiver.receive(encoded[1], 1, bridged));
    CHECK(bridge_receiver.receive(encoded[2], 2, bridged));
    CHECK(bridged.runtime == cycle.faults && bridged.channel_faults == 1 && bridged.flags == 1);
    uint8_t p[24] = {1, 255, 1, 0, 0,    7,    0,    0, 1, 0, 0, 0,
                     0, 0,   0, 0, 0xac, 0x0d, 0xee, 2, 1, 0, 0, 0};
    uint16_t crc = crc16(p, 22);
    p[22] = static_cast<uint8_t>(crc);
    p[23] = static_cast<uint8_t>(crc >> 8);
    Telemetryreceiver rx;
    Telemetry t;
    Canframe f = {};
    f.length = 8;
    for (unsigned i = 0; i < 3; ++i)
    {
        f.id = static_cast<uint16_t>(0x650 + i);
        memcpy(f.data, p + i * 8, 8);
        CHECK(rx.receive(f, i, t) == (i == 2));
    }
    CHECK(t.legacy == 1 && t.context.pack_decivolts == 3500 && t.channel_faults == 1);
    // Replay rejected. Counter wraps 255 -> 0. Corruption and assembly timeout rejected.
    for (unsigned i = 0; i < 3; ++i)
    {
        f.id = static_cast<uint16_t>(0x650 + i);
        memcpy(f.data, p + i * 8, 8);
        CHECK(!rx.receive(f, i + 5, t));
    }
    p[1] = 0;
    crc = crc16(p, 22);
    p[22] = static_cast<uint8_t>(crc);
    p[23] = static_cast<uint8_t>(crc >> 8);
    for (unsigned i = 0; i < 3; ++i)
    {
        f.id = static_cast<uint16_t>(0x650 + i);
        memcpy(f.data, p + i * 8, 8);
        CHECK(rx.receive(f, i + 10, t) == (i == 2));
    }
    p[1] = 1;
    p[22] ^= 1;
    for (unsigned i = 0; i < 3; ++i)
    {
        f.id = static_cast<uint16_t>(0x650 + i);
        memcpy(f.data, p + i * 8, 8);
        CHECK(!rx.receive(f, i + 20, t));
    }
    p[1] = 2;
    crc = crc16(p, 22);
    p[22] = static_cast<uint8_t>(crc);
    p[23] = static_cast<uint8_t>(crc >> 8);
    for (unsigned i = 0; i < 3; ++i)
    {
        f.id = static_cast<uint16_t>(0x650 + i);
        memcpy(f.data, p + i * 8, 8);
        CHECK(!rx.receive(f, 100 + i * 30, t));
    }
}
int main()
{
    engine_tests();
    store_tests();
    obd_tests();
    transport_tests();
    telemetry_tests();
    printf("%u checks; %u failures. Synthetic/software evidence only.\n", checks, failures);
    return failures ? 1 : 0;
}
