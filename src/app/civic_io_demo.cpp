#include "engine.h"
#include "transport.h"
#include <stdio.h>
using namespace Civic;
// Generate synthetic traces and injected code records from the real portable core; no device is opened.
int main()
{
    Engine engine(synthetic_profile());
    engine.command("ARM BENCH", 0, true);
    engine.command("RPM 1000", 0, true);
    engine.command("STATE RUN", 0, true);
    printf(
        "{\"classification\":\"synthetic_demo\",\"run_id\":\"CIVIC-SYN-001\",\"virtual_rpm\":1000,\"edges\":[");
    uint32_t now = 0;
    uint8_t levels = 0;
    for (unsigned i = 0; i < 96; ++i)
    {
        engine.next_edge(now, levels);
        printf("%s[%lu,%u]", i ? "," : "", static_cast<unsigned long>(now), levels);
        now += engine.wait_us(now);
    }
    Store store;
    Context ctx = {123, 3500, 750, 7};
    store.set(0x0133,
              honda_source,
              true,
              0,
              ctx,
              true); // injected original-code example, not an ECU measurement
    store.update_masks(1U << 7, 1UL << 21, 1, 0, ctx);
    store.update_masks(1U << 7, 1UL << 21, 1, 500, ctx);
    printf("],\"faults\":[");
    for (unsigned i = 0; i < store.size(); ++i)
    {
        char c[6];
        format_code(store.at(i).code, c);
        printf("%s{\"code\":\"%s\",\"source\":%u,\"stored\":%s}",
               i ? "," : "",
               c,
               store.at(i).source,
               store.at(i).stored ? "true" : "false");
    }
    Obd obd(store);
    uint8_t request[] = {3}, payload[192];
    size_t count = obd.request(request, 1, true, payload, sizeof(payload));
    printf("],\"mode03_payload\":[");
    for (size_t i = 0; i < count; ++i)
        printf("%s%u", i ? "," : "", payload[i]);
    printf("]}\n");
    return 0;
}
