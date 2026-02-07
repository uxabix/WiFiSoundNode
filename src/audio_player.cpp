#include "audio_player.h"

AudioPlayer::AudioPlayer(int bck, int ws, int dout, int ampSdPin, bool ampOnState)
    : _bck(bck), _ws(ws), _dout(dout), _ampSdPin(ampSdPin), _OnState(ampOnState) {
    pinMode(_ampSdPin, OUTPUT);
    amplifierOff();
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
    gpio_hold_dis((gpio_num_t)_ampSdPin);
    gpio_set_direction((gpio_num_t)_ampSdPin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)_ampSdPin, _OnState ? 1 : 0);
    Serial.printf("Amplifier enabled (pin %d)\n", _ampSdPin);
}

void AudioPlayer::amplifierOff() {
    gpio_hold_dis((gpio_num_t)_ampSdPin);
    gpio_set_direction((gpio_num_t)_ampSdPin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)_ampSdPin, _OnState ? 0 : 1);
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
    if (_task || _isStreaming) {
        stopRequested = true;
        stopStreaming();
        while (_task) { vTaskDelay(pdMS_TO_TICKS(10)); }
    }
    stopI2S();
    uninstallI2S();
    amplifierOff();
    _isStreaming = false;
    stopRequested = false;
    if (_shouldFreeAudioData && _audioData) {
        heap_caps_free(_audioData);
        _audioData = nullptr;
        _shouldFreeAudioData = false;
    }

    stopRequested = false;

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

    i2s_zero_dma_buffer(I2S_NUM_0);
    Serial.printf("Starting playback from offset %zu\n", offset);
    while (offset < self->_audioSize && !self->stopRequested) {
        size_t written = 0;
        size_t chunk = min<size_t>(2048, self->_audioSize - offset);

        int res = i2s_write(I2S_NUM_0, self->_audioData + offset, chunk, &written, portMAX_DELAY);
        if (res != ESP_OK) {
            Serial.printf("i2s_write error: %d\n", res);
        }
        offset += written;
    }
    Serial.printf("Playback finished\n");

    uint8_t* dataToFree = nullptr;
    if (self->_shouldFreeAudioData && self->_audioData) {
        dataToFree = self->_audioData;
        self->_audioData = nullptr;
        self->_shouldFreeAudioData = false;
    }

    if (dataToFree) {
        heap_caps_free(dataToFree);
    }
    self->_audioSize = 0;
    self->_shouldFreeAudioData = false;
    self->_isStreaming = false;

    self->stopI2S();
    self->amplifierOff();
    self->uninstallI2S();

    self->stopRequested = false;
    self->_task = nullptr;
    vTaskDelete(nullptr);
}

void AudioPlayer::stop() {
    stopRequested = true;
    if (_task) {
        while (_task) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    _isStreaming = false;
}

void AudioPlayer::stopStreaming() {
    if (!_isStreaming) return;

    Serial.println("Stopping streaming...");
    stopRequested = true;          
    vTaskDelay(pdMS_TO_TICKS(50)); 
    stopI2S();                     
    i2s_zero_dma_buffer(I2S_NUM_0);
    uninstallI2S();                
    amplifierOff();                

    _isStreaming = false;
    stopRequested = false;

    if (_audioData && _shouldFreeAudioData) {
        heap_caps_free(_audioData);
        _audioData = nullptr;
        _audioSize = 0;
        _shouldFreeAudioData = false;
    }
}


bool AudioPlayer::isPlaying() const { 
    return _task != nullptr; 
}

void AudioPlayer::setVolume(float v) { 
    _volume = constrain(v, 0.0f, 1.0f); 
}

static bool hasWavExtension(const String &name) {
    return name.endsWith(".wav");
}

bool AudioPlayer::playRandom(const char* directory) {
    if (_task || _isStreaming) {
        stopRequested = true;
        stopStreaming();
        while (_task) { vTaskDelay(pdMS_TO_TICKS(10)); }
    }
    stopI2S();
    uninstallI2S();
    amplifierOff();
    _isStreaming = false;
    stopRequested = false;
    if (_shouldFreeAudioData && _audioData) {
        heap_caps_free(_audioData);
        _audioData = nullptr;
        _shouldFreeAudioData = false;
    }

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

void AudioPlayer::streamUploadStart(size_t totalSize) {
    (void)totalSize;
    _uploadHeaderSkipped = false;
    _uploadHeaderBytes = 0;
    if (_isStreaming) {       // If already streaming, signal the current stream to stop
        stopRequested = true;
        while (_isStreaming) vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    installI2S();
    startI2S();
    vTaskDelay(20);
    amplifierOn();
    _isStreaming = true; 
    stopRequested = false;
    Serial.printf("Upload stream start: %zu bytes\n", totalSize);
}

void AudioPlayer::streamUploadWrite(const uint8_t* buf, size_t len) {
    if (!_i2sInstalled) return;
    if (stopRequested) {
        Serial.println("Stop detected during stream upload");
        return;
    }

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
    if (!_isStreaming) return;
    Serial.println("Upload stream end");
    amplifierOff();
    stopI2S();
    uninstallI2S();
    _uploadHeaderSkipped = false;
    _uploadHeaderBytes = 0;
    stopRequested = false;
    _isStreaming = false; 
    if (_audioData && _shouldFreeAudioData) {
        heap_caps_free(_audioData);
        _audioData = nullptr;
        _audioSize = 0;
        _shouldFreeAudioData = false;
    }
}

void AudioPlayer::streamUploadAbort() {
    Serial.println("Upload stream aborted");
    amplifierOff();
    stopI2S();
    uninstallI2S();
    _uploadHeaderSkipped = false;
    _uploadHeaderBytes = 0;
    stopRequested = false;
    _isStreaming = false; 
    if (_audioData && _shouldFreeAudioData) {
        heap_caps_free(_audioData);
        _audioData = nullptr;
        _audioSize = 0;
        _shouldFreeAudioData = false;
    }
}