#pragma once

#include <string>

namespace cp0_alsa_parser {

std::string encode_cards(const std::string &raw_cards);
std::string encode_controls(const std::string &raw_controls);

} // namespace cp0_alsa_parser
