#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

class AudioPlayer {
public:
    AudioPlayer(int bck, int ws, int dout, int ampSdPin);

    bool playFile(const String &filename);
    bool playRandom(const char* directory);
    void stop();
    bool isPlaying() const;
    void setVolume(float v);


private:
    static void playTask(void* arg);

    bool loadFileToRam(const char* filename);

    void initI2S();
    void startI2S();
    void stopI2S();
    void installI2S();
    void uninstallI2S();


    void amplifierOn();
    void amplifierOff();

    // pins
    int _bck, _ws, _dout, _ampSdPin;

    // task handle
    TaskHandle_t _task = nullptr;
    volatile bool _stopRequested = false;

    // audio buffer
    uint8_t* _audioData = nullptr;
    size_t   _audioSize = 0;

    float _volume = 1.0f; // 0.0 … 1.0
    bool _i2sInstalled = false;
};
