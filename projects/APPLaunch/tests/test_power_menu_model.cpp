/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../main/ui/model/power_menu_model.hpp"

#include <cassert>
#include <initializer_list>
#include <cstring>

int main()
{
    PowerMenuModel model;
    assert(!model.is_open());
    assert(!model.handle(PowerMenuKey::Enter).execute);

    // Opens on Cancel; Enter on Cancel just closes.
    model.open();
    assert(model.stage() == PowerMenuStage::Choosing);
    assert(model.selected() == PowerMenuItem::Cancel);
    auto decision = model.handle(PowerMenuKey::Enter);
    assert(decision.close && !decision.execute && !model.is_open());

    // ESC closes from the list.
    model.open();
    decision = model.handle(PowerMenuKey::Back);
    assert(decision.close && !decision.execute && !model.is_open());

    // Navigation wraps in both directions.
    model.open();
    decision = model.handle(PowerMenuKey::Previous);
    assert(decision.navigated && model.selected() == PowerMenuItem::Shutdown);
    decision = model.handle(PowerMenuKey::Next);
    assert(model.selected() == PowerMenuItem::Cancel);
    model.handle(PowerMenuKey::Next);
    assert(model.selected() == PowerMenuItem::Lock);
    model.handle(PowerMenuKey::Next);
    assert(model.selected() == PowerMenuItem::Reboot);
    model.handle(PowerMenuKey::Next);
    assert(model.selected() == PowerMenuItem::Shutdown);
    model.handle(PowerMenuKey::Next);
    assert(model.selected() == PowerMenuItem::Cancel);
    assert(!model.handle(PowerMenuKey::Other).redraw);

    // Every action needs an explicit OK; ESC from the confirmation returns to the list.
    for (PowerMenuItem item : {PowerMenuItem::Lock, PowerMenuItem::Reboot, PowerMenuItem::Shutdown}) {
        model.open();
        while (model.selected() != item) model.handle(PowerMenuKey::Next);
        decision = model.handle(PowerMenuKey::Enter);
        assert(!decision.execute && decision.redraw);
        assert(model.stage() == PowerMenuStage::Confirming);
        assert(!model.handle(PowerMenuKey::Next).redraw);
        assert(model.selected() == item);

        decision = model.handle(PowerMenuKey::Back);
        assert(!decision.close && !decision.execute);
        assert(model.stage() == PowerMenuStage::Choosing && model.selected() == item);

        model.handle(PowerMenuKey::Enter);
        decision = model.handle(PowerMenuKey::Enter);
        assert(decision.execute && decision.close && decision.action == item);
        assert(!model.is_open());
        assert(std::strlen(PowerMenuModel::confirm_title(item)) > 0);
    }

    assert(std::strcmp(PowerMenuModel::label(PowerMenuItem::Cancel), "Cancel") == 0);
    assert(std::strcmp(PowerMenuModel::label(PowerMenuItem::Shutdown), "Shutdown") == 0);
    assert(PowerMenuModel::item_at(99) == PowerMenuItem::Cancel);
    return 0;
}
