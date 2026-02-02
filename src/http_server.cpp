#include <LittleFS.h>
#include <WebServer.h>

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

    static unsigned long lastPlayMs = 0;
    const unsigned long minInterval = 200;
    unsigned long now = millis();
    if (now - lastPlayMs < minInterval) {
        server.send(429, "text/plain", "Too many requests");
        return;
    }

    String filename = "/" + server.arg("file");

    if (!LittleFS.exists(filename)) {
        server.send(404, "text/plain", "File not found");
        return;
    }

    bool started = audioPlayer && audioPlayer->playFile(filename);

    Serial.print("Play request: ");
    Serial.print(filename);
    Serial.print(" -> ");
    Serial.println(started ? "started" : "rejected");

    if (!started) {
        server.send(409, "text/plain", "Already playing or failed to start");
        return;
    }

    lastPlayMs = now;
    server.send(200, "text/plain", "Playing " + filename);
}

void handle_play_random() {
    static unsigned long lastRandomMs = 0;
    const unsigned long minInterval = 200;
    unsigned long now = millis();

    if (now - lastRandomMs < minInterval) {
        server.send(429, "text/plain", "Too many requests");
        return;
    }

    if (audioPlayer && audioPlayer->playRandom("/")) {
        lastRandomMs = now;
        server.send(200, "text/plain", "Random sound playing");
    } else {
        server.send(500, "text/plain", "Failed to play random sound");
    }
}

void handle_stop() {
    if (!audioPlayer) {
        server.send(500, "text/plain", "AudioPlayer not initialized");
        return;
    }

    audioPlayer->stop();
    Serial.println("Stop requested");
    server.send(200, "text/plain", "Stopped");
}

void handle_not_found() {
    String msg = "404 Not Found\nURI: ";
    msg += server.uri();
    msg += "\n";

    server.send(404, "text/plain", msg);

    Serial.print("Not found: ");
    Serial.println(server.uri());
}

void http_server_init(AudioPlayer& player) {
    audioPlayer = &player;

    server.on("/ping", HTTP_GET, handle_ping);
    server.on("/list", HTTP_GET, handle_list);
    server.on("/play", HTTP_GET, handle_play);
    server.on("/stop", HTTP_GET, handle_stop);
    server.on("/play_random", HTTP_GET, handle_play_random);
    server.onNotFound(handle_not_found);

    server.begin();
    Serial.println("HTTP server started (sync)");
}

void http_server_handle() {
    server.handleClient();
}
