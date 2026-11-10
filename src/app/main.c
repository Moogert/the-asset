// Host application entry point; simulator clock progression is separate from physical MCU scheduling.
#include "harness.h"
#include "http_server.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(unsigned long ms)
{
    Sleep(ms);
}
#else
#include <time.h>
static void sleep_ms(unsigned long ms)
{
    struct timespec ts = {ms / 1000UL, (long)(ms % 1000UL) * 1000000L};
    nanosleep(&ts, NULL);
}
#endif

int main(int argc, char** argv)
{
    unsigned short port = 8080U;
    if (argc != 1)
    {
        if (argc != 3 || strcmp(argv[1], "--port") != 0)
        {
            fprintf(stderr, "Usage: bench_ev_harness [--port 1..65535]\n");
            return 2;
        }
        char* end = NULL;
        const unsigned long parsed = strtoul(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0' || parsed == 0UL || parsed > 65535UL)
            return 2;
        port = (unsigned short)parsed;
    }
    harness_t harness;
    harness_init(&harness);
    http_server_t server;
    if (!http_server_start(&server, &harness, port))
    {
        (void)fprintf(stderr, "Unable to bind localhost:%u\n", (unsigned)port);
        return 1;
    }
    (void)printf("Bench EV Harness listening at http://127.0.0.1:%u/\n", (unsigned)port);
    (void)printf("Simulation-only mode; press Ctrl+C to stop.\n");
    for (;;)
    {
        http_server_poll(&server);
        harness_step(&harness, 10U);
        sleep_ms(10UL);
    }
}
