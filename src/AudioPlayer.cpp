#include "AudioPlayer.h"
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct TaskArgs {
    AudioPlayer* self;
    char* filename;
};

AudioPlayer::AudioPlayer(int bck, int ws, int dout, int ampSdPin)
    : _bck(bck), _ws(ws), _dout(dout), _ampSdPin(ampSdPin)
{
    pinMode(_ampSdPin, OUTPUT);
    amplifierOff();
}

void AudioPlayer::initI2S() {
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

    i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    i2s_set_pin(I2S_NUM_0, &pins);
}

bool AudioPlayer::loadFileToRam(const char* filename) {
    File f = LittleFS.open(filename, "r");
    if (!f) return false;

    size_t size = f.size();
    uint8_t* buf = (uint8_t*)malloc(size);
    if (!buf) {
        f.close();
        return false;
    }

    f.read(buf, size);
    f.close();

    _audioData = buf;
    _audioSize = size;
    return true;
}

bool AudioPlayer::playFile(const String& filename) {
    if (_task) return false;

    _stopRequested = false;

    TaskArgs* args = new TaskArgs();
    args->self = this;
    args->filename = strdup(filename.c_str());
    if (!args->filename) {
        delete args;
        return false;
    }

    if (xTaskCreate(
            playTask,
            "AudioPlay",
            4096,
            args,
            1,
            &_task
        ) != pdPASS)
    {
        free(args->filename);
        delete args;
        _task = nullptr;
        return false;
    }

    return true;
}

void AudioPlayer::playTask(void* arg) {
    TaskArgs* args = static_cast<TaskArgs*>(arg);
    AudioPlayer* self = args->self;
    char* filename = args->filename;
    delete args;

    if (!self->loadFileToRam(filename)) {
        free(filename);
        self->_task = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    free(filename);

    self->installI2S();
    self->startI2S();
    self->amplifierOn();

    constexpr size_t WAV_HEADER_SIZE = 44;

    size_t offset = WAV_HEADER_SIZE;
    while (offset < self->_audioSize && !self->_stopRequested) {
        size_t written = 0;
        size_t chunk = min<size_t>(2048, self->_audioSize - offset);
        // apply volume
        int16_t* samples = (int16_t*)(self->_audioData + offset);
        size_t sampleCount = chunk / sizeof(int16_t);

        for (size_t i = 0; i < sampleCount; i++) {
            samples[i] = (int16_t)(samples[i] * self->_volume);
        }

        i2s_write(
            I2S_NUM_0,
            self->_audioData + offset,
            chunk,
            &written,
            portMAX_DELAY
        );

        offset += written;
    }

    free(self->_audioData);
    self->_audioData = nullptr;
    self->_audioSize = 0;

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
        if (!file.isDirectory() && hasWavExtension(file.name())) {
            wavFiles.push_back(String(directory) + "/" + file.name());
        }
        file = dir.openNextFile();
    }

    if (wavFiles.empty()) return false;

    int idx = random(wavFiles.size());

    return playFile(wavFiles[idx]);
}

void AudioPlayer::stop() {
    _stopRequested = true;
}

bool AudioPlayer::isPlaying() const {
    return _task != nullptr;
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

    i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    i2s_set_pin(I2S_NUM_0, &pins);

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
    i2s_start(I2S_NUM_0);
}

void AudioPlayer::stopI2S() {
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_stop(I2S_NUM_0);
}

void AudioPlayer::amplifierOn() {
    digitalWrite(_ampSdPin, HIGH);
}

void AudioPlayer::amplifierOff() {
    digitalWrite(_ampSdPin, LOW);
}

void AudioPlayer::setVolume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    _volume = v;
}
