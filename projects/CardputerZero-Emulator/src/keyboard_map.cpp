/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "keyboard_map.h"
#include "lvgl/lvgl.h"

/* --------------------------------------------------------------------------
 * KeyboardMapper implementation
 * -------------------------------------------------------------------------- */

bool KeyboardMapper::handleEvent(const SDL_KeyboardEvent &ev)
{
    const bool down = (ev.type == SDL_KEYDOWN);
    const SDL_Keycode kc = ev.keysym.sym;

    /* ---- Modifier toggles (F1 = Sym, F2 = Fn) ---- */
    if (kc == SDLK_F1) {
        if (down) sym_active_ = !sym_active_;
        return false;  /* consumed -- don't forward */
    }
    if (kc == SDLK_F2) {
        if (down) fn_active_ = !fn_active_;
        return false;
    }

    /* ---- Track physical modifier keys ---- */
    if (kc == SDLK_LSHIFT || kc == SDLK_RSHIFT) { shift_held_ = down; return false; }
    if (kc == SDLK_LCTRL  || kc == SDLK_RCTRL)  { ctrl_held_  = down; return false; }
    if (kc == SDLK_LALT   || kc == SDLK_RALT)    { alt_held_   = down; return false; }

    /* ---- Fn layer takes priority ---- */
    if (fn_active_) {
        uint32_t nav = applyFn(kc);
        if (nav != 0) {
            lv_key_ = nav;
            pressed_ = down;
            return true;
        }
        /* If the Fn layer doesn't remap this key, fall through to normal */
    }

    /* ---- Base mapping ---- */
    uint32_t base = mapBase(kc);
    if (base == 0) return false;  /* unmapped key */

    /* ---- Sym layer ---- */
    if (sym_active_) {
        uint32_t sym = applySym(base);
        if (sym != 0) base = sym;
    }

    /* ---- Shift for letters ---- */
    if (shift_held_ && base >= 'a' && base <= 'z') {
        base = base - 'a' + 'A';
    }

    lv_key_ = base;
    pressed_ = down;
    return true;
}

/* --------------------------------------------------------------------------
 * Base mapping: SDL_Keycode -> character or LVGL key
 * -------------------------------------------------------------------------- */
uint32_t KeyboardMapper::mapBase(SDL_Keycode kc) const
{
    switch (kc) {
        /* Row4 */
        case SDLK_ESCAPE:      return LV_KEY_ESC;
        case SDLK_1:           return '1';
        case SDLK_TAB:         return LV_KEY_NEXT;  /* Tab -> focus next */

        /* Row0 */
        case SDLK_2:           return '2';
        case SDLK_3:           return '3';
        case SDLK_4:           return '4';
        case SDLK_5:           return '5';
        case SDLK_6:           return '6';
        case SDLK_7:           return '7';
        case SDLK_8:           return '8';
        case SDLK_9:           return '9';
        case SDLK_0:           return '0';
        case SDLK_BACKSPACE:   return LV_KEY_BACKSPACE;

        /* Row1 */
        case SDLK_q:           return 'q';
        case SDLK_w:           return 'w';
        case SDLK_e:           return 'e';
        case SDLK_r:           return 'r';
        case SDLK_t:           return 't';
        case SDLK_y:           return 'y';
        case SDLK_u:           return 'u';
        case SDLK_i:           return 'i';
        case SDLK_o:           return 'o';
        case SDLK_p:           return 'p';

        /* Row2 */
        case SDLK_a:           return 'a';
        case SDLK_s:           return 's';
        case SDLK_d:           return 'd';
        case SDLK_f:           return 'f';
        case SDLK_g:           return 'g';
        case SDLK_h:           return 'h';
        case SDLK_j:           return 'j';
        case SDLK_k:           return 'k';
        case SDLK_l:           return 'l';
        case SDLK_RETURN:      return LV_KEY_ENTER;

        /* Row3 */
        case SDLK_z:           return 'z';
        case SDLK_x:           return 'x';
        case SDLK_c:           return 'c';
        case SDLK_v:           return 'v';
        case SDLK_b:           return 'b';
        case SDLK_n:           return 'n';
        case SDLK_m:           return 'm';
        case SDLK_SPACE:       return ' ';

        /* Pass through arrow keys directly (useful even without Fn) */
        case SDLK_UP:          return LV_KEY_UP;
        case SDLK_DOWN:        return LV_KEY_DOWN;
        case SDLK_LEFT:        return LV_KEY_LEFT;
        case SDLK_RIGHT:       return LV_KEY_RIGHT;
        case SDLK_HOME:        return LV_KEY_HOME;
        case SDLK_END:         return LV_KEY_END;
        case SDLK_DELETE:      return LV_KEY_DEL;

        default:               return 0;  /* not mapped */
    }
}

