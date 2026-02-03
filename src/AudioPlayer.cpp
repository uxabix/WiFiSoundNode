#include "AudioPlayer.h"

AudioPlayer::AudioPlayer(int bck, int ws, int dout, int ampSdPin)
    : _bck(bck), _ws(ws), _dout(dout), _ampSdPin(ampSdPin) {
    pinMode(_ampSdPin, OUTPUT);
    amplifierOff();
}

void AudioPlayer::playBuffer(uint8_t* data, size_t size) {
    if (_task) return;

    _stopRequested = false;
    _isStreaming = true;

    // DMA-safe malloc
    _audioData = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_DMA);
    if (!_audioData) return;

    memcpy(_audioData, data, size);
    _audioSize = size;
    _shouldFreeAudioData = true;

    struct Args {
        AudioPlayer* self;
    };

    Args* args = new Args{this};
    if (xTaskCreate(playBufferTask, "AudioPlay", 8192, args, 1, &_task) != pdPASS) {
        heap_caps_free(_audioData);
        _audioData = nullptr;
        delete args;
    }
}


void AudioPlayer::installI2S() {
    if (_i2sInstalled) return;

    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 22050,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };

    i2s_pin_config_t pins = {
        .bck_io_num = _bck,
        .ws_io_num = _ws,
        .data_out_num = _dout,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err1 = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    esp_err_t err2 = i2s_set_pin(I2S_NUM_0, &pins);
    
    Serial.printf("I2S install result: %d, pin result: %d\n", err1, err2);

    _i2sInstalled = true;
}

void AudioPlayer::uninstallI2S() {
    if (!_i2sInstalled) return;

    i2s_stop(I2S_NUM_0);
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);

    _i2sInstalled = false;
}

void AudioPlayer::startI2S() {
    if (_i2sInstalled) i2s_start(I2S_NUM_0);
}

void AudioPlayer::stopI2S() {
    if (_i2sInstalled) {
        i2s_zero_dma_buffer(I2S_NUM_0);
        i2s_stop(I2S_NUM_0);
    }
}

void AudioPlayer::amplifierOn() {
    digitalWrite(_ampSdPin, HIGH);
    Serial.printf("Amplifier enabled (pin %d)\n", _ampSdPin);
}

void AudioPlayer::amplifierOff() {
    digitalWrite(_ampSdPin, LOW);
    Serial.printf("Amplifier disabled (pin %d)\n", _ampSdPin);
}

bool AudioPlayer::loadFileToRam(const char* filename) {
    File f = LittleFS.open(filename, "r");
    if (!f) {
        Serial.printf("Cannot open file: %s\n", filename);
        return false;
    }

    size_t size = f.size();
    Serial.printf("File size: %zu bytes\n", size);
    
    uint8_t* buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_DMA);
    if (!buf) { 
        Serial.println("DMA malloc failed");
        f.close(); 
        return false; 
    }

    size_t readBytes = f.read(buf, size);
    f.close();
    
    Serial.printf("Read %zu bytes from file\n", readBytes);

    _audioData = buf;
    _audioSize = size;
    _shouldFreeAudioData = true; 
    return true;
}

bool AudioPlayer::playFile(const String &filename) {
    if (_task) return false;

    _stopRequested = false;

    struct Args {
        AudioPlayer* self;
        char* filename;
    };

    Args* args = new Args{this, strdup(filename.c_str())};
    if (!args->filename) { delete args; return false; }

    if (xTaskCreate(playTask, "AudioPlay", 4096, args, 2, &_task) != pdPASS) {
        free(args->filename);
        delete args;
        _task = nullptr;
        return false;
    }

    return true;
}

