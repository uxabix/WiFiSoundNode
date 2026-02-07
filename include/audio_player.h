#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <LittleFS.h>
#include <vector>

class AudioPlayer {
public:
    AudioPlayer(int bck, int ws, int dout, int ampSdPin, bool ampOnState);

    bool playFile(const String &filename);
    bool playRandom(const char* directory);
    void stop();
    void stopStreaming();
    bool isPlaying() const;
    void setVolume(float v);
    // upload-based streaming (called from HTTP upload handler)
    void streamUploadStart(size_t totalSize);
    void streamUploadWrite(const uint8_t* buf, size_t len);
    void streamUploadEnd();
    void streamUploadAbort();
    void uninstallI2S();

    volatile bool stopRequested = false;
private:
    static void playTask(void* arg);
    static void playBufferTask(void* arg);
    static void directStreamTask(void* arg);

    bool loadFileToRam(const char* filename);

    void installI2S();
    void startI2S();
    void stopI2S();

    void amplifierOn();
    void amplifierOff();

    int _bck, _ws, _dout, _ampSdPin;
    TaskHandle_t _task = nullptr;

    uint8_t* _audioData = nullptr;
    size_t _audioSize = 0;

    float _volume = 1.0f;
    bool _OnState = 1; // 1 - on when HIGH, 0 - on when LOW  
    bool _i2sInstalled = false;
    bool _isStreaming = false;
    bool _shouldFreeAudioData = false;
    // upload streaming state
    bool _uploadHeaderSkipped = false;
    size_t _uploadHeaderBytes = 0;
};
