// Loopback simulator UI/server. Device control is simulated; HTML/JS literals are kept intact by formatting.
#include "http_server.h"
#include "json_util.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET socket_t;
#define BAD_SOCKET INVALID_SOCKET
#define CLOSE_SOCKET closesocket
static bool socket_init_done = false;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_t;
#define BAD_SOCKET (-1)
#define CLOSE_SOCKET close
#endif

static void close_socket(socket_t s)
{
    if (s != BAD_SOCKET)
        CLOSE_SOCKET(s);
}

static uint64_t wall_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec t;
    (void)timespec_get(&t, TIME_UTC);
    return (uint64_t)t.tv_sec * 1000U + (uint64_t)t.tv_nsec / 1000000U;
#endif
}

static void send_response(socket_t client, int code, const char* type, const char* body)
{
    char header[256];
    const size_t len = strlen(body);
    const int n = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: %llu\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",
        code,
        type,
        (unsigned long long)len);
    if (n > 0)
    {
        (void)send(client, header, n, 0);
        (void)send(client, body, (int)len, 0);
    }
}

static const char* page_for_path(const char* path, size_t* length, const char** type)
{
    static const char html[] =
        "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Bench EV Harness</title><link rel=\"stylesheet\" href=\"/style.css\"></head><body><main><h1>Bench EV Harness</h1><div class=\"warning\">SIMULATION ONLY — NO HARDWARE OUTPUTS</div><section class=\"grid\"><article><h2>Safety supervisor</h2><pre id=\"state\">Loading…</pre></article><article><h2>Controls</h2><label>Throttle A <input id=\"ta\" type=\"range\" min=\"0\" max=\"100\" value=\"0\"></label><label>Throttle B <input id=\"tb\" type=\"range\" min=\"0\" max=\"100\" value=\"0\"></label><button onclick=\"drive()\">Enable simulated drive</button><button onclick=\"stopDrive()\">Stop drive</button><button onclick=\"fault('bms_timeout')\">Inject BMS timeout</button><button onclick=\"fault('bus_off')\">Inject CAN bus-off</button><button onclick=\"fault('inverter_desat')\">Inject inverter desat</button><button onclick=\"fault('estop')\">Inject emergency stop</button><button onclick=\"reset()\">Reset simulation</button></article></section><h2>Dual-motor scenarios</h2><p>Each scenario resets the simulation. Mode changes during armed operation latch an inhibit.</p><button onclick=\"scenario('mg1_only')\">MG1 only</button><button onclick=\"scenario('nominal_drive')\">MG2 only</button><button onclick=\"scenario('combined_drive')\">Combined drive</button><button onclick=\"scenario('combined_regen')\">Combined regen</button><button onclick=\"fault('mg1_timeout')\">Lose MG1 feedback</button><button onclick=\"fault('mg2_resolver')\">Lose MG2 resolver</button><h2>MG1 / MG2</h2><pre id=\"dual\"></pre><h2>CAN trace</h2><pre id=\"can\"></pre><h2>Event log</h2><pre id=\"events\"></pre></main><script src=\"/app.js\"></script></body></html>";
    static const char css[] =
        "body{margin:0;background:#111827;color:#e5e7eb;font:15px system-ui}main{max-width:1000px;margin:32px auto;padding:0 20px}.warning{background:#7f1d1d;color:#fecaca;padding:12px;border-radius:8px;font-weight:700}.grid{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-top:16px}article{background:#1f2937;padding:18px;border-radius:10px;box-shadow:0 4px 16px #0005}h1{color:#93c5fd}h2{font-size:18px}pre{white-space:pre-wrap;word-break:break-word;color:#d1d5db}label{display:block;margin:14px 0}input{width:100%}button{background:#2563eb;border:0;color:white;border-radius:6px;padding:9px;margin:4px;cursor:pointer}button:hover{background:#1d4ed8}";
    static const char js[] =
        "async function j(url,opt){let r=await fetch(url,opt);return r.json()}async function refresh(){let s=await j('/api/state');document.getElementById('dual').textContent=JSON.stringify(s.dual,null,2);delete s.dual;document.getElementById('state').textContent=JSON.stringify(s,null,2);document.getElementById('can').textContent=JSON.stringify(await j('/api/can/trace'),null,2);document.getElementById('events').textContent=JSON.stringify(await j('/api/events'),null,2)}async function post(url,b){await j(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});refresh()}function drive(){post('/api/input',{arm:true,drive_request:true,precharge_ok:true,throttle_a_percent:+ta.value,throttle_b_percent:+tb.value})}function stopDrive(){post('/api/input',{drive_request:false,charge_request:false,throttle_a_percent:0,throttle_b_percent:0})}function fault(n){post('/api/fault',{name:n,enabled:true})}function scenario(n){post('/api/scenarios/run',{name:n})}function reset(){post('/api/control',{action:'reset'})}setInterval(refresh,500);refresh();";
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
    {
        *length = sizeof(html) - 1U;
        *type = "text/html";
        return html;
    }
    if (strcmp(path, "/style.css") == 0)
    {
        *length = sizeof(css) - 1U;
        *type = "text/css";
        return css;
    }
    if (strcmp(path, "/app.js") == 0)
    {
        *length = sizeof(js) - 1U;
        *type = "application/javascript";
        return js;
    }
    return NULL;
}

