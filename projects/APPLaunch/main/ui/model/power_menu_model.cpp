/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "power_menu_model.hpp"

namespace {

constexpr PowerMenuItem kItems[PowerMenuModel::kItemCount] = {
    PowerMenuItem::Cancel,
    PowerMenuItem::Lock,
    PowerMenuItem::Reboot,
    PowerMenuItem::Shutdown,
};

} // namespace

const char *PowerMenuModel::label(PowerMenuItem item)
{
    switch (item) {
        case PowerMenuItem::Lock:
            return "Lock";
        case PowerMenuItem::Reboot:
            return "Reboot";
        case PowerMenuItem::Shutdown:
            return "Shutdown";
        case PowerMenuItem::Cancel:
        default:
            return "Cancel";
    }
}

const char *PowerMenuModel::confirm_title(PowerMenuItem item)
{
    switch (item) {
        case PowerMenuItem::Lock:
            return "Lock the screen?";
        case PowerMenuItem::Reboot:
            return "Reboot the system?";
        case PowerMenuItem::Shutdown:
            return "Shut down the system?";
        case PowerMenuItem::Cancel:
        default:
            return "";
    }
}

PowerMenuItem PowerMenuModel::item_at(size_t index)
{
    return index < kItemCount ? kItems[index] : PowerMenuItem::Cancel;
}

void PowerMenuModel::open()
{
    stage_ = PowerMenuStage::Choosing;
    selected_ = 0;  // Cancel is the default.
}

void PowerMenuModel::close()
{
    stage_ = PowerMenuStage::Closed;
    selected_ = 0;
}

PowerMenuDecision PowerMenuModel::handle(PowerMenuKey key)
{
    PowerMenuDecision decision;
    switch (stage_) {
        case PowerMenuStage::Choosing:
            switch (key) {
                case PowerMenuKey::Previous:
                    selected_ = (selected_ + kItemCount - 1) % kItemCount;
                    decision.redraw = decision.navigated = true;
                    break;
                case PowerMenuKey::Next:
                    selected_ = (selected_ + 1) % kItemCount;
                    decision.redraw = decision.navigated = true;
                    break;
                case PowerMenuKey::Enter:
                    if (selected() == PowerMenuItem::Cancel) {
                        close();
                        decision.close = true;
                    } else {
                        stage_ = PowerMenuStage::Confirming;
                        decision.redraw = true;
                    }
                    break;
                case PowerMenuKey::Back:
                    close();
                    decision.close = true;
                    break;
                case PowerMenuKey::Other:
                default:
                    break;
            }
            break;
        case PowerMenuStage::Confirming:
            if (key == PowerMenuKey::Enter) {
                decision.execute = true;
                decision.action = selected();
                close();
                decision.close = true;
            } else if (key == PowerMenuKey::Back) {
                // Back to the list with the same entry still highlighted.
                stage_ = PowerMenuStage::Choosing;
                decision.redraw = true;
            }
            break;
        case PowerMenuStage::Closed:
        default:
            break;
    }
    return decision;
}
