#include "ssh_connection_model.hpp"
#include "../keyboard_text_input.hpp"
#include "integer_parse_policy.hpp"

#include <algorithm>
#include <cctype>

SshConnectionModel::SshConnectionModel()
    : values_{"192.168.1.1", "22", "pi"}
{
}

const char *SshConnectionModel::label(size_t index) const
{
    static constexpr const char *labels[FIELD_COUNT] = {"Host", "Port", "User"};
    return index < FIELD_COUNT ? labels[index] : "";
}

const std::string &SshConnectionModel::value(size_t index) const
{
    static const std::string empty;
    return index < FIELD_COUNT ? values_[index] : empty;
}

bool SshConnectionModel::select_previous()
{
    if (active_index_ == 0) return false;
    --active_index_;
    return true;
}

bool SshConnectionModel::select_next()
{
    if (active_index_ + 1 >= FIELD_COUNT) return false;
    ++active_index_;
    return true;
}

bool SshConnectionModel::append(const std::string &text)
{
    return launcher_ui::text_input::append_limited(values_[active_index_], text,
                                                    MAX_FIELD_BYTES);
}

bool SshConnectionModel::erase_last()
{
    return launcher_ui::text_input::erase_last_codepoint(values_[active_index_]);
}

bool SshConnectionModel::valid() const
{
    const std::string &host = values_[static_cast<size_t>(Field::HOST)];
    const std::string &port = values_[static_cast<size_t>(Field::PORT)];
    const std::string &user = values_[static_cast<size_t>(Field::USER)];
    auto invalid_identity = [](const std::string &value) {
        return (!value.empty() && value.front() == '-') ||
               std::any_of(value.begin(), value.end(), [](unsigned char character) {
                   return std::isspace(character) != 0;
               });
    };
    if (host.empty() || invalid_identity(host) || invalid_identity(user)) return false;
    if (port.empty()) return true;
    unsigned int number = 0;
    return launcher_ui::integer_parse::complete(port, 1U, 65535U, number);
}

std::list<std::string> SshConnectionModel::arguments() const
{
    if (!valid()) return {};
    const std::string &host = values_[static_cast<size_t>(Field::HOST)];
    const std::string &port = values_[static_cast<size_t>(Field::PORT)];
    const std::string &user = values_[static_cast<size_t>(Field::USER)];
    std::list<std::string> arguments = {"-o", "StrictHostKeyChecking=no"};
    if (!port.empty() && port != "22") {
        arguments.push_back("-p");
        arguments.push_back(port);
    }
    arguments.push_back(user.empty() ? host : user + "@" + host);
    return arguments;
}
