#pragma once

#include "zclaw_types.h"

#include <string>

namespace zclaw {

ProviderConfig normalize_setup_provider(ProviderConfig provider);
std::string extract_pairing_code(const std::string &text);

}  // namespace zclaw
