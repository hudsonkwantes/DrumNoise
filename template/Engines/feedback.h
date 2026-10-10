
#pragma once
#include <cmath>
#include <cstdint>

class Feedback {
  private:
    static constexpr int BUFFER_SIZE = 1024;

    float buffer[BUFFER_SIZE] = {};
    int writeIndex = 0;

    float sampleRate = 48000.0f;
    float frequency = 220.0f;
    float texture = 0.5f;
    float drive = 5.0f;

    float feedbackLowpass = 0.0f;
    float feedbackDC = 0.0f;

    uint32_t randomState = 987654321u;

    float Random() {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;

        return (float)(randomState & 0xFFFF)
               / 32767.5f - 1.0f;
    }

  public:
    void Init(float sr) {
        sampleRate = sr;
        writeIndex = 0;
        feedbackLowpass = 0.0f;
        feedbackDC = 0.0f;

        for(int i = 0; i < BUFFER_SIZE; i++)
            buffer[i] = 0.0f;
    }

    void SetFrequency(float hz) {
        frequency = hz;
    }

    void SetTexture(float amount) {
        texture = fmaxf(0.0f, fminf(1.0f, amount));
    }

    void SetDrive(float amount) {
        drive = amount;
    }

    float Process() {
        // Delay length determines resonance
        int delaySamples = (int)(sampleRate / frequency);

        if(delaySamples < 2)
            delaySamples = 2;

        if(delaySamples >= BUFFER_SIZE)
            delaySamples = BUFFER_SIZE - 1;

        int readIndex = writeIndex - delaySamples;

        if(readIndex < 0)
            readIndex += BUFFER_SIZE;

        float delayed = buffer[readIndex];

        // Continuous noise excitation
        float excitation = Random() * 0.08f;

        // Feedback grows with texture
        float feedbackAmount = 0.45f + texture * 0.52f;

        // Filter the feedback path
        float cutoff = 0.05f + texture * 0.65f;

        feedbackLowpass += cutoff *
                          (delayed - feedbackLowpass);

        // Remove DC from feedback
        feedbackDC += 0.001f *
                      (feedbackLowpass - feedbackDC);

        float filtered = feedbackLowpass - feedbackDC;

        // Nonlinear feedback
        float feedbackSignal = tanhf(
            filtered * (1.0f + texture * 4.0f)
        );

        // Feed the delayed signal back into the buffer
        float next = excitation
                   + feedbackSignal * feedbackAmount;

        buffer[writeIndex] = tanhf(next);

        writeIndex++;

        if(writeIndex >= BUFFER_SIZE)
            writeIndex = 0;

        // Output distortion
        float signal = tanhf(
            filtered * drive
        );

        // Blend in additional abrasive noise
        signal += Random() * texture * 0.12f;

        return tanhf(signal) * 0.22f;
    }
};
