/* SPDX-License-Identifier: MIT */
#pragma once

#include <sys/types.h>

namespace launch_wizard {

enum class WizardSoundCue : char {
    Typing = 'k',
    Lock = 'l',
    Unlock = 'u',
    Error = 'e',
    Confirm = 'n',
    Complete = 'a',
};

class WizardKeySound {
public:
    ~WizardKeySound();
    WizardKeySound() = default;
    WizardKeySound(const WizardKeySound &) = delete;
    WizardKeySound &operator=(const WizardKeySound &) = delete;

    void start();
    void play(WizardSoundCue cue = WizardSoundCue::Typing);
    void stop();

private:
    int socket_ = -1;
    pid_t child_ = -1;
};

int run_key_sound_worker();

} // namespace launch_wizard
