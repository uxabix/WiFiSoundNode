#include "http_server.h"
#include "audio_engine.h"

WebServer server(80);

void handle_ping() {
    server.send(200, "text/plain", "OK");
}

void handle_play_test() {
    server.send(200, "text/plain", "PLAYING");
    audio_play_test();
}

void http_server_init() {
    server.on("/ping", HTTP_GET, handle_ping);
    server.on("/play/test", HTTP_GET, handle_play_test);
    server.begin();
}


void http_server_handle() {
    server.handleClient();
}
