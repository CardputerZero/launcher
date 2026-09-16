/* SPDX-License-Identifier: MIT */
#include "wizard_key_sound.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

// The process tests replace the audio loop, so they need no audio server or device.
static char receive(int fd)
{
    pollfd input{fd, POLLIN, 0};
    assert(poll(&input, 1, 2000) == 1);
    char value = 0;
    assert(read(fd, &value, 1) == 1);
    return value;
}

static void expect_reaped()
{
    assert(waitpid(-1, nullptr, WNOHANG) == -1);
    assert(errno == ECHILD);
}

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--key-sound-worker") == 0) {
        const int report = std::atoi(std::getenv("WIZARD_TEST_REPORT_FD"));
        const char mode = *std::getenv("WIZARD_TEST_SOUND_MODE");
        assert(write(report, "r", 1) == 1);
        if (mode == 'h') {
            for (;;) pause();
        }
        if (mode == 'e') {
            shutdown(STDIN_FILENO, SHUT_RDWR);
            assert(write(report, "e", 1) == 1);
            return 0;
        }
        char cue;
        while (read(STDIN_FILENO, &cue, 1) == 1)
            assert(write(report, &cue, 1) == 1);
        return 0;
    }

    int reports[2];
    assert(pipe(reports) == 0);
    char fd[32];
    std::snprintf(fd, sizeof(fd), "%d", reports[1]);
    setenv("WIZARD_TEST_REPORT_FD", fd, 1);
    setenv("WIZARD_TEST_SOUND_MODE", "normal", 1);
    launch_wizard::WizardKeySound sound;
    sound.play(); // Safe before setup.
    sound.start();
    assert(receive(reports[0]) == 'r');
    sound.start(); // Idempotent: no second ready notification.
    sound.play();
    assert(receive(reports[0]) == 'k');
    using launch_wizard::WizardSoundCue;
    for (WizardSoundCue cue : {WizardSoundCue::Lock, WizardSoundCue::Unlock,
                              WizardSoundCue::Error, WizardSoundCue::Confirm,
                              WizardSoundCue::Complete}) {
        sound.play(cue);
        assert(receive(reports[0]) == static_cast<char>(cue));
    }
    sound.stop();
    sound.stop();
    expect_reaped();

    sound.start(); // The wizard resumes sounds after account migration.
    assert(receive(reports[0]) == 'r');
    sound.play();
    assert(receive(reports[0]) == 'k');
    sound.stop();
    expect_reaped();

    setenv("WIZARD_TEST_SOUND_MODE", "exit", 1);
    sound.start();
    assert(receive(reports[0]) == 'r');
    assert(receive(reports[0]) == 'e');
    sound.play(); // Closed peer must not deliver SIGPIPE to the wizard.
    sound.stop();
    expect_reaped();

    setenv("WIZARD_TEST_SOUND_MODE", "hang", 1);
    {
        launch_wizard::WizardKeySound stalled;
        stalled.start();
        assert(receive(reports[0]) == 'r');
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 100000; ++i) stalled.play();
        assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
        stalled.stop();
        assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(3));
    }
    expect_reaped();
    close(reports[0]);
    close(reports[1]);
}
