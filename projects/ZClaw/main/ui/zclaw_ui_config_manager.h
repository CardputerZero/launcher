#pragma once

#include "zclaw_config_store_status.h"
#include "zclaw_types.h"

#include <string>

namespace zclaw {

class UiConfigManager {
public:
    explicit UiConfigManager(std::string storage_path);

    ConfigStoreLoadStatus load(std::string *error = nullptr);
    const UiConfig &config() const;
    bool update(const UiConfig &config, std::string *error = nullptr);
    bool clear_token(std::string *error = nullptr);

private:
    std::string storage_path_;
    UiConfig config_;
};

}  // namespace zclaw