static void update_input_from_json(harness_t* h, const char* body, size_t length)
{
    double n;
    bool b;
    if (json_get_bool(body, length, "arm", &b))
        h->inputs.arm = b;
    if (json_get_bool(body, length, "brake", &b))
        h->inputs.brake = b;
    if (json_get_bool(body, length, "drive_request", &b))
        h->inputs.drive_request = b;
    if (json_get_bool(body, length, "charge_request", &b))
        h->inputs.charge_request = b;
    if (json_get_bool(body, length, "ac_present", &b))
        h->inputs.ac_present = b;
    if (json_get_number(body, length, "throttle_a_percent", &n) && isfinite(n) && n >= 0.0 &&
        n <= 100.0)
        h->inputs.throttle_a_percent = (float)n;
    if (json_get_number(body, length, "throttle_b_percent", &n) && isfinite(n) && n >= 0.0 &&
        n <= 100.0)
        h->inputs.throttle_b_percent = (float)n;
}

static void handle_request(http_server_t* server, socket_t client, char* request, size_t length)
{
    char method[8] = {0};
    char path[96] = {0};
    if (sscanf(request, "%7s %95s", method, path) != 2)
    {
        send_response(client, 400, "text/plain", "bad request");
        return;
    }
    char* body = strstr(request, "\r\n\r\n");
    size_t body_len = 0U;
    if (body != NULL)
    {
        body += 4;
        body_len = length - (size_t)(body - request);
    }
    if (strcmp(method, "GET") == 0)
    {
        char json[32768];
        if (strcmp(path, "/api/state") == 0)
        {
            (void)harness_state_json(server->harness, json, sizeof(json));
            send_response(client, 200, "application/json", json);
            return;
        }
        if (strcmp(path, "/api/events") == 0)
        {
            (void)harness_events_json(server->harness, json, sizeof(json));
            send_response(client, 200, "application/json", json);
            return;
        }
        if (strcmp(path, "/api/can/trace") == 0)
        {
            (void)harness_can_trace_json(server->harness, json, sizeof(json));
            send_response(client, 200, "application/json", json);
            return;
        }
        if (strcmp(path, "/api/scenarios") == 0)
        {
            (void)harness_scenarios_json(json, sizeof(json));
            send_response(client, 200, "application/json", json);
            return;
        }
        size_t n;
        const char* type;
        const char* page = page_for_path(path, &n, &type);
        if (page)
        {
            send_response(client, 200, type, page);
            return;
        }
        send_response(client, 404, "text/plain", "not found");
        return;
    }
    if (strcmp(method, "POST") == 0 && body != NULL && body_len <= 16384U &&
        json_is_safe_document(body, body_len))
    {
        if (strcmp(path, "/api/input") == 0)
            update_input_from_json(server->harness, body, body_len);
        else if (strcmp(path, "/api/fault") == 0)
        {
            char name[48];
            bool enabled;
            if (json_get_string(body, body_len, "name", name, sizeof(name)) &&
                json_get_bool(body, body_len, "enabled", &enabled))
                harness_set_fault(server->harness, name, enabled);
        }
        else if (strcmp(path, "/api/control") == 0)
        {
            char action[24];
            if (json_get_string(body, body_len, "action", action, sizeof(action)) &&
                strcmp(action, "reset") == 0)
                harness_reset(server->harness);
        }
        else if (strcmp(path, "/api/scenarios/run") == 0)
        {
            char name[48];
            if (json_get_string(body, body_len, "name", name, sizeof(name)))
                (void)harness_run_scenario(server->harness, name);
        }
        else
        {
            send_response(client, 404, "text/plain", "not found");
            return;
        }
        send_response(client, 200, "application/json", "{\"ok\":true}");
        return;
    }
    send_response(client, 400, "text/plain", "invalid request");
}

