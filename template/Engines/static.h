
#pragma once
#include <cmath>
#include <cstdint>

class Static {
  private:
    float sampleRate = 48000.0f;

    float frequency = 1500.0f;
    float texture = 0.5f;
    float drive = 5.0f;

    uint32_t randomState = 123456789u;

    float lowState = 0.0f;
    float highState = 0.0f;

    float lowCoeff = 0.1f;
    float highCoeff = 0.02f;

    float heldNoise = 0.0f;
    int holdCounter = 0;

    float Random() {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;

        return (float)(randomState & 0xFFFF) / 32767.5f - 1.0f;
    }

  public:
    void Init(float sr) {
        sampleRate = sr;
        lowState = 0.0f;
        highState = 0.0f;
        holdCounter = 0;
    }

    void SetFrequency(float hz) {
        frequency = hz;

        float hp = frequency * 0.35f;
        float lp = frequency * 2.5f;

        if (lp > sampleRate * 0.45f)
            lp = sampleRate * 0.45f;

        highCoeff = 1.0f - expf(
            -6.2831853f * hp / sampleRate
        );

        lowCoeff = 1.0f - expf(
            -6.2831853f * lp / sampleRate
        );
    }

    void SetTexture(float amount) {
        texture = amount;
    }

    void SetDrive(float amount) {
        drive = amount;
    }

    float Process() {
        float noise = Random();

        // Variable-rate sample and hold
        int holdLength = 1 + (int)(
            texture * texture * 180.0f
        );

        if (holdCounter <= 0) {
            heldNoise = Random();
            holdCounter = holdLength;
        }

        holdCounter--;

        // Blend broadband noise with digital crackle
        float signal = noise * (1.0f - texture)
                     + heldNoise * texture;

        // Highpass stage
        highState += highCoeff * (signal - highState);
        signal -= highState;

        // Lowpass stage
        lowState += lowCoeff * (signal - lowState);
        signal = lowState;

        // Aggressive distortion
        signal = tanhf(signal * drive);

        return signal * 0.28f;
    }
};
