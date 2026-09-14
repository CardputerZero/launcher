/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "../src/cp0/cp0_bluetooth_recovery.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
    using namespace cp0::bluetooth::recovery;
    for (int off_code : {0, -1}) {
        for (int on_code : {0, -1}) {
            std::vector<bool> requests;
            Completion pending;
            int completions = 0;
            int result = 42;
            std::string detail;
            restart([&](bool enabled, Completion done) {
                requests.push_back(enabled);
                pending = std::move(done);
            }, [&](int code, std::string data) {
                ++completions;
                result = code;
                detail = std::move(data);
            });
            assert(requests == std::vector<bool>{false});
            assert(completions == 0);
            auto off_done = std::move(pending);
            off_done(off_code, off_code == 0 ? "ok" : "timeout");
            assert((requests == std::vector<bool>{false, true}));
            assert(completions == 0);
            // Completion remains alive after the initiating call has returned.
            auto on_done = std::move(pending);
            on_done(on_code, on_code == 0 ? "" : "adapter unavailable");
            assert(completions == 1);
            assert((result == 0) == (off_code == 0 && on_code == 0));
            if (on_code != 0) assert(detail == "Bluetooth power-on failed.");
            else if (off_code != 0) assert(detail == "Bluetooth power-off failed.");
            else assert(detail.empty());
        }
    }
    for (bool fail_on : {false, true}) {
        std::vector<bool> requests;
        int completions = 0;
        restart([&](bool enabled, Completion done) {
            requests.push_back(enabled);
            if (enabled == fail_on) throw std::runtime_error("dispatch failed");
            done(0, {});
        }, [&](int code, std::string) {
            assert(code != 0);
            ++completions;
        });
        assert((requests == std::vector<bool>{false, true}));
        assert(completions == 1);
    }
}
