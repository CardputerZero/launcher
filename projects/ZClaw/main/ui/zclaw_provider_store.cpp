#include "zclaw_provider_store.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace zclaw {
namespace {

bool write_all(int fd, const std::string &data)
{
    size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = ::write(fd, data.data() + offset, data.size() - offset);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (written == 0) {
            errno = EIO;
            return false;
        }
        offset += static_cast<size_t>(written);
    }
    return true;
}

void set_error(std::string *error, const char *operation)
{
    if (error)
        *error = std::string(operation) + ": " + std::strerror(errno);
}

}  // namespace

std::string encode_config_field(const std::string &value)
{
    std::string out;
    for (char ch : value) {
        if (ch == '\\')
            out += "\\\\";
        else if (ch == '\t')
            out += "\\t";
        else if (ch == '\n')
            out += "\\n";
        else
            out += ch;
    }
    return out;
}

std::string decode_config_field(const std::string &value)
{
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            const char next = value[++i];
            if (next == 't')
                out += '\t';
            else if (next == 'n')
                out += '\n';
            else
                out += next;
        } else {
            out += value[i];
        }
    }
    return out;
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

bool load_provider_configs(const std::string &path, std::vector<ProviderConfig> *providers)
{
    if (!providers)
        return false;
    providers->clear();
    std::ifstream file(path);
    if (!file)
        return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        const std::vector<std::string> fields = split_config_line(line);
        if (fields.size() < 5)
            continue;
        providers->push_back({decode_config_field(fields[0]), decode_config_field(fields[1]),
                              decode_config_field(fields[2]), decode_config_field(fields[3]),
                              decode_config_field(fields[4])});
    }
    return !file.bad();
}

bool save_provider_configs(const std::string &path, const std::vector<ProviderConfig> &providers,
                           std::string *error)
{
    if (error)
        error->clear();

    std::string contents;
    for (const ProviderConfig &provider : providers) {
        contents += encode_config_field(provider.alias) + '\t' +
                    encode_config_field(provider.family) + '\t' +
                    encode_config_field(provider.model) + '\t' +
                    encode_config_field(provider.uri) + '\t' +
                    encode_config_field(provider.api_key) + '\n';
    }

    std::string temp_path = path + ".tmp.XXXXXX";
    std::vector<char> temp_name(temp_path.begin(), temp_path.end());
    temp_name.push_back('\0');
    const int fd = ::mkstemp(temp_name.data());
    if (fd < 0) {
        set_error(error, "Could not create provider settings file");
        return false;
    }

    bool ok = write_all(fd, contents);
    int saved_errno = ok ? 0 : errno;
    if (ok && ::fsync(fd) != 0) {
        ok = false;
        saved_errno = errno;
    }
    if (::close(fd) != 0 && ok) {
        ok = false;
        saved_errno = errno;
    }
    if (!ok) {
        errno = saved_errno;
        set_error(error, "Could not write provider settings");
        ::unlink(temp_name.data());
        return false;
    }

    if (::rename(temp_name.data(), path.c_str()) != 0) {
        set_error(error, "Could not replace provider settings");
        ::unlink(temp_name.data());
        return false;
    }
    return true;
}

}  // namespace zclaw