bool http_server_start(http_server_t* server, harness_t* harness, unsigned short port)
{
    if (server == NULL || harness == NULL)
        return false;
#ifdef _WIN32
    WSADATA wsa;
    if (!socket_init_done && WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;
    socket_init_done = true;
#endif
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == BAD_SOCKET)
        return false;
    int yes = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    (void)inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(s, 4) != 0)
    {
        close_socket(s);
        return false;
    }
#ifdef _WIN32
    u_long nonblock = 1;
    (void)ioctlsocket(s, FIONBIO, &nonblock);
#else
    (void)fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
    server->listen_socket = (intptr_t)s;
    server->running = true;
    server->harness = harness;
    return true;
}

void http_server_poll(http_server_t* server)
{
    if (server == NULL || !server->running)
        return;
    socket_t client = accept((socket_t)server->listen_socket, NULL, NULL);
    if (client == BAD_SOCKET)
        return;
    /* Accepted sockets may inherit nonblocking mode. Wait for a complete request
       within a bounded deadline, including fragmented header/body delivery. */
    char request[16384 + 1];
    size_t used = 0U, expected = 0U;
    const uint64_t deadline = wall_ms() + 50U;
    while (used < sizeof(request) - 1U)
    {
        const uint64_t now = wall_ms();
        if (now >= deadline)
            break;
        fd_set readers;
        FD_ZERO(&readers);
        FD_SET(client, &readers);
        struct timeval timeout = {0, (long)((deadline - now) * 1000U)};
        if (select((int)client + 1, &readers, NULL, NULL, &timeout) <= 0)
            break;
        const int n = recv(client, request + used, (int)(sizeof(request) - 1U - used), 0);
        if (n <= 0)
            break;
        used += (size_t)n;
        request[used] = '\0';
        char* end = strstr(request, "\r\n\r\n");
        if (end != NULL && expected == 0U)
        {
            expected = (size_t)(end - request) + 4U;
            /* Match the standard header case-insensitively. Chunked requests
               are unsupported by this small loopback-only development server. */
            for (char* line = strstr(request, "\r\n"); line && line < end;
                 line = strstr(line + 2, "\r\n"))
            {
                const char* header = line + 2;
                const char* key = "content-length:";
                unsigned k = 0U;
                while (key[k] && header + k < end &&
                       (header[k] == key[k] ||
                        (header[k] >= 'A' && header[k] <= 'Z' && header[k] - 'A' + 'a' == key[k])))
                    ++k;
                if (key[k] == '\0')
                {
                    char* number_end = NULL;
                    const unsigned long content = strtoul(header + k, &number_end, 10);
                    if (number_end == header + k || content > 16384U ||
                        expected + content >= sizeof(request))
                    {
                        close_socket(client);
                        return;
                    }
                    expected += (size_t)content;
                }
            }
        }
        if (expected && used >= expected)
        {
            handle_request(server, client, request, expected);
            close_socket(client);
            return;
        }
    }
    close_socket(client);
}

void http_server_stop(http_server_t* server)
{
    if (server && server->running)
    {
        close_socket((socket_t)server->listen_socket);
        server->running = false;
    }
}