/* --------------------------------------------------------------------------
 * Sym layer: digits -> symbols, top-row letters -> special chars
 *
 * Row0 digits:  1->! 2->@ 3-># 4->$ 5->% 6->^ 7->& 8->* 9->( 0->)
 * Row1 letters: Q->~ W->` E->+ R->- T->/ Y->\ U->{ I->} O->[ P->]
 * -------------------------------------------------------------------------- */
uint32_t KeyboardMapper::applySym(uint32_t base) const
{
    switch (base) {
        /* digits -> shifted symbols */
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';

        /* Row1 Q-P -> special characters */
        case 'q': case 'Q': return '~';
        case 'w': case 'W': return '`';
        case 'e': case 'E': return '+';
        case 'r': case 'R': return '-';
        case 't': case 'T': return '/';
        case 'y': case 'Y': return '\\';
        case 'u': case 'U': return '{';
        case 'i': case 'I': return '}';
        case 'o': case 'O': return '[';
        case 'p': case 'P': return ']';

        /* Additional useful symbols on Row2 */
        case 'a': case 'A': return ';';
        case 's': case 'S': return ':';
        case 'd': case 'D': return '\'';
        case 'f': case 'F': return '"';
        case 'g': case 'G': return ',';
        case 'h': case 'H': return '.';
        case 'j': case 'J': return '?';
        case 'k': case 'K': return '<';
        case 'l': case 'L': return '>';

        /* Row3 extras */
        case 'z': case 'Z': return '_';
        case 'x': case 'X': return '=';
        case 'c': case 'C': return '|';
        case 'v': case 'V': return '@';
        case 'b': case 'B': return '#';
        case 'n': case 'N': return '&';
        case 'm': case 'M': return '*';

        default: return 0;  /* no sym mapping -- use base */
    }
}

/* --------------------------------------------------------------------------
 * Fn layer: navigation keys
 *
 * Row2: F->Up
 * Row3: Z->Left, X->Down, C->Right
 * Row2: K->PgUp, L->PgDn
 * Row3: N->Home, M->Insert (mapped to LV_KEY_HOME and special char)
 * -------------------------------------------------------------------------- */
uint32_t KeyboardMapper::applyFn(SDL_Keycode kc) const
{
    switch (kc) {
        /* Arrow keys */
        case SDLK_f:    return LV_KEY_UP;
        case SDLK_z:    return LV_KEY_LEFT;
        case SDLK_x:    return LV_KEY_DOWN;
        case SDLK_c:    return LV_KEY_RIGHT;

        /* Page navigation */
        case SDLK_k:    return LV_KEY_PREV;   /* PgUp -> previous in LVGL */
        case SDLK_l:    return LV_KEY_NEXT;   /* PgDn -> next in LVGL */

        /* Home / End */
        case SDLK_n:    return LV_KEY_HOME;
        case SDLK_m:    return LV_KEY_END;    /* Insert -> End in LVGL context */

        default:         return 0;  /* not an Fn-layer key */
    }
}
