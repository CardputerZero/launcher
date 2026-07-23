#include "zclaw_config_codec.h"

namespace zclaw {

std::string encode_config_field(const std::string &value)
{
    std::string encoded;
    for (char ch : value) {
        if (ch == '\\')
            encoded += "\\\\";
        else if (ch == '\t')
            encoded += "\\t";
        else if (ch == '\n')
            encoded += "\\n";
        else
            encoded += ch;
    }
    return encoded;
}

std::string decode_config_field(const std::string &value)
{
    std::string decoded;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\\' && index + 1 < value.size()) {
            const char next = value[++index];
            if (next == 't')
                decoded += '\t';
            else if (next == 'n')
                decoded += '\n';
            else
                decoded += next;
        } else {
            decoded += value[index];
        }
    }
    return decoded;
}

bool is_valid_config_field_encoding(const std::string &value)
{
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\')
            continue;
        if (++index >= value.size())
            return false;
        const char escaped = value[index];
        if (escaped != '\\' && escaped != 't' && escaped != 'n')
            return false;
    }
    return true;
}

std::vector<std::string> split_config_line(const std::string &line)
{
    std::vector<std::string> fields;
    std::string current;
    for (char ch : line) {
        if (ch == '\t') {
            fields.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    fields.push_back(current);
    return fields;
}

}  // namespace zclaw
