#pragma once
#include "SampleSource.h"

class TestToneSource : public SampleSource
{
public:
    TestToneSource(int sampleRate = 22050);

    int sampleRate() override;
    void getFrames(Frame_t *frames, int number_frames) override;
    bool finished() override;

private:
    int m_sampleRate;
    float m_phase;
};