void AudioPlayer::playTask(void* arg) {
    struct Args { AudioPlayer* self; char* filename; };
    Args* args = static_cast<Args*>(arg);
    AudioPlayer* self = args->self;
    char* filename = args->filename;
    delete args;

    if (!self->loadFileToRam(filename)) {
        Serial.printf("Failed to load file: %s\n", filename);
        free(filename);
        self->_task = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    Serial.printf("File loaded: %s, size=%zu bytes\n", filename, self->_audioSize);
    free(filename);

    self->installI2S();
    self->startI2S();
    vTaskDelay(50);
    self->amplifierOn();
    Serial.printf("Amplifier ON, I2S started\n");

    constexpr size_t WAV_HEADER_SIZE = 44;
    size_t offset = min((size_t)WAV_HEADER_SIZE, self->_audioSize);

    Serial.printf("Starting playback from offset %zu\n", offset);
    while (offset < self->_audioSize && !self->_stopRequested) {
        size_t written = 0;
        size_t chunk = min<size_t>(2048, self->_audioSize - offset);

        int res = i2s_write(I2S_NUM_0, self->_audioData + offset, chunk, &written, portMAX_DELAY);
        if (res != ESP_OK) {
            Serial.printf("i2s_write error: %d\n", res);
        }
        offset += written;
    }
    Serial.printf("Playback finished\n");

    if (self->_shouldFreeAudioData && self->_audioData) {
        heap_caps_free(self->_audioData);
    }
    self->_audioData = nullptr;
    self->_audioSize = 0;
    self->_shouldFreeAudioData = false;
    self->_isStreaming = false;

    self->stopI2S();
    self->amplifierOff();
    self->uninstallI2S();

    self->_stopRequested = false;
    self->_task = nullptr;
    vTaskDelete(nullptr);
}

void AudioPlayer::stop() { 
    _stopRequested = true; 
}

bool AudioPlayer::isPlaying() const { 
    return _task != nullptr; 
}

void AudioPlayer::setVolume(float v) { 
    _volume = constrain(v, 0.0f, 1.0f); 
}

void AudioPlayer::playBufferTask(void* arg) {
    struct Args { AudioPlayer* self; };
    Args* args = static_cast<Args*>(arg);
    AudioPlayer* self = args->self;
    delete args;

    self->installI2S();
    self->startI2S();
    vTaskDelay(50);
    self->amplifierOn();
    Serial.printf("Buffer playback started: size=%zu bytes\n", self->_audioSize);

    // Check if this looks like a WAV file
    constexpr size_t WAV_HEADER_SIZE = 44;
    size_t offset = 0;
    
    if (self->_audioSize >= 4) {
        char riff[4];
        memcpy(riff, self->_audioData, 4);
        if (riff[0] == 'R' && riff[1] == 'I' && riff[2] == 'F' && riff[3] == 'F') {
            Serial.println("WAV file detected, skipping header");
            offset = min((size_t)WAV_HEADER_SIZE, self->_audioSize);
        } else {
            Serial.println("No WAV header detected, playing from start");
            offset = 0;
        }
    }

    Serial.printf("Starting playback from offset %zu\n", offset);
    while (offset < self->_audioSize && !self->_stopRequested) {
        size_t written = 0;
        size_t chunk = min<size_t>(2048, self->_audioSize - offset);

        int res = i2s_write(I2S_NUM_0, self->_audioData + offset, chunk, &written, portMAX_DELAY);
        if (res != ESP_OK) {
            Serial.printf("i2s_write error: %d\n", res);
        }
        offset += written;
    }
    Serial.printf("Buffer playback finished\n");

    if (self->_shouldFreeAudioData && self->_audioData) {
        heap_caps_free(self->_audioData);
    }
    self->_audioData = nullptr;
    self->_audioSize = 0;
    self->_shouldFreeAudioData = false;
    self->_isStreaming = false;

    self->stopI2S();
    self->amplifierOff();
    self->uninstallI2S();

    self->_stopRequested = false;
    self->_task = nullptr;
    vTaskDelete(nullptr);
}

static bool hasWavExtension(const String &name) {
    return name.endsWith(".wav");
}

bool AudioPlayer::playRandom(const char* directory) {
    if (_task) return false;

    File dir = LittleFS.open(directory);
    if (!dir || !dir.isDirectory()) return false;

    std::vector<String> wavFiles;
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory() && hasWavExtension(file.name()))
            wavFiles.push_back(String(directory) + "/" + file.name());
        file = dir.openNextFile();
    }

    if (wavFiles.empty()) return false;

    int idx = random(wavFiles.size());
    return playFile(wavFiles[idx]);
}

