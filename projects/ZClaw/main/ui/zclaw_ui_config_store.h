#pragma once

#include "zclaw_config_store_status.h"
#include "zclaw_types.h"

#include <string>

namespace zclaw {

bool load_ui_config(const std::string &path, UiConfig *config);
ConfigStoreLoadStatus load_ui_config_with_status(
    const std::string &path, UiConfig *config, std::string *error = nullptr);
bool save_ui_config(const std::string &path, const UiConfig &config,
                    std::string *error = nullptr);

}  // namespace zclaw
