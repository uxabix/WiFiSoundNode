#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <LittleFS.h>
#include <vector>

class AudioPlayer {
public:
    AudioPlayer(int bck, int ws, int dout, int ampSdPin);

    bool playFile(const String &filename);
    bool playRandom(const char* directory);
    void stop();
    bool isPlaying() const;
    void setVolume(float v);
    void playFromHttp(const char* url);
    void playBuffer(uint8_t* data, size_t size);
    void streamDirectSync(WiFiClient& client, size_t contentLength);
    // upload-based streaming (called from HTTP upload handler)
    void streamUploadStart(size_t totalSize);
    void streamUploadWrite(const uint8_t* buf, size_t len);
    void streamUploadEnd();
    void streamUploadAbort();
    void streamDirect(WiFiClient* client, size_t contentLength);

private:
    static void playTask(void* arg);
    static void playBufferTask(void* arg);
    static void directStreamTask(void* arg);
    static void httpStreamTask(void* arg);

    bool loadFileToRam(const char* filename);

    void installI2S();
    void uninstallI2S();
    void startI2S();
    void stopI2S();

    void amplifierOn();
    void amplifierOff();

    int _bck, _ws, _dout, _ampSdPin;
    TaskHandle_t _task = nullptr;
    volatile bool _stopRequested = false;

    uint8_t* _audioData = nullptr;
    size_t _audioSize = 0;

    float _volume = 1.0f;
    bool _i2sInstalled = false;
    bool _isStreaming = false;
    bool _shouldFreeAudioData = false;
    // upload streaming state
    bool _uploadHeaderSkipped = false;
    size_t _uploadHeaderBytes = 0;
};
