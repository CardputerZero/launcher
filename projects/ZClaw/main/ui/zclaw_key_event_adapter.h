#pragma once

#include "zclaw_key_router.h"

#include <cstdint>

namespace zclaw {

KeyEvent adapt_key_event(std::uint32_t key_code, int key_state,
                         std::uint32_t modifiers, const char *utf8);

}  // namespace zclaw
