/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace settings_software {

using Capture = std::function<int(const std::vector<std::string> &, std::string &)>;

struct Result {
    int code = -1;
    std::string stage;
};

inline std::string trim(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])))
        ++first;
    return value.substr(first);
}

/**
 * One application whose Debian package is published as a GitHub release asset.
 *
 * The asset name is expected to look like
 * `<asset_prefix><version><asset_suffix>`, for example
 * `m5cardputerzero-recorder_0.1.8_m5stack1_arm64.deb`.  The prefix and suffix
 * are supplied by the caller so this module stays independent from the
 * packaging script.
 */

struct AppSpec {
    std::string package;
    std::string repo;
    std::string asset_prefix;
    std::string asset_suffix;
};

struct AppUpdate {
    std::string package;
    std::string installed;
    std::string latest;
};

struct LauncherInfo {
    bool known = false;
    bool available = false;
    bool failed = false;
    std::string installed;
    std::string version;
    std::string commit;
};

struct LauncherUpdateInfo {
    std::string version;
    std::string package_version;
    std::string commit;
};

inline bool safe_metadata_value(const std::string &value)
{
    if (value.empty() || value.size() > 128) return false;
    for (const unsigned char character : value) {
        if (character < 0x21 || character > 0x7e || character == '|' ||
            character == '=')
            return false;
    }
    return true;
}

inline bool launcher_update_info(const std::string &text, LauncherUpdateInfo &info)
{
    LauncherUpdateInfo parsed;
    bool format_seen = false;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) return false;
        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);
        if (key == "format") {
            if (format_seen || value != "1") return false;
            format_seen = true;
        } else if (key == "version") {
            if (!parsed.version.empty()) return false;
            parsed.version = value;
        } else if (key == "package_version") {
            if (!parsed.package_version.empty()) return false;
            parsed.package_version = value;
        } else if (key == "commit") {
            if (!parsed.commit.empty()) return false;
            parsed.commit = value;
        }
    }
    if (!format_seen || !safe_metadata_value(parsed.version) ||
        !safe_metadata_value(parsed.commit))
        return false;
    if (parsed.package_version.empty()) parsed.package_version = parsed.version;
    if (!safe_metadata_value(parsed.package_version)) return false;
    for (const unsigned char character : parsed.commit)
        if (!std::isxdigit(character)) return false;
    if (parsed.commit.size() < 7 || parsed.commit.size() > 64) return false;
    info = std::move(parsed);
    return true;
}

inline void check_launcher(const Capture &capture,
                           const std::string &release_url,
                           LauncherInfo &info)
{
    std::string metadata;
    if (capture({"curl", "-fsSL", "--max-time", "20", "--connect-timeout", "5",
                 release_url + "/applaunch_arm64.deb.update-info"},
                metadata) != 0) {
        info.failed = true;
        return;
    }
    LauncherUpdateInfo parsed;
    if (!launcher_update_info(metadata, parsed)) {
        info.failed = true;
        return;
    }
    std::string installed;
    if (capture({"dpkg-query", "-W", "-f=${Version}", "applaunch"}, installed) != 0) {
        info.failed = true;
        return;
    }
    installed = trim(std::move(installed));
    if (installed.empty()) {
        info.failed = true;
        return;
    }
    info.installed = installed;
    std::string ignored;
    const int comparison = capture(
        {"dpkg", "--compare-versions", parsed.package_version, "gt", installed},
        ignored);
    if (comparison != 0 && comparison != 1) {
        info.failed = true;
        return;
    }
    info.known = true;
    info.available = comparison == 0;
    info.version = parsed.version;
    info.commit = parsed.commit;
}


struct Outcome {
    int checked = 0;
    int failed = 0;
    int skipped = 0;
    std::vector<AppUpdate> updates;
    LauncherInfo launcher;
};

inline bool safe_field(const std::string &value)
{
    if (value.empty() || value.size() > 160) return false;
    for (const unsigned char character : value) {
        if (std::isalnum(character) || character == '.' || character == '_' ||
            character == '+' || character == '~' || character == '-')
            continue;
        return false;
    }
    return true;
}

