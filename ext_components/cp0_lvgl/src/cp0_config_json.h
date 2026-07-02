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

#include <cctype>
#include <cstdlib>
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
    // Only treat as a JSON number if it round-trips, so values like "007" keep
    // their string form and get_str() returns them unchanged.
    long n = std::atol(v.c_str());
    return std::to_string(n) == v;
}

struct JsonNode {
    std::map<std::string, JsonNode> children;
    std::string value;
    bool numeric = false;
};

inline void json_escape(const std::string &s, std::string &out)
{
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
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
        skip_ws();
        if (p_ >= end_ || *p_ != '{')
            return false;
        return parse_object(std::string());
    }

private:
    const char *p_;
    const char *end_;
    std::vector<std::pair<std::string, std::string>> &out_;

    void skip_ws()
    {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r'))
            ++p_;
    }

    bool parse_string(std::string &s)
    {
        if (p_ >= end_ || *p_ != '"')
            return false;
        ++p_;
        while (p_ < end_ && *p_ != '"') {
            char c = *p_++;
            if (c == '\\' && p_ < end_) {
                char e = *p_++;
                switch (e) {
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                case '/': s += '/'; break;
                default: s += e; break;
                }
            } else {
                s += c;
            }
        }
        if (p_ >= end_)
            return false;
        ++p_; // closing quote
        return true;
    }

    bool skip_container(char close)
    {
        ++p_; // opening bracket/brace
        while (p_ < end_) {
            skip_ws();
            if (p_ >= end_)
                return false;
            if (*p_ == close) {
                ++p_;
                return true;
            }
            if (*p_ == '"') {
                std::string t;
                if (!parse_string(t))
                    return false;
            } else if (*p_ == '{') {
                if (!skip_container('}'))
                    return false;
            } else if (*p_ == '[') {
                if (!skip_container(']'))
                    return false;
            } else {
                ++p_;
            }
        }
        return false;
    }

    bool parse_value(const std::string &prefix)
    {
        skip_ws();
        if (p_ >= end_)
            return false;
        char c = *p_;
        if (c == '{')
            return parse_object(prefix);
        if (c == '[')
            return skip_container(']');
        if (c == '"') {
            std::string s;
            if (!parse_string(s))
                return false;
            record(prefix, s);
            return true;
        }
        const char *start = p_;
        while (p_ < end_ && *p_ != ',' && *p_ != '}' && *p_ != ']' && *p_ != ' ' &&
               *p_ != '\t' && *p_ != '\n' && *p_ != '\r')
            ++p_;
        std::string tok(start, static_cast<size_t>(p_ - start));
        if (tok.empty())
            return false;
        if (tok == "true")
            tok = "1";
        else if (tok == "false")
            tok = "0";
        else if (tok == "null")
            tok = "";
        record(prefix, tok);
        return true;
    }

    bool parse_object(const std::string &prefix)
    {
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
            if (!parse_value(child))
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
    JsonReader reader(text, out);
    return reader.parse();
}

} // namespace cp0cfg
