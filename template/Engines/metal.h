
#pragma once
#include <cmath>

class Metal {
  private:
    float sampleRate = 48000.0f;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;

    float frequency = 220.0f;
    float texture = 0.5f;
    float drive = 5.0f;

    static constexpr float TWO_PI = 6.28318530718f;

    float Advance(float &phase, float hz) {
        phase += hz / sampleRate;
        if (phase >= 1.0f)
            phase -= 1.0f;

        return sinf(TWO_PI * phase);
    }

  public:
    void Init(float sr) {
        sampleRate = sr;
        phase1 = phase2 = phase3 = 0.0f;
    }

    void SetFrequency(float hz) {
        frequency = hz;
    }

    void SetTexture(float amount) {
        texture = amount;
    }

    void SetDrive(float amount) {
        drive = amount;
    }

    float Process() {
        // Inharmonic oscillator frequencies
        float ratio = 1.4f + texture * 5.0f;

        float osc1 = Advance(phase1, frequency);
        float osc2 = Advance(phase2, frequency * ratio);
        float osc3 = Advance(
            phase3, frequency * (2.73f + texture * 1.7f)
        );

        // Ring modulation creates metallic sidebands
        float ring = osc1 * osc2;

        // Blend increasingly dissonant components
        float signal = (1.0f - texture) * osc1
                     + texture * (0.7f * ring
                     + 0.3f * osc3);

        // Saturate
        signal = tanhf(signal * drive);

        return signal * 0.22f;
    }
};
