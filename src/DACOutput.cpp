#include <Arduino.h>
#include "driver/i2s.h"

#include "DACOutput.h"
#include "SampleSource.h"

#define NUM_FRAMES 128

static void i2sWriterTask(void *param)
{
    DACOutput *output = static_cast<DACOutput *>(param);

    Frame_t frames[NUM_FRAMES];
    uint16_t i2sBuffer[NUM_FRAMES];
    i2s_zero_dma_buffer(I2S_NUM_0); // обнуляем все DMA буферы
    uint16_t zeroBuffer[NUM_FRAMES] = {0};
    size_t written;
    i2s_write(I2S_NUM_0, zeroBuffer, sizeof(zeroBuffer), &written, portMAX_DELAY);

    while (true)
    {
        // читаем PCM
        output->m_sample_generator->getFrames(frames, NUM_FRAMES);

        for (int i = 0; i < NUM_FRAMES; i++)
        {
            int32_t s = frames[i].left;

            // громкость
            s = (int32_t)(s * output->getVolume());

            // clamp
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;

            // PCM16 -> DAC8
            uint8_t dac = (uint8_t)((s + 32768) >> 8);

            // DAC использует MSB
            i2sBuffer[i] = dac << 8;
        }

        size_t written;
        i2s_write(
            I2S_NUM_0,
            i2sBuffer,
            sizeof(i2sBuffer),
            &written,
            portMAX_DELAY
        );

        if (output->m_sample_generator->finished())
        {
            i2s_zero_dma_buffer(I2S_NUM_0);
            vTaskDelete(NULL);
        }
    }
}


void DACOutput::setVolume(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    m_volume = v;
}

float DACOutput::getVolume()
{
    return m_volume;
}

void DACOutput::start(SampleSource *sample_generator)
{
    m_sample_generator = sample_generator;

    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(
            I2S_MODE_MASTER |
            I2S_MODE_TX |
            I2S_MODE_DAC_BUILT_IN
        ),
        .sample_rate = 22050, // СОВЕТ: начни с 16k
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
};


    i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN); // ТОЛЬКО ОДИН
    i2s_zero_dma_buffer(I2S_NUM_0);


    xTaskCreate(
        i2sWriterTask,
        "i2s_dac_writer",
        4096,
        this,
        1,
        nullptr
    );
}
