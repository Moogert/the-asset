#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "harness.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    intptr_t listen_socket;
    bool running;
    harness_t* harness;
} http_server_t;

bool http_server_start(http_server_t* server, harness_t* harness, unsigned short port);
void http_server_poll(http_server_t* server);
void http_server_stop(http_server_t* server);

#endif
