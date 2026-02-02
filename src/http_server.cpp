#include <LittleFS.h>

#include "http_server.h"

WebServer server(80);
static AudioPlayer* audioPlayer = nullptr;


void handle_ping() {
    server.send(200, "text/plain", "OK");
}

void handle_list() {
    String response = "[";
    bool first = true;

    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        server.send(500, "text/plain", "LittleFS error");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!first) response += ",";
        response += "\"" + String(file.name()) + "\"";
        first = false;
        file = root.openNextFile();
    }

    response += "]";
    server.send(200, "application/json", response);
}

void handle_play() {
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Missing file parameter");
        return;
    }

    String filename = "/" + server.arg("file");

    if (!LittleFS.exists(filename)) {
        server.send(404, "text/plain", "File not found");
        return;
    }

    audioPlayer->playFile(filename);
    Serial.print("Play request: ");
    Serial.println(filename);

    server.send(200, "text/plain", "Playing " + filename);
}

void handle_stop() {
    if (!audioPlayer) {
        server.send(500, "text/plain", "AudioPlayer not initialized");
        return;
    }

    // TODO: Implement stop functionality in AudioPlayer class
    server.send(200, "text/plain", "Stop");
}

void http_server_init(AudioPlayer& player) {
    audioPlayer = &player;
    server.on("/ping", HTTP_GET, handle_ping);
    server.on("/list", HTTP_GET, handle_list);
    server.on("/play", HTTP_GET, handle_play);
    server.on("/stop", HTTP_GET, handle_stop);

    server.begin();
    Serial.println("HTTP server started");
}

void http_server_handle() {
    server.handleClient();
}
