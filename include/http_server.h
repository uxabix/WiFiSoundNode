#pragma once

#include <WebServer.h>
#include "AudioPlayer.h"

extern WebServer server;

void http_server_init(AudioPlayer& player);
void http_server_handle();
