/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Minimal JSON <-> flat "dotted key" store used by the unified device config
// (~/.config/cardputerzero/config.json). The launcher's config system keeps a
// flat key=value model internally; nested JSON objects map to dotted keys, e.g.
//   camera.resolution.width -> { "camera": { "resolution": { "width": .. }}}
// This keeps the shared contract with other apps (the camera app reads
// camera.resolution.{width,height}) while the launcher still uses simple keys.
//
// Only objects, strings and numbers are emitted; arrays/true/false/null are
// tolerated on read but never written (the launcher is the sole writer).

#include <charconv>
#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cp0cfg {

inline bool looks_numeric(const std::string &v)
{
    if (v.empty())
        return false;
    size_t i = 0;
    if (v[0] == '-') {
        if (v.size() == 1)
            return false;
        i = 1;
    }
    for (; i < v.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(v[i])))
            return false;
    std::int64_t n = 0;
    const auto result = std::from_chars(v.data(), v.data() + v.size(), n);
    if (result.ec != std::errc{} || result.ptr != v.data() + v.size())
        return false;
    // Only treat as a JSON number if it round-trips, so values like "007" keep
    // their string form and get_str() returns them unchanged.
    return std::to_string(n) == v;
}

struct JsonNode {
    std::map<std::string, JsonNode> children;
    std::string value;
    bool numeric = false;
};

inline void json_escape(const std::string &s, std::string &out)
{
    static constexpr char hex[] = "0123456789abcdef";
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += "\\u00";
                out += hex[(static_cast<unsigned char>(c) >> 4) & 0x0f];
                out += hex[static_cast<unsigned char>(c) & 0x0f];
            } else {
                out += c;
            }
            break;
        }
    }
}

inline void json_emit(const JsonNode &node, std::string &out, int indent)
{
    if (node.children.empty()) {
        if (node.numeric) {
            out += node.value;
        } else {
            out += '"';
            json_escape(node.value, out);
            out += '"';
        }
        return;
    }
    const std::string pad(static_cast<size_t>(indent) * 2, ' ');
    const std::string pad2(static_cast<size_t>(indent + 1) * 2, ' ');
    out += "{\n";
    size_t i = 0;
    for (const auto &kv : node.children) {
        out += pad2;
        out += '"';
        json_escape(kv.first, out);
        out += "\": ";
        json_emit(kv.second, out, indent + 1);
        if (++i < node.children.size())
            out += ',';
        out += '\n';
    }
    out += pad;
    out += '}';
}

inline std::string to_json(const std::vector<std::pair<std::string, std::string>> &kv)
{
    JsonNode root;
    for (const auto &e : kv) {
        const std::string &key = e.first;
        if (key.empty())
            continue;
        JsonNode *cur = &root;
        size_t start = 0;
        while (true) {
            size_t dot = key.find('.', start);
            std::string seg = (dot == std::string::npos) ? key.substr(start)
                                                          : key.substr(start, dot - start);
            if (seg.empty())
                break;
            cur = &cur->children[seg];
            if (dot == std::string::npos) {
                cur->value = e.second;
                cur->numeric = looks_numeric(e.second);
                break;
            }
            start = dot + 1;
        }
    }
    if (root.children.empty())
        return "{}\n";
    std::string out;
    json_emit(root, out, 0);
    out += '\n';
    return out;
}

class JsonReader {
public:
    JsonReader(const std::string &text, std::vector<std::pair<std::string, std::string>> &out)
        : p_(text.c_str()), end_(text.c_str() + text.size()), out_(out)
    {
    }

    bool parse()
    {
        if (static_cast<size_t>(end_ - p_) > kMaxDocumentLength)
            return false;
        skip_ws();
        if (p_ >= end_ || *p_ != '{')
            return false;
        if (!parse_object(std::string(), 1))
            return false;
        skip_ws();
        return p_ == end_;
    }

private:
    static constexpr size_t kMaxDocumentLength = 1024 * 1024;
    static constexpr size_t kMaxNestingDepth = 64;
    static constexpr size_t kMaxStringLength = 64 * 1024;