inline bool parse_app_spec(const std::string &text, AppSpec &spec)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (;;) {
        const std::size_t separator = text.find('|', start);
        fields.push_back(text.substr(
            start, separator == std::string::npos ? std::string::npos : separator - start));
        if (separator == std::string::npos) break;
        start = separator + 1;
        if (fields.size() > 4) return false;
    }
    if (fields.size() != 4) return false;
    if (!safe_field(fields[0]) || !safe_field(fields[1]) ||
        !safe_field(fields[2]) || !safe_field(fields[3]))
        return false;
    if (fields[3].size() < 4 ||
        fields[3].compare(fields[3].size() - 4, 4, ".deb") != 0)
        return false;

    spec.package = std::move(fields[0]);
    spec.repo = std::move(fields[1]);
    spec.asset_prefix = std::move(fields[2]);
    spec.asset_suffix = std::move(fields[3]);
    return true;
}

/**
 * Extract the version from the first asset in a GitHub release JSON payload
 * whose name starts with `asset_prefix` and ends with `asset_suffix`.
 *
 * Only the JSON string value is inspected, so callers do not need a JSON
 * dependency for this narrow, machine-generated payload.
 */
inline bool parse_asset_version(const std::string &json, const AppSpec &spec,
                                std::string &version)
{
    std::size_t position = 0;
    while ((position = json.find("\"name\"", position)) != std::string::npos) {
        const std::size_t colon = json.find(':', position + 6);
        if (colon == std::string::npos) break;
        const std::size_t quote = json.find('"', colon + 1);
        if (quote == std::string::npos) break;
        const std::size_t end_quote = json.find('"', quote + 1);
        if (end_quote == std::string::npos) break;

        const std::string name = json.substr(quote + 1, end_quote - quote - 1);
        if (name.size() > spec.asset_prefix.size() + spec.asset_suffix.size() &&
            name.rfind(spec.asset_prefix, 0) == 0 &&
            name.compare(name.size() - spec.asset_suffix.size(),
                         spec.asset_suffix.size(), spec.asset_suffix) == 0) {
            version = name.substr(
                spec.asset_prefix.size(),
                name.size() - spec.asset_prefix.size() - spec.asset_suffix.size());
            return !version.empty();
        }
        position = end_quote + 1;
    }
    return false;
}

inline bool parse_asset_version_from_text(const std::string &text,
                                          const AppSpec &spec,
                                          std::string &version)
{
    std::size_t position = 0;
    while ((position = text.find(spec.asset_prefix, position)) !=
           std::string::npos) {
        const std::size_t version_begin = position + spec.asset_prefix.size();
        const std::size_t suffix = text.find(spec.asset_suffix, version_begin);
        if (suffix != std::string::npos && suffix >= version_begin &&
            suffix - version_begin <= 128) {
            version = text.substr(version_begin, suffix - version_begin);
            if (!version.empty()) return true;
        }
        position += 1;
    }
    return false;
}

inline bool safe_version(const std::string &version)
{
    if (version.empty() || version.size() > 128) return false;
    for (const unsigned char character : version) {
        if (character == '|' || character == ';' || character == '=' ||
            character == '>' || character == '\n' || character == '\r')
            return false;
    }
    return true;
}

struct ReleaseCacheEntry {
    std::string version;
    std::chrono::steady_clock::time_point fetched;
};

inline std::mutex &release_cache_mutex()
{
    static std::mutex mutex;
    return mutex;
}

inline std::map<std::string, ReleaseCacheEntry> &release_cache()
{
    static std::map<std::string, ReleaseCacheEntry> cache;
    return cache;
}

inline bool cached_latest_version(const std::string &repo, std::string &version)
{
    std::lock_guard<std::mutex> lock(release_cache_mutex());
    const auto found = release_cache().find(repo);
    if (found == release_cache().end()) return false;
    if (std::chrono::steady_clock::now() - found->second.fetched >
        std::chrono::minutes(5))
        return false;
    version = found->second.version;
    return !version.empty();
}

inline void store_latest_version(const std::string &repo,
                                 const std::string &version)
{
    std::lock_guard<std::mutex> lock(release_cache_mutex());
    release_cache()[repo] = {version, std::chrono::steady_clock::now()};
}

inline void clear_release_cache()
{
    std::lock_guard<std::mutex> lock(release_cache_mutex());
    release_cache().clear();
}

inline bool installed_version(const Capture &capture, const std::string &package,
                              std::string &version)
{
    if (!capture || package.empty()) return false;
    std::string output;
    if (capture({"dpkg-query", "-W", "-f=${Version}", package}, output) != 0)
        return false;
    version = trim(std::move(output));
    return safe_version(version);
}

