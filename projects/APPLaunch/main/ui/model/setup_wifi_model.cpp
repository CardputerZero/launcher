#include "setup_wifi_model.hpp"

#include <utility>

namespace {

bool is_continuation(unsigned char value)
{
    return (value & 0xc0) == 0x80;
}

bool is_valid_utf8_text(const std::string &text)
{
    std::size_t index = 0;
    while (index < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        std::size_t length = 0;
        unsigned int codepoint = 0;
        if (lead < 0x80) {
            length = 1;
            codepoint = lead;
        } else if ((lead & 0xe0) == 0xc0) {
            length = 2;
            codepoint = lead & 0x1f;
        } else if ((lead & 0xf0) == 0xe0) {
            length = 3;
            codepoint = lead & 0x0f;
        } else if ((lead & 0xf8) == 0xf0) {
            length = 4;
            codepoint = lead & 0x07;
        } else {
            return false;
        }
        if (index + length > text.size()) return false;
        for (std::size_t offset = 1; offset < length; ++offset) {
            const unsigned char value = static_cast<unsigned char>(text[index + offset]);
            if (!is_continuation(value)) return false;
            codepoint = (codepoint << 6) | (value & 0x3f);
        }
        if ((length == 2 && codepoint < 0x80) ||
            (length == 3 && codepoint < 0x800) ||
            (length == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff ||
            codepoint < 0x20 || codepoint == 0x7f)
            return false;
        index += length;
    }
    return true;
}

} // namespace

SetupWifiPasswordModel::~SetupWifiPasswordModel()
{
    secure_clear_password();
}

void SetupWifiPasswordModel::begin(std::string ssid)
{
    ssid_ = std::move(ssid);
    secure_clear_password();
}

void SetupWifiPasswordModel::reset()
{
    ssid_.clear();
    secure_clear_password();
}

bool SetupWifiPasswordModel::append(const std::string &text)
{
    if (text.empty() || password_.size() + text.size() > MAX_PASSWORD_BYTES)
        return false;
    if (!is_valid_utf8_text(text)) return false;
    password_ += text;
    return true;
}

bool SetupWifiPasswordModel::erase_last()
{
    if (password_.empty()) return false;
    std::size_t start = password_.size() - 1;
    while (start > 0 && is_continuation(static_cast<unsigned char>(password_[start])))
        --start;
    volatile char *bytes = password_.data();
    for (std::size_t index = start; index < password_.size(); ++index) bytes[index] = '\0';
    password_.erase(start);
    return true;
}

void SetupWifiPasswordModel::clear_password()
{
    secure_clear_password();
}

void SetupWifiPasswordModel::secure_clear_password()
{
    volatile char *bytes = password_.data();
    for (std::size_t index = 0; index < password_.size(); ++index) bytes[index] = '\0';
    password_.clear();
}

std::string SetupWifiPasswordModel::masked_display() const
{
    std::size_t codepoints = 0;
    for (unsigned char value : password_)
        if (!is_continuation(value)) ++codepoints;
    return std::string(codepoints, '*') + '_';
}

SetupWifiFeedbackModel::Token SetupWifiFeedbackModel::begin()
{
    if (pending_) return 0;
    pending_ = true;
    if (++generation_ == 0) ++generation_;
    return generation_;
}

bool SetupWifiFeedbackModel::complete(Token token)
{
    if (!pending_ || token == 0 || token != generation_) return false;
    pending_ = false;
    return true;
}

bool SetupWifiFeedbackModel::cancel(Token token)
{
    if (!pending_ || token == 0 || token != generation_) return false;
    pending_ = false;
    return true;
}
