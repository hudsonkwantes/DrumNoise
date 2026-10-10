
#pragma once

#include <cmath>
#include "../samples/RadioVoice_Small.h"

class Sample {
  private:
    float sampleRate = 48000.0f;
    float position = 0.0f;

    float speed = 1.0f;
    float texture = 0.0f;
    float drive = 5.0f;

    float heldSample = 0.0f;
    int holdCounter = 0;

    static constexpr int FADE_LENGTH = 128;

    float ReadSample(float index) {
        int i0 = (int)index;
        int i1 = i0 + 1;

        if(i1 >= (int)radio_voice::kSampleCount)
            i1 = 0;

        float fraction = index - (float)i0;

        float a = (float)radio_voice::kSamples[i0] / 128.0f;
        float b = (float)radio_voice::kSamples[i1] / 128.0f;

        return a + (b - a) * fraction;
    }

  public:
    void Init(float sr) {
        sampleRate = sr;
        position = 0.0f;
        holdCounter = 0;
        heldSample = 0.0f;
    }

    void SetFrequency(float value) {
        // Universal pitch control: playback speed
        speed = fmaxf(0.25f, fminf(4.0f, value));
    }

    void SetTexture(float amount) {
        texture = fmaxf(0.0f, fminf(1.0f, amount));
    }

    void SetDrive(float amount) {
        drive = fmaxf(1.0f, amount);
    }

    float Process() {
        // Convert 12 kHz sample rate to 48 kHz output
        float increment =
            speed * (float)radio_voice::kSampleRate
            / sampleRate;

        float signal = ReadSample(position);

        // Crossfade the end into the beginning
        float fadeStart =
            (float)radio_voice::kSampleCount
            - FADE_LENGTH;

        if(position >= fadeStart) {
            float blend =
                (position - fadeStart) / FADE_LENGTH;

            float beginning =
                ReadSample(position - fadeStart);

            signal = signal * (1.0f - blend)
                   + beginning * blend;
        }

        // Advance continuously
        position += increment;

        if(position >= radio_voice::kSampleCount) {
            position -= radio_voice::kSampleCount
                        - FADE_LENGTH;
        }

        // Sample-rate reduction
        int holdLength =
            1 + (int)(texture * texture * 48.0f);

        if(holdCounter <= 0) {
            heldSample = signal;
            holdCounter = holdLength;
        }

        holdCounter--;

        // Bit crushing
        float levels =
            4096.0f - texture * 4088.0f;

        float crushed =
            roundf(heldSample * levels) / levels;

        signal = signal * (1.0f - texture)
               + crushed * texture;

        // Saturation
        signal = tanhf(signal * drive);

        return signal * 0.30f;
    }
};
