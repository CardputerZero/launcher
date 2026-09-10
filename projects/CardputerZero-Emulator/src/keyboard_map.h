/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
/**
 * @file keyboard_map.h
 * @brief Maps SDL keyboard scancodes to CardputerZero TCA8418 key codes
 *        and ultimately to LVGL key values.
 *
 * Physical keyboard layout of the M5CardputerZero:
 *
 * Row4: [Esc(0x01)]          [1(0x02)]                                         [Tab(0x0f)]
 * Row0:                       [2(0x03)][3(0x04)][4(0x05)][5(0x06)][6(0x07)][7(0x08)][8(0x09)][9(0x0a)][0(0x0b)][Backspace(0x0e)]
 * Row1: [Q(0x10)][W(0x11)][E(0x12)][R(0x13)][T(0x14)][Y(0x15)][U(0x16)][I(0x17)][O(0x18)][P(0x19)]
 * Row2: [Shift(0x2a)][A(0x1e)][S(0x1f)][D(0x20)][F(0x21)][G(0x22)][H(0x23)][J(0x24)][K(0x25)][L(0x26)][Enter(0x1c)]
 * Row3: [Ctrl(0x1d)][Alt(0x38)][Z(0x2c)][X(0x2d)][C(0x2e)][V(0x2f)][B(0x30)][N(0x31)][M(0x32)][Space(0x39)]
 *
 * Host key mapping:
 *   F1 = toggle Sym layer (for symbol characters)
 *   F2 = toggle Fn layer  (for navigation keys)
 */

#include <cstdint>
#include <SDL2/SDL.h>

/* TCA8418 key codes matching the physical keyboard firmware */
namespace TCA8418 {
    /* Row4 */
    constexpr uint8_t KEY_ESC       = 0x01;
    constexpr uint8_t KEY_1         = 0x02;
    constexpr uint8_t KEY_TAB       = 0x0F;
    /* Row0 */
    constexpr uint8_t KEY_2         = 0x03;
    constexpr uint8_t KEY_3         = 0x04;
    constexpr uint8_t KEY_4         = 0x05;
    constexpr uint8_t KEY_5         = 0x06;
    constexpr uint8_t KEY_6         = 0x07;
    constexpr uint8_t KEY_7         = 0x08;
    constexpr uint8_t KEY_8         = 0x09;
    constexpr uint8_t KEY_9         = 0x0A;
    constexpr uint8_t KEY_0         = 0x0B;
    constexpr uint8_t KEY_BACKSPACE = 0x0E;
    /* Row1 */
    constexpr uint8_t KEY_Q         = 0x10;
    constexpr uint8_t KEY_W         = 0x11;
    constexpr uint8_t KEY_E         = 0x12;
    constexpr uint8_t KEY_R         = 0x13;
    constexpr uint8_t KEY_T         = 0x14;
    constexpr uint8_t KEY_Y         = 0x15;
    constexpr uint8_t KEY_U         = 0x16;
    constexpr uint8_t KEY_I         = 0x17;
    constexpr uint8_t KEY_O         = 0x18;
    constexpr uint8_t KEY_P         = 0x19;
    /* Row2 */
    constexpr uint8_t KEY_SHIFT     = 0x2A;
    constexpr uint8_t KEY_A         = 0x1E;
    constexpr uint8_t KEY_S         = 0x1F;
    constexpr uint8_t KEY_D         = 0x20;
    constexpr uint8_t KEY_F         = 0x21;
    constexpr uint8_t KEY_G         = 0x22;
    constexpr uint8_t KEY_H         = 0x23;
    constexpr uint8_t KEY_J         = 0x24;
    constexpr uint8_t KEY_K         = 0x25;
    constexpr uint8_t KEY_L         = 0x26;
    constexpr uint8_t KEY_ENTER     = 0x1C;
    /* Row3 */
    constexpr uint8_t KEY_CTRL      = 0x1D;
    constexpr uint8_t KEY_ALT       = 0x38;
    constexpr uint8_t KEY_Z         = 0x2C;
    constexpr uint8_t KEY_X         = 0x2D;
    constexpr uint8_t KEY_C         = 0x2E;
    constexpr uint8_t KEY_V         = 0x2F;
    constexpr uint8_t KEY_B         = 0x30;
    constexpr uint8_t KEY_N         = 0x31;
    constexpr uint8_t KEY_M         = 0x32;
    constexpr uint8_t KEY_SPACE     = 0x39;
}

/**
 * @brief Translates SDL keyboard events into LVGL-compatible key codes,
 *        optionally applying Sym or Fn modifier layers.
 *
 * Usage:
 *   1. Create a single KeyboardMapper instance.
 *   2. For each SDL key event, call handleEvent().
 *   3. If it returns true, read the resulting LVGL key from lastLvglKey()
 *      and pressed state from lastPressed().
 */
class KeyboardMapper {
public:
    KeyboardMapper() = default;

    /**
     * Process an SDL key event.
     * @return true if a key should be forwarded to LVGL, false if consumed
     *         internally (e.g. modifier toggle).
     */
    bool handleEvent(const SDL_KeyboardEvent &ev);

    /** LVGL key code produced by the last successful handleEvent(). */
    uint32_t lastLvglKey() const { return lv_key_; }

    /** true = key pressed, false = key released. */
    bool lastPressed() const { return pressed_; }

    /* Modifier queries */
    bool isSymActive()   const { return sym_active_; }
    bool isFnActive()    const { return fn_active_; }
    bool isShiftActive() const { return shift_held_; }
    bool isCtrlActive()  const { return ctrl_held_; }
    bool isAltActive()   const { return alt_held_; }

private:
    /* Modifier state */
    bool sym_active_  = false;  /* toggled by F1 */
    bool fn_active_   = false;  /* toggled by F2 */
    bool shift_held_  = false;
    bool ctrl_held_   = false;
    bool alt_held_    = false;

    /* Last produced key */
    uint32_t lv_key_ = 0;
    bool pressed_     = false;

    /** Map an SDL keycode to a base character or LVGL key. */
    uint32_t mapBase(SDL_Keycode kc) const;

    /** Apply the Sym layer to a base character. */
    uint32_t applySym(uint32_t base) const;

    /** Apply the Fn layer -- returns an LVGL navigation key or 0. */
    uint32_t applyFn(SDL_Keycode kc) const;
};
