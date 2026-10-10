
#pragma once
#include <cstdint>

class Clock {
public:
    void Init(float sampleRate) {
        sampleRate_ = sampleRate;
        Reset();
    }

    void Reset() {
        phase_ = 0.0f;
        ticks_ = 0;
    }

    // Four ticks per sixteenth note: supports 4x speed.
    bool Process(float bpm) {
        if(bpm < 40.0f) bpm = 40.0f;
        if(bpm > 240.0f) bpm = 240.0f;

        phase_ += (bpm * 16.0f) /
                  (60.0f * sampleRate_);

        if(phase_ >= 1.0f) {
            phase_ -= 1.0f;
            ++ticks_;
            return true;
        }
        return false;
    }

    uint32_t Ticks() const { return ticks_; }
    int Step() const { return (ticks_ / 4) % 16; }
    float Phase() const { return phase_; }

private:
    float sampleRate_ = 48000.0f;
    float phase_ = 0.0f;
    uint32_t ticks_ = 0;
};
