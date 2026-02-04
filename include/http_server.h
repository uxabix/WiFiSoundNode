#pragma once

#include <ESPAsyncWebServer.h>
#include "audio_player.h"

extern AsyncWebServer server;
extern bool isStreaming;

void http_server_init(AudioPlayer& player);
void http_server_handle();
