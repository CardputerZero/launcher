#pragma once

#include <string>
#include <vector>

namespace zclaw {

std::string encode_config_field(const std::string &value);
std::string decode_config_field(const std::string &value);
bool is_valid_config_field_encoding(const std::string &value);
std::vector<std::string> split_config_line(const std::string &line);

}  // namespace zclaw
