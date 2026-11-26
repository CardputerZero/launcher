/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl/lvgl.h"

#include <array>
#include <cstddef>

namespace launcher_grid_layout {

// 3x2 app grid, one page shows six apps.  Cells keep the carousel's
// center-offset convention: coordinates are offsets from the container
// centre (the container is a 320x150 object aligned CENTER).
inline constexpr size_t kCols = 3;
inline constexpr size_t kRows = 2;
inline constexpr size_t kCells = kCols * kRows;

// Square cards: the icon artwork is square, so cards stay square and the
// title lives under the card inside the same cell.
inline constexpr lv_coord_t kCardSize = 58;
inline constexpr lv_coord_t kCardRadius = 14;
inline constexpr lv_coord_t kTitleWidth = 96;

struct Cell
{
    lv_coord_t card_x;
    lv_coord_t card_y;
    lv_coord_t title_x;
    lv_coord_t title_y;
};

inline constexpr std::array<Cell, kCells> kCellLayout = {{
    // row 0: cards y -44 (spans -73..-15), titles y -8
    {-107, -44, -107, -8},
    {   0, -44,    0, -8},
    { 107, -44,  107, -8},
    // row 1: cards y 32 (spans 3..61), titles y 67
    {-107,  32, -107, 67},
    {   0,  32,    0, 67},
    { 107,  32,  107, 67},
}};

static_assert(kCellLayout.size() == kCells);

} // namespace launcher_grid_layout
