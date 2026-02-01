#include "TestToneSource.h"
#include <math.h>

#define TWO_PI 6.28318530718f

TestToneSource::TestToneSource(int sampleRate)
    : m_sampleRate(sampleRate), m_phase(0.0f)
{
}

int TestToneSource::sampleRate()
{
    return m_sampleRate;
}

void TestToneSource::getFrames(Frame_t *frames, int number_frames)
{
    const float freq = 440.0f;          // LA — идеально различим
    const float step = TWO_PI * freq / m_sampleRate;

    for (int i = 0; i < number_frames; i++)
    {
        float s = sinf(m_phase);
        int16_t sample = (int16_t)(s * 30000); // почти максимум

        frames[i].left  = sample;
        frames[i].right = sample;

        m_phase += step;
        if (m_phase >= TWO_PI)
            m_phase -= TWO_PI;
    }
}

bool TestToneSource::finished()
{
    return false; // бесконечный тон
}
