#ifndef __sampler_base_h__
#define __sampler_base_h__

#include "driver/i2s.h"

class SampleSource;

/**
 * Base Class for both the ADC and I2S sampler
 **/
class DACOutput
{
public:
    void start(SampleSource *sample_generator);
    void setVolume(float v);   // 0.0 … 1.0
    float getVolume();         // 0.0 … 1.0
    QueueHandle_t m_i2sQueue = nullptr;
    SampleSource *m_sample_generator = nullptr;
private:
    float m_volume = 1.0f;
};

#endif