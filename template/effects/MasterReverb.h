
#pragma once

#include <cmath>

class MasterReverb {
private:
    static constexpr int SIZE_A = 4013;
    static constexpr int SIZE_B = 4787;
    static constexpr int SIZE_C = 3371;
    static constexpr int SIZE_D = 3911;

    float delayA[SIZE_A] = {};
    float delayB[SIZE_B] = {};
    float delayC[SIZE_C] = {};
    float delayD[SIZE_D] = {};

    int indexA = 0;
    int indexB = 0;
    int indexC = 0;
    int indexD = 0;

    float dampA = 0.0f;
    float dampB = 0.0f;
    float dampC = 0.0f;
    float dampD = 0.0f;

    float mix = 0.0f;

public:
    void Init(float sampleRate) {
        (void)sampleRate;

        indexA = indexB = indexC = indexD = 0;
        dampA = dampB = dampC = dampD = 0.0f;
        mix = 0.0f;

        for(int i = 0; i < SIZE_A; ++i) delayA[i] = 0.0f;
        for(int i = 0; i < SIZE_B; ++i) delayB[i] = 0.0f;
        for(int i = 0; i < SIZE_C; ++i) delayC[i] = 0.0f;
        for(int i = 0; i < SIZE_D; ++i) delayD[i] = 0.0f;
    }

    void SetMix(float amount) {
        mix = amount;
        if(mix < 0.0f) mix = 0.0f;
        if(mix > 1.0f) mix = 1.0f;
    }

    void Process(float input, float &left, float &right) {
        float a = delayA[indexA];
        float b = delayB[indexB];
        float c = delayC[indexC];
        float d = delayD[indexD];

        // Damping in the feedback paths
        dampA += 0.25f * (a - dampA);
        dampB += 0.22f * (b - dampB);
        dampC += 0.28f * (c - dampC);
        dampD += 0.20f * (d - dampD);

        // Cross-coupled feedback network
        delayA[indexA] = tanhf(input * 0.35f + dampD * 0.76f);
        delayB[indexB] = tanhf(input * 0.35f + dampA * 0.74f);
        delayC[indexC] = tanhf(input * 0.35f + dampB * 0.77f);
        delayD[indexD] = tanhf(input * 0.35f + dampC * 0.73f);

        if(++indexA >= SIZE_A) indexA = 0;
        if(++indexB >= SIZE_B) indexB = 0;
        if(++indexC >= SIZE_C) indexC = 0;
        if(++indexD >= SIZE_D) indexD = 0;

        float wetLeft = (a + c) * 0.5f;
        float wetRight = (b + d) * 0.5f;

        left = input * (1.0f - mix) + wetLeft * mix;
        right = input * (1.0f - mix) + wetRight * mix;
    }
};