inline bool latest_version_from_html(const Capture &capture,
                                     const AppSpec &spec,
                                     std::string &version)
{
    std::string effective_url;
    if (capture({"curl", "-fsSL", "--max-time", "15", "--connect-timeout", "5",
                 "-o", "/dev/null", "-w", "%{url_effective}",
                 "https://github.com/CardputerZero/" + spec.repo +
                     "/releases/latest"},
                effective_url) != 0)
        return false;
    const std::size_t tag_position = effective_url.rfind("/tag/");
    if (tag_position == std::string::npos) return false;
    std::string tag = trim(effective_url.substr(tag_position + 5));
    if (tag.empty()) return false;

    std::string html;
    if (capture({"curl", "-fsSL", "--max-time", "15", "--connect-timeout", "5",
                 "https://github.com/CardputerZero/" + spec.repo +
                     "/releases/expanded_assets/" + tag},
                html) != 0)
        return false;
    return parse_asset_version_from_text(html, spec, version) &&
           safe_version(version);
}

inline bool latest_version(const Capture &capture, const AppSpec &spec,
                           std::string &version)
{
    if (!capture) return false;
    if (cached_latest_version(spec.repo, version)) return true;
    std::string json;
    const std::vector<std::string> argv = {
        "curl", "-fsSL", "--max-time", "15", "--connect-timeout", "5",
        "-H", "Accept: application/vnd.github+json",
        "-H", "User-Agent: APPLaunch-Software-Check",
        "https://api.github.com/repos/CardputerZero/" + spec.repo +
            "/releases/latest"};
    if (capture(argv, json) == 0 &&
        parse_asset_version(json, spec, version) && safe_version(version)) {
        store_latest_version(spec.repo, version);
        return true;
    }
    if (!latest_version_from_html(capture, spec, version)) return false;
    store_latest_version(spec.repo, version);
    return true;
}

/** Returns 0 when `latest` is newer, 1 when it is not, other values on error. */
inline int compare_versions(const Capture &capture, const std::string &latest,
                            const std::string &installed)
{
    if (!capture) return -1;
    std::string ignored;
    return capture({"dpkg", "--compare-versions", latest, "gt", installed},
                   ignored);
}

inline std::string encode_outcome(const Outcome &outcome)
{
    std::ostringstream encoded;
    encoded << "software|checked=" << outcome.checked
            << "|failed=" << outcome.failed
            << "|skipped=" << outcome.skipped
            << "|updates=";
    bool first = true;
    for (const AppUpdate &update : outcome.updates) {
        if (!safe_field(update.package) || !safe_version(update.installed) ||
            !safe_version(update.latest))
            continue;
        if (!first) encoded << ';';
        first = false;
        encoded << update.package << '=' << update.installed << '>'
                << update.latest;
    }
    encoded << "|launcher_known=" << (outcome.launcher.known ? 1 : 0)
            << "|launcher_available=" << (outcome.launcher.available ? 1 : 0)
            << "|launcher_installed=" << outcome.launcher.installed
            << "|launcher_version=" << outcome.launcher.version
            << "|launcher_commit=" << outcome.launcher.commit
            << "|launcher_failed=" << (outcome.launcher.failed ? 1 : 0);
    return encoded.str();
}

/**
 * Check every supplied application plus APPLaunch itself.
 *
 * The operation is cooperative: `cancel` is observed between commands, and
 * `progress` receives a 0..100 value.  A failure for one package does not
 * discard the results of the other packages.
 */
inline Result check_software(const std::vector<AppSpec> &specs,
                             const Capture &capture,
                             const std::string &launcher_release_url,
                             const std::atomic<bool> &cancel,
                             const std::function<void(int)> &progress = {})
{
    Outcome outcome;
    outcome.checked = static_cast<int>(specs.size());
    const int total = std::max(1, outcome.checked);

    for (std::size_t index = 0; index < specs.size(); ++index) {
        if (cancel.load()) return {-ECANCELED, "cancelled"};
        if (progress) progress(static_cast<int>(index * 80 / total));

        std::string installed;
        if (!installed_version(capture, specs[index].package, installed)) {
            ++outcome.skipped;
            continue;
        }
        std::string latest;
        if (!latest_version(capture, specs[index], latest)) {
            ++outcome.failed;
            continue;
        }
        const int comparison = compare_versions(capture, latest, installed);
        if (comparison == 0) {
            outcome.updates.push_back({specs[index].package, installed, latest});
        } else if (comparison != 1) {
            ++outcome.failed;
        }
    }

    if (progress) progress(85);
    if (cancel.load()) return {-ECANCELED, "cancelled"};
    check_launcher(capture, launcher_release_url, outcome.launcher);
    if (cancel.load()) return {-ECANCELED, "cancelled"};

    if (progress) progress(100);
    return {0, encode_outcome(outcome)};
}

} // namespace settings_software
