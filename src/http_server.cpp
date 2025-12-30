#include "http_server.h"

WebServer server(80);

void handle_ping() {
    server.send(200, "text/plain", "OK");
}

void http_server_init() {
    server.on("/ping", HTTP_GET, handle_ping);
    server.begin();
}

void http_server_handle() {
    server.handleClient();
}
