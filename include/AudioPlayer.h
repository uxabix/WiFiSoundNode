#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class AudioPlayer {
public:
    AudioPlayer(int bck, int ws, int dout);
    // Start playback asynchronously; returns false if already playing or on error
    bool playFile(const String &filename);
    // Request stop and wait for playback task to end
    void stop();
    // Is audio currently playing
    bool isPlaying();

private:
    int _bck, _ws, _dout;
    void initI2S();

    TaskHandle_t _taskHandle = nullptr;
    volatile bool _stopRequested = false;
    static void playTask(void* pvParameters);
};
