#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class AudioPlayer {
public:
    AudioPlayer(int bck, int ws, int dout, int ampSdPin);
    // Start playback asynchronously; returns false if already playing or on error
    bool playFile(const String &filename);
    bool playRandom(const char* directory = "/");
    // Request stop and wait for playback task to end
    void stop();
    // Is audio currently playing
    bool isPlaying();

private:
    int _bck, _ws, _dout, _ampSdPin;
    void initI2S();
    void startI2S();
    void stopI2S();
    void amplifierOn();
    void amplifierOff();

    TaskHandle_t _taskHandle = nullptr;
    volatile bool _stopRequested = false;
    static void playTask(void* pvParameters);
};
