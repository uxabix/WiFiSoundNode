#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <esp_heap_caps.h>

#include "http_server.h"
#include "sleep_manager.h"
#include "battery.h"

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

static constexpr uint16_t HTTP_PORT = 80;
static constexpr size_t WAV_HEADER_SIZE = 44;

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

AsyncWebServer server(HTTP_PORT);
static AudioPlayer* audioPlayer = nullptr;
bool isStreaming = false;

// -----------------------------------------------------------------------------
// Simple handlers
// -----------------------------------------------------------------------------

void handle_ping(AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "OK");
}

void handle_not_found(AsyncWebServerRequest* request) {
    String msg = "404 Not Found\nURI: ";
    msg += request->url();

    Serial.print("Not found: ");
    Serial.println(request->url());

    request->send(404, "text/plain", msg);
}

// -----------------------------------------------------------------------------
// File system handlers
// -----------------------------------------------------------------------------

void handle_list(AsyncWebServerRequest* request) {
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        request->send(500, "text/plain", "LittleFS error");
        return;
    }

    String response = "[";
    bool first = true;

    File file = root.openNextFile();
    while (file) {
        if (!first) response += ",";
        response += "\"" + String(file.name()) + "\"";
        first = false;
        file = root.openNextFile();
    }

    response += "]";
    request->send(200, "application/json", response);
}

// -----------------------------------------------------------------------------
// Playback handlers
// -----------------------------------------------------------------------------

void handle_play(AsyncWebServerRequest* request) {
    if (!request->hasParam("file")) {
        request->send(400, "text/plain", "Missing file parameter");
        return;
    }

    String filename = "/" + request->getParam("file")->value();

    if (!LittleFS.exists(filename)) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    bool started = audioPlayer && audioPlayer->playFile(filename);

    Serial.printf(
        "Play request: %s -> %s\n",
        filename.c_str(),
        started ? "started" : "rejected"
    );

    if (!started) {
        request->send(409, "text/plain", "Already playing or failed to start");
        return;
    }

    request->send(200, "text/plain", "Playing " + filename);
}

void handle_play_random(AsyncWebServerRequest* request) {
    if (audioPlayer && audioPlayer->playRandom("/")) {
        request->send(200, "text/plain", "Random sound playing");
    } else {
        request->send(500, "text/plain", "Failed to play random sound");
    }
}

void handle_stop(AsyncWebServerRequest* request) {
    if (!audioPlayer) {
        request->send(500, "text/plain", "AudioPlayer not initialized");
        return;
    }

    audioPlayer->uninstallI2S();
    Serial.println("Stop requested");

    request->send(200, "text/plain", "Stopped");
}

// -----------------------------------------------------------------------------
// Streaming upload handler
// -----------------------------------------------------------------------------

void handle_stream_upload(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
) {
    static bool headerSkipped = false;
    static size_t headerBytes = 0;

    // Upload start
    if (index == 0) {
        isStreaming = true;
        headerSkipped = false;
        headerBytes = 0;

        Serial.printf("Stream upload start, total=%zu\n", total);

        if (audioPlayer && audioPlayer->isPlaying()) {
            audioPlayer->stop();
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (audioPlayer) {
            audioPlayer->streamUploadStart(total);
        }
    }

    if (!audioPlayer) {
        return;
    }

    // Skip WAV header (first 44 bytes)
    size_t offset = 0;
    if (!headerSkipped) {
        size_t remaining = WAV_HEADER_SIZE - headerBytes;

        if (len <= remaining) {
            headerBytes += len;
            return;
        }

        offset = remaining;
        headerBytes = WAV_HEADER_SIZE;
        headerSkipped = true;

        Serial.println("WAV header skipped");
    }

    // Send raw PCM data directly to I2S
    if (len > offset) {
        audioPlayer->streamUploadWrite(data + offset, len - offset);
    }

    // Upload end
    if (index + len == total) {
        Serial.println("Stream upload complete");

        audioPlayer->streamUploadEnd();
        isStreaming = false;
    }
}

// Handler for /battery endpoint, returns JSON with voltage and percentage
void handle_battery(AsyncWebServerRequest* request) {
    float voltage = battery_get_voltage();
    float percent = battery_get_percentage();

    String response = "{";
    response += "\"voltage\":" + String(voltage, 2) + ",";
    response += "\"percent\":" + String(percent, 1);
    response += "}";

    request->send(200, "application/json", response);
}

// Handler for /sleep endpoint, returns JSON with sleep info
void handle_sleep(AsyncWebServerRequest* request) {
    SleepInfo info = sleep_get_info();

    String json = "{";
    json += "\"night_now\":" + String(info.night_now ? "true":"false") + ",";
    json += "\"current\":\"" +
            String(info.current_hour) + ":" +
            String(info.current_minute) + "\",";
    json += "\"sleep_from\":\"" +
            String(info.sleep_from_hour) + ":" +
            String(info.sleep_from_minute) + "\",";
    json += "\"sleep_to\":\"" +
            String(info.sleep_to_hour) + ":" +
            String(info.sleep_to_minute) + "\",";
    json += "\"seconds_to_event\":" + String(info.seconds_to_event);
    json += "}";

    request->send(200, "application/json", json);
}


// -----------------------------------------------------------------------------
// Server initialization
// -----------------------------------------------------------------------------

void http_server_init(AudioPlayer& player) {
    audioPlayer = &player;

    server.on("/ping", HTTP_GET, handle_ping);
    server.on("/list", HTTP_GET, handle_list);
    server.on("/play", HTTP_GET, handle_play);
    server.on("/play_random", HTTP_GET, handle_play_random);
    server.on("/stop", HTTP_GET, handle_stop);
    server.on("/battery", HTTP_GET, handle_battery);
    server.on("/sleep", HTTP_GET, handle_sleep);

    server.on(
        "/stream",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            isStreaming = false;
            request->send(200, "text/plain", "OK");
        },
        nullptr,
        handle_stream_upload
    );

    server.onNotFound(handle_not_found);
    server.begin();

    Serial.println("HTTP server started (async)");
}

// -----------------------------------------------------------------------------
// Compatibility stub
// -----------------------------------------------------------------------------

void http_server_handle() {
    // AsyncWebServer does not require handleClient()
    // This function is kept for compatibility
}