void AudioPlayer::playFromHttp(const char* url) {
    if (_task) return;
    _stopRequested = false;

    struct HttpArgs { AudioPlayer* self; String url; };
    HttpArgs* args = new HttpArgs{this, String(url)};
    xTaskCreate(httpStreamTask, "HTTP_STREAM", 8192, args, 2, &_task);
}

void AudioPlayer::httpStreamTask(void* arg) {
    struct HttpArgs { AudioPlayer* self; String url; };
    HttpArgs* args = static_cast<HttpArgs*>(arg);
    AudioPlayer* self = args->self;
    String url = args->url;
    delete args;

    HTTPClient http;
    WiFiClient client;

    if (!http.begin(client, url)) { self->_task=nullptr; vTaskDelete(nullptr); return; }

    int code = http.GET();
    if (code != HTTP_CODE_OK) { http.end(); self->_task=nullptr; vTaskDelete(nullptr); return; }

    self->installI2S();
    self->startI2S();
    self->amplifierOn();

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[2048];
    bool headerSkipped = false;
    size_t headerBytes = 0;

    while (http.connected() && !self->_stopRequested) {
        int avail = stream->available();
        if (avail <= 0) { vTaskDelay(1); continue; }

        int toRead = min(avail, (int)sizeof(buffer));
        int len = stream->read(buffer, toRead);
        if (len <= 0) break;

        size_t offset = 0;
        if (!headerSkipped) {
            size_t need = 44 - headerBytes;
            if (len <= need) { headerBytes += len; continue; }
            offset = need;
            headerSkipped = true;
        }

        size_t written;
        i2s_write(I2S_NUM_0, buffer + offset, len - offset, &written, portMAX_DELAY);
    }

    self->amplifierOff();
    self->stopI2S();
    self->uninstallI2S();

    http.end();
    self->_stopRequested=false;
    self->_task=nullptr;
    vTaskDelete(nullptr);
}

void AudioPlayer::streamDirect(WiFiClient* client, size_t contentLength) {
    if (_task) return;
    _stopRequested = false;

    struct StreamArgs { 
        AudioPlayer* self; 
        WiFiClient* client;
        size_t contentLength;
    };

    StreamArgs* args = new StreamArgs{this, client, contentLength};
    if (xTaskCreate(directStreamTask, "DirectStream", 8192, args, 2, &_task) != pdPASS) {
        delete args;
    }
}

void AudioPlayer::directStreamTask(void* arg) {
    struct StreamArgs { 
        AudioPlayer* self; 
        WiFiClient* client;
        size_t contentLength;
    };
    StreamArgs* args = static_cast<StreamArgs*>(arg);
    AudioPlayer* self = args->self;
    WiFiClient* client = args->client;
    size_t contentLength = args->contentLength;
    delete args;

    self->installI2S();
    self->startI2S();
    vTaskDelay(50);
    self->amplifierOn();
    
    Serial.printf("Direct stream started: %zu bytes\n", contentLength);

    uint8_t buffer[2048];
    size_t bytesRead = 0;
    bool headerSkipped = false;
    size_t headerBytes = 0;

    while (bytesRead < contentLength && !self->_stopRequested) {
        // Check if data available
        if (!client || !client->connected()) {
            Serial.println("Client disconnected");
            break;
        }

        int available = client->available();
        if (available <= 0) {
            vTaskDelay(1);
            continue;
        }

        int toRead = min(available, (int)sizeof(buffer));
        int len = client->read(buffer, toRead);
        if (len <= 0) break;

        bytesRead += len;

        // Skip WAV header
        size_t offset = 0;
        if (!headerSkipped) {
            size_t need = 44 - headerBytes;
            if (len <= (int)need) { 
                headerBytes += len; 
                continue; 
            }
            offset = need;
            headerBytes = 44;
            headerSkipped = true;
            Serial.printf("Header skipped, starting audio data\n");
        }

        if (len > (int)offset) {
            size_t written;
            int res = i2s_write(I2S_NUM_0, buffer + offset, len - offset, &written, 100 / portTICK_PERIOD_MS);
            if (res != ESP_OK) {
                Serial.printf("i2s_write error: %d\n", res);
            }
        }
    }

    Serial.printf("Direct stream finished: received %zu bytes\n", bytesRead);
    self->amplifierOff();
    self->stopI2S();
    self->uninstallI2S();

    self->_stopRequested = false;
    self->_task = nullptr;
    vTaskDelete(nullptr);
}

