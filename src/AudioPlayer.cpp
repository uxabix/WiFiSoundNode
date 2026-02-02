#include "AudioPlayer.h"
#include <driver/i2s.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct PlayTaskParams {
    AudioPlayer* self;
    String filename;
};

void AudioPlayer::playTask(void* pvParameters) {
    PlayTaskParams* params = static_cast<PlayTaskParams*>(pvParameters);
    AudioPlayer* self = params->self;
    String filename = params->filename;
    delete params;

    File file = LittleFS.open(filename, "r");
    if (!file) {
        self->_taskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    const size_t BUF_SIZE = 512;
    uint8_t buffer[BUF_SIZE];

    while (file.available() && !self->_stopRequested) {
        size_t bytesRead = file.read(buffer, BUF_SIZE);
        if (bytesRead == 0) break;
        size_t bytesWritten = 0;
        i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
        // allow other tasks to run
        vTaskDelay(1);
    }

    file.close();
    self->_stopRequested = false;
    self->_taskHandle = nullptr;
    vTaskDelete(nullptr);
}

AudioPlayer::AudioPlayer(int bck, int ws, int dout) : _bck(bck), _ws(ws), _dout(dout) {
    initI2S();
}

void AudioPlayer::initI2S(){
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = _bck,
        .ws_io_num = _ws,
        .data_out_num = _dout,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, nullptr);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

bool AudioPlayer::playFile(const String &filename){
    if (_taskHandle != nullptr) return false; // already playing

    _stopRequested = false;

    PlayTaskParams* params = new PlayTaskParams();
    params->self = this;
    params->filename = filename;

    BaseType_t res = xTaskCreatePinnedToCore(
        playTask,
        "AudioPlay",
        8192,
        params,
        1,
        &_taskHandle,
        1
    );

    if (res != pdPASS) {
        delete params;
        _taskHandle = nullptr;
        return false;
    }

    return true;
}

void AudioPlayer::stop(){
    if (_taskHandle == nullptr) return;
    _stopRequested = true;
    // wait for task to clear handle
    uint32_t wait = 0;
    while (_taskHandle != nullptr && wait++ < 500) {
        vTaskDelay(1);
    }
}

bool AudioPlayer::isPlaying(){
    return _taskHandle != nullptr;
}