    const char *p_;
    const char *end_;
    std::vector<std::pair<std::string, std::string>> &out_;

    void skip_ws()
    {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r'))
            ++p_;
    }

    static int hex_value(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool parse_hex_quad(std::uint32_t &value)
    {
        if (end_ - p_ < 4) return false;
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const int digit = hex_value(*p_++);
            if (digit < 0) return false;
            value = (value << 4) | static_cast<std::uint32_t>(digit);
        }
        return true;
    }

    static void append_utf8(std::uint32_t value, std::string &out)
    {
        if (value <= 0x7f) {
            out += static_cast<char>(value);
        } else if (value <= 0x7ff) {
            out += static_cast<char>(0xc0 | (value >> 6));
            out += static_cast<char>(0x80 | (value & 0x3f));
        } else if (value <= 0xffff) {
            out += static_cast<char>(0xe0 | (value >> 12));
            out += static_cast<char>(0x80 | ((value >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (value & 0x3f));
        } else {
            out += static_cast<char>(0xf0 | (value >> 18));
            out += static_cast<char>(0x80 | ((value >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((value >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (value & 0x3f));
        }
    }

    bool parse_string(std::string &s)
    {
        if (p_ >= end_ || *p_ != '"')
            return false;
        ++p_;
        while (p_ < end_ && *p_ != '"') {
            char c = *p_++;
            if (static_cast<unsigned char>(c) < 0x20)
                return false;
            if (c == '\\' && p_ < end_) {
                char e = *p_++;
                switch (e) {
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                case '/': s += '/'; break;
                case 'u': {
                    std::uint32_t value = 0;
                    if (!parse_hex_quad(value)) return false;
                    if (value >= 0xd800 && value <= 0xdbff) {
                        if (end_ - p_ < 2 || p_[0] != '\\' || p_[1] != 'u') return false;
                        p_ += 2;
                        std::uint32_t low = 0;
                        if (!parse_hex_quad(low) || low < 0xdc00 || low > 0xdfff) return false;
                        value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
                    } else if (value >= 0xdc00 && value <= 0xdfff) {
                        return false;
                    }
                    append_utf8(value, s);
                    break;
                }
                default: return false;
                }
            } else {
                s += c;
            }
            if (s.size() > kMaxStringLength)
                return false;
        }
        if (p_ >= end_)
            return false;
        ++p_; // closing quote
        return true;
    }

    bool parse_number_token(std::string &token)
    {
        const char *start = p_;
        if (p_ < end_ && *p_ == '-') ++p_;
        if (p_ >= end_) return false;
        if (*p_ == '0') {
            ++p_;
            if (p_ < end_ && std::isdigit(static_cast<unsigned char>(*p_))) return false;
        } else if (*p_ >= '1' && *p_ <= '9') {
            while (p_ < end_ && std::isdigit(static_cast<unsigned char>(*p_))) ++p_;
        } else {
            return false;
        }
        bool integer = true;
        if (p_ < end_ && *p_ == '.') {
            integer = false;
            ++p_;
            const char *fraction = p_;
            while (p_ < end_ && std::isdigit(static_cast<unsigned char>(*p_))) ++p_;
            if (p_ == fraction) return false;
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            integer = false;
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            const char *exponent = p_;
            while (p_ < end_ && std::isdigit(static_cast<unsigned char>(*p_))) ++p_;
            if (p_ == exponent) return false;
        }
        token.assign(start, static_cast<size_t>(p_ - start));
        if (integer) {
            std::int64_t value = 0;
            const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
            if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) return false;
        }
        return true;
    }

    bool consume_literal(const char *literal)
    {
        const char *cursor = literal;
        while (*cursor && p_ < end_ && *p_ == *cursor) {
            ++p_;
            ++cursor;
        }
        return *cursor == '\0';
    }

    bool skip_value(size_t depth)
    {
        if (depth > kMaxNestingDepth) return false;
        skip_ws();
        if (p_ >= end_) return false;
        if (*p_ == '"') {
            std::string ignored;
            return parse_string(ignored);
        }
        if (*p_ == '{') return skip_object(depth + 1);
        if (*p_ == '[') return skip_array(depth + 1);
        if (*p_ == 't') return consume_literal("true");
        if (*p_ == 'f') return consume_literal("false");
        if (*p_ == 'n') return consume_literal("null");
        std::string ignored;
        return parse_number_token(ignored);
    }

    bool skip_array(size_t depth)
    {
        if (depth > kMaxNestingDepth || p_ >= end_ || *p_++ != '[') return false;
        skip_ws();
        if (p_ < end_ && *p_ == ']') { ++p_; return true; }
        while (skip_value(depth)) {
            skip_ws();
            if (p_ < end_ && *p_ == ',') { ++p_; continue; }
            if (p_ < end_ && *p_ == ']') { ++p_; return true; }
            return false;
        }
        return false;
    }

    bool skip_object(size_t depth)
    {
        if (depth > kMaxNestingDepth || p_ >= end_ || *p_++ != '{') return false;
        skip_ws();
        if (p_ < end_ && *p_ == '}') { ++p_; return true; }
        while (p_ < end_) {
            skip_ws();
            std::string key;
            if (!parse_string(key)) return false;
            skip_ws();
            if (p_ >= end_ || *p_++ != ':') return false;
            if (!skip_value(depth)) return false;
            skip_ws();
            if (p_ < end_ && *p_ == ',') { ++p_; continue; }
            if (p_ < end_ && *p_ == '}') { ++p_; return true; }
            return false;
        }
        return false;
    }

    bool parse_value(const std::string &prefix, size_t depth)
    {
        skip_ws();
        if (p_ >= end_)
            return false;
        char c = *p_;
        if (c == '{')
            return parse_object(prefix, depth + 1);
        if (c == '[')
            return skip_array(depth + 1);
        if (c == '"') {
            std::string s;
            if (!parse_string(s))
                return false;
            record(prefix, s);
            return true;
        }
        std::string tok;
        if (c == 't' && consume_literal("true"))
            tok = "1";
        else if (c == 'f' && consume_literal("false"))
            tok = "0";
        else if (c == 'n' && consume_literal("null"))
            tok = "";
        else if (!parse_number_token(tok))
            return false;
        record(prefix, tok);
        return true;
    }

    bool parse_object(const std::string &prefix, size_t depth)
    {
        if (depth > kMaxNestingDepth)
            return false;
        skip_ws();
        if (p_ >= end_ || *p_ != '{')
            return false;
        ++p_;
        skip_ws();
        if (p_ < end_ && *p_ == '}') {
            ++p_;
            return true;
        }
        while (p_ < end_) {
            skip_ws();
            std::string key;
            if (!parse_string(key))
                return false;
            skip_ws();
            if (p_ >= end_ || *p_ != ':')
                return false;
            ++p_;
            std::string child = prefix.empty() ? key : (prefix + "." + key);
            if (!parse_value(child, depth))
                return false;
            skip_ws();
            if (p_ < end_ && *p_ == ',') {
                ++p_;
                continue;
            }
            if (p_ < end_ && *p_ == '}') {
                ++p_;
                return true;
            }
            return false;
        }
        return false;
    }

    void record(const std::string &prefix, const std::string &val)
    {
        if (!prefix.empty())
            out_.emplace_back(prefix, val);
    }
};

inline bool from_json(const std::string &text,
                      std::vector<std::pair<std::string, std::string>> &out)
{
    try {
        std::vector<std::pair<std::string, std::string>> parsed;
        JsonReader reader(text, parsed);
        if (!reader.parse())
            return false;
        out.swap(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace cp0cfg