void AudioPlayer::streamDirectSync(WiFiClient& client, size_t contentLength) {
    // Synchronous streaming inside HTTP handler: keep connection open while reading
    if (contentLength == 0) return;

    installI2S();
    startI2S();
    vTaskDelay(50);
    amplifierOn();

    Serial.printf("Direct sync stream started: %zu bytes\n", contentLength);

    uint8_t buffer[2048];
    size_t bytesRead = 0;
    bool headerSkipped = false;
    size_t headerBytes = 0;

    while (bytesRead < contentLength && client.connected()) {
        if (client.available() <= 0) {
            delay(1);
            continue;
        }

        int toRead = min(client.available(), (int)sizeof(buffer));
        int len = client.read(buffer, toRead);
        if (len <= 0) break;

        bytesRead += len;

        size_t offset = 0;
        if (!headerSkipped) {
            size_t need = 44 - headerBytes;
            if (len <= (int)need) {
                headerBytes += len;
                continue;
            }
            offset = need;
            headerBytes = 44;
            headerSkipped = true;
            Serial.println("Header skipped, starting audio data");
        }

        if (len > (int)offset) {
            size_t written = 0;
            esp_err_t res = i2s_write(I2S_NUM_0, buffer + offset, len - offset, &written, 100 / portTICK_PERIOD_MS);
            if (res != ESP_OK) Serial.printf("i2s_write error: %d\n", res);
        }
    }

    Serial.printf("Direct sync stream finished: received %zu bytes\n", bytesRead);
    amplifierOff();
    stopI2S();
    uninstallI2S();
}

void AudioPlayer::streamUploadStart(size_t totalSize) {
    (void)totalSize;
    _uploadHeaderSkipped = false;
    _uploadHeaderBytes = 0;
    installI2S();
    startI2S();
    vTaskDelay(20);
    amplifierOn();
    Serial.printf("Upload stream start: %zu bytes\n", totalSize);
}

void AudioPlayer::streamUploadWrite(const uint8_t* buf, size_t len) {
    if (!_i2sInstalled) return;

    size_t offset = 0;
    if (!_uploadHeaderSkipped) {
        size_t need = 44 - _uploadHeaderBytes;
        if (len <= (int)need) {
            _uploadHeaderBytes += len;
            return;
        }
        offset = need;
        _uploadHeaderBytes = 44;
        _uploadHeaderSkipped = true;
        Serial.println("Upload: WAV header skipped");
    }

    if (len > (int)offset) {
        size_t written = 0;
        esp_err_t res = i2s_write(I2S_NUM_0, buf + offset, len - offset, &written, 100 / portTICK_PERIOD_MS);
        if (res != ESP_OK) Serial.printf("i2s_write error: %d\n", res);
    }
}

void AudioPlayer::streamUploadEnd() {
    Serial.println("Upload stream end");
    amplifierOff();
    stopI2S();
    uninstallI2S();
    _uploadHeaderSkipped = false;
    _uploadHeaderBytes = 0;
}

void AudioPlayer::streamUploadAbort() {
    Serial.println("Upload stream aborted");
    amplifierOff();
    stopI2S();
    uninstallI2S();
    _uploadHeaderSkipped = false;
    _uploadHeaderBytes = 0;
}