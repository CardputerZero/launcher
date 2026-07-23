#pragma once

#include "zclaw_types.h"

#include <string>
#include <vector>

namespace zclaw {

std::vector<ProviderConfig> parse_provider_config_document(
    const std::string &document);
bool parse_provider_config_document_checked(
    const std::string &document, std::vector<ProviderConfig> *providers);
std::string make_provider_config_document(
    const std::vector<ProviderConfig> &providers);
UiConfig parse_ui_config_document(const std::string &document);
bool parse_ui_config_document_checked(const std::string &document,
                                      UiConfig *config);
std::string make_ui_config_document(const UiConfig &config);

}  // namespace zclaw
