#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define REQUEST_SIZE 8192
#define RESPONSE_SIZE 4096

/* Demo state only: this program does not connect to HVAC equipment. */
static double current_temp = 21.4;
static int airflow_percent = 75;

static int send_all(SOCKET socket, const char *data, int length)
{
    int sent = 0;
    while (sent < length) {
        int result = send(socket, data + sent, length - sent, 0);
        if (result == SOCKET_ERROR || result == 0) return 0;
        sent += result;
    }
    return 1;
}

static void respond(SOCKET client, int status, const char *reason,
                    const char *content_type, const char *body)
{
    char header[512];
    int body_length = (int)strlen(body);
    int header_length = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n\r\n",
        status, reason, content_type, body_length);

    if (header_length > 0 && header_length < (int)sizeof(header)) {
        send_all(client, header, header_length);
        send_all(client, body, body_length);
    }
}

static void render_dashboard(SOCKET client, const char *message)
{
    char body[RESPONSE_SIZE];
    int length = snprintf(body, sizeof(body),
        "<!doctype html>"
        "<html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>HVAC Edge Dashboard</title><style>"
        ":root{color-scheme:dark;--bg:#0b1220;--card:#111c2e;--line:#24354c;"
        "--ink:#e8eef7;--muted:#94a3b8;--accent:#5eead4}"
        "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);"
        "font:16px/1.5 system-ui,sans-serif}.wrap{max-width:880px;margin:48px auto;padding:0 20px}"
        "header,.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:24px}"
        "header{margin-bottom:18px}h1{font-size:1.65rem;margin:0 0 4px}p{color:var(--muted);margin:0}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:16px}"
        ".label{color:var(--muted);font-size:.85rem}.value{font-size:2.5rem;font-weight:700;margin-top:8px}"
        "form{margin-top:18px}.fields{display:flex;flex-wrap:wrap;gap:16px}label{display:grid;gap:6px;color:var(--muted)}"
        "input,button{font:inherit;border-radius:8px;padding:10px 12px}input{width:190px;color:var(--ink);"
        "background:var(--bg);border:1px solid var(--line)}button{margin-top:16px;border:0;"
        "background:var(--accent);color:#06201e;font-weight:700;cursor:pointer}.note{margin-top:18px;"
        "font-size:.9rem;color:var(--muted)}.message{color:var(--accent);min-height:1.5em;margin:10px 0}"
        "</style></head><body><main class=\"wrap\"><header><h1>HVAC Edge Dashboard</h1>"
        "<p>Local learning demo · simulated readings only</p></header>"
        "<section class=\"grid\"><article class=\"card\"><div class=\"label\">Zone temperature</div>"
        "<div class=\"value\">%.1f °C</div></article><article class=\"card\">"
        "<div class=\"label\">Airflow setting</div><div class=\"value\">%d%%</div></article></section>"
        "<section class=\"card\" style=\"margin-top:16px\"><h2 style=\"margin-top:0\">"
        "Update simulated values</h2><div class=\"message\">%s</div>"
        "<form method=\"post\" action=\"/\"><div class=\"fields\">"
        "<label>Airflow (0–100%%)<input name=\"airflow\" type=\"number\" min=\"0\" max=\"100\" value=\"%d\" required></label>"
        "<label>Temperature (16–30 °C)<input name=\"temp\" type=\"number\" min=\"16\" max=\"30\" step=\"0.1\" value=\"%.1f\" required></label>"
        "</div><button type=\"submit\">Save demo values</button></form></section>"
        "<p class=\"note\">This single-process demo stores values in memory. It is not a controller for real equipment.</p>"
        "</main></body></html>", current_temp, airflow_percent, message,
        airflow_percent, current_temp);

    if (length < 0 || length >= (int)sizeof(body)) {
        respond(client, 500, "Internal Server Error", "text/plain", "Dashboard response too large.");
        return;
    }
    respond(client, 200, "OK", "text/html", body);
}

static int get_content_length(const char *request)
{
    const char *header = strstr(request, "Content-Length:");
    if (header == NULL) return 0;
    header += strlen("Content-Length:");
    while (*header == ' ' || *header == '\t') header++;
    return atoi(header);
}

static void handle_client(SOCKET client)
{
    char request[REQUEST_SIZE];
    int used = 0;
    int header_end = -1;
    int expected_total = -1;

    while (used < REQUEST_SIZE - 1) {
        int received = recv(client, request + used, REQUEST_SIZE - 1 - used, 0);
        if (received <= 0) break;
        used += received;
        request[used] = '\0';

        if (header_end < 0) {
            char *separator = strstr(request, "\r\n\r\n");
            if (separator != NULL) {
                header_end = (int)(separator - request) + 4;
                int body_length = get_content_length(request);
                if (body_length < 0 || body_length > REQUEST_SIZE - header_end - 1) {
                    respond(client, 413, "Content Too Large", "text/plain", "Request body is too large.");
                    return;
                }
                expected_total = header_end + body_length;
            }
        }
        if (expected_total >= 0 && used >= expected_total) break;
    }

    if (used == 0) return;
    if (header_end < 0 || expected_total < 0 || used < expected_total) {
        respond(client, 400, "Bad Request", "text/plain", "Incomplete HTTP request.");
        return;
    }

    if (strncmp(request, "GET / ", 6) == 0) {
        render_dashboard(client, "");
    } else if (strncmp(request, "POST / ", 7) == 0) {
        char *body = request + header_end;
        int new_airflow;
        double new_temp;
        char extra;
        if (sscanf(body, "airflow=%d&temp=%lf%c", &new_airflow, &new_temp, &extra) != 2 ||
            new_airflow < 0 || new_airflow > 100 || new_temp < 16.0 || new_temp > 30.0) {
            respond(client, 400, "Bad Request", "text/plain",
                    "Values must be airflow 0-100 and temperature 16-30 C.");
            return;
        }
        airflow_percent = new_airflow;
        current_temp = new_temp;
        render_dashboard(client, "Demo values saved.");
    } else {
        respond(client, 404, "Not Found", "text/plain", "Route not found.");
    }
}

int main(void)
{
    WSADATA data;
    SOCKET server;
    struct sockaddr_in address;

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        fprintf(stderr, "Could not start Winsock.\n");
        return 1;
    }
    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        fprintf(stderr, "Could not create server socket.\n");
        WSACleanup();
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR ||
        listen(server, 8) == SOCKET_ERROR) {
        fprintf(stderr, "Could not listen on 127.0.0.1:%d.\n", PORT);
        closesocket(server);
        WSACleanup();
        return 1;
    }

    printf("HVAC Edge Dashboard is available at http://127.0.0.1:%d\n", PORT);
    printf("Demo only; values are simulated. Press Ctrl+C to stop.\n");
    for (;;) {
        SOCKET client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) break;
        handle_client(client);
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
