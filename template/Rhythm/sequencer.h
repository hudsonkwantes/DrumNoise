
#pragma once
#include <cstdint>

class Sequencer {
public:
    static constexpr int VOICES = 5;
    static constexpr int STEPS = 16;

    void Init() {
        for(int v = 0; v < VOICES; ++v) {
            start_[v] = 0;
            end_[v] = 15;
            speed_[v] = 2;

            for(int s = 0; s < STEPS; ++s)
                pattern_[v][s] = false;
        }
    }

    void ClearPatterns() {
        for(int v = 0; v < VOICES; ++v)
            for(int s = 0; s < STEPS; ++s)
                pattern_[v][s] = false;
    }

    void SetLoop(int v, int start, int end) {
        if(v < 0 || v >= VOICES) return;

        start = Clamp(start);
        end = Clamp(end);

        if(start > end) end = start;

        start_[v] = start;
        end_[v] = end;
    }

    void SetSpeed(int v, int index) {
        if(v < 0 || v >= VOICES) return;

        if(index < 0) index = 0;
        if(index > 4) index = 4;

        speed_[v] = index;
    }

    int CurrentStep(int v, uint32_t ticks) const {
        if(v < 0 || v >= VOICES) return 0;

        // 1/4x, 1/2x, 1x, 2x, 4x.
        static constexpr int ticksPerStep[5] = {
            16, 8, 4, 2, 1
        };

        uint32_t count = ticks / ticksPerStep[speed_[v]];
        int length = end_[v] - start_[v] + 1;

        return start_[v] + (count % length);
    }

    void Record(int v, uint32_t ticks) {
        if(v < 0 || v >= VOICES) return;

        int step = CurrentStep(v, ticks);
        pattern_[v][step] = true;
    }

    bool Gate(int v, uint32_t ticks) const {
        if(v < 0 || v >= VOICES) return false;

        return pattern_[v][CurrentStep(v, ticks)];
    }

private:
    bool pattern_[VOICES][STEPS] = {};
    int start_[VOICES] = {};
    int end_[VOICES] = {};
    int speed_[VOICES] = {};

    int Clamp(int step) const {
        if(step < 0) return 0;
        if(step > 15) return 15;
        return step;
    }
};
