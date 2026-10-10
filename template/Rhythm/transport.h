
#pragma once

class Transport {
public:
    enum Mode { STOP, PLAY, RECORD };

    void Init() {
        mode_ = STOP;
    }

    void SetMode(Mode mode) {
        mode_ = mode;
    }

    Mode GetMode() const {
        return mode_;
    }

    bool Running() const {
        return mode_ != STOP;
    }

    bool Recording() const {
        return mode_ == RECORD;
    }

private:
    Mode mode_ = STOP;
};
