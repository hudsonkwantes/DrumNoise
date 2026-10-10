
#pragma once

#include <cmath>
#include <cstdint>

class LowEnd {
private:
    static constexpr float TWO_PI = 6.28318530718f;

    float sampleRate = 48000.0f;
    float phase = 0.0f;
    float envelopeTime = 0.0f;
    float stepDuration = 0.125f;

    float frequency = 50.0f;
    float texture = 0.5f;
    float drive = 5.0f;

    float noiseHP = 0.0f;
    float bodyLP = 0.0f;

    uint32_t randomState = 135792468u;

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
        phase = 0.0f;
        envelopeTime = 0.0f;
        noiseHP = 0.0f;
        bodyLP = 0.0f;
    }

    void SetFrequency(float hz) {
        frequency = hz;
    }

    void SetPitchEnvelope(float amount) {
        texture = amount;
    }

    void SetDrive(float amount) {
        drive = amount;
    }

    // Called by the master clock once per sixteenth note.
    void ClockStep(float bpm) {
        if(bpm < 40.0f) bpm = 40.0f;
        if(bpm > 240.0f) bpm = 240.0f;

        stepDuration = 60.0f / (bpm * 4.0f);
        envelopeTime = 0.0f;
    }

    float Process() {
        float normalizedTime =
            envelopeTime / stepDuration;

        if(normalizedTime > 1.0f)
            normalizedTime = 1.0f;

        // Pitch sweep decays relative to step length
        float pitchEnv = expf(-normalizedTime * 8.0f);

        float hz = frequency *
            (1.0f + texture * 3.0f * pitchEnv);

        phase += hz / sampleRate;
        if(phase >= 1.0f)
            phase -= 1.0f;

        float angle = TWO_PI * phase;

        // Midrange-heavy oscillator
        float body =
            sinf(angle)
            + 0.45f * sinf(2.0f * angle)
            + 0.25f * sinf(3.0f * angle);

        body = tanhf(body * (2.0f + drive));

        bodyLP += 0.18f * (body - bodyLP);
        body = bodyLP;

        // Clock-synchronized noisy attack
        float attackEnv =
            expf(-normalizedTime * 14.0f);

        float noise = Random();

        noiseHP += 0.08f * (noise - noiseHP);

        float attack =
            (noise - noiseHP) *
            attackEnv *
            (0.2f + texture * 1.5f);

        float signal = body * 0.75f + attack;

        signal = tanhf(signal * drive);

        // Envelope runs independently of the gate.
        envelopeTime += 1.0f / sampleRate;

        return signal * 0.22f;
    }
};
