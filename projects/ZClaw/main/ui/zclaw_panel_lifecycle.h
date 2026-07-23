#pragma once

namespace zclaw {

class PanelLifecycle {
public:
    bool mount()
    {
        if (phase_ != Phase::Empty)
            return false;
        phase_ = Phase::Mounted;
        return true;
    }

    bool begin_open()
    {
        if (phase_ != Phase::Mounted)
            return false;
        phase_ = Phase::Opening;
        return true;
    }

    bool begin_close()
    {
        if (phase_ != Phase::Mounted && phase_ != Phase::Open)
            return false;
        phase_ = Phase::Closing;
        return true;
    }

    bool complete_animation()
    {
        if (phase_ == Phase::Opening) {
            phase_ = Phase::Open;
            return false;
        }
        if (phase_ == Phase::Closing) {
            phase_ = Phase::Empty;
            return true;
        }
        return false;
    }

    void release()
    {
        phase_ = Phase::Empty;
    }

    bool mounted() const
    {
        return phase_ != Phase::Empty;
    }

    bool animating() const
    {
        return phase_ == Phase::Opening || phase_ == Phase::Closing;
    }

private:
    enum class Phase { Empty, Mounted, Opening, Open, Closing };
    Phase phase_ = Phase::Empty;
};

}  // namespace zclaw
