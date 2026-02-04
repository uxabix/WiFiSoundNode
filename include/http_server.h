#pragma once

#include <ESPAsyncWebServer.h>
#include "AudioPlayer.h"

extern AsyncWebServer server;
extern bool isStreaming;

void http_server_init(AudioPlayer& player);
void http_server_handle();
