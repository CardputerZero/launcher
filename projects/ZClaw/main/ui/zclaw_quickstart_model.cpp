#include "zclaw_quickstart_model.h"

namespace zclaw {

ProviderConfig normalize_setup_provider(ProviderConfig provider)
{
    if (provider.alias.empty())
        provider.alias = "zclaw";
    if (provider.family.empty())
        provider.family = "openai";
    if (provider.model.empty())
        provider.model = "gpt-4.1-mini";
    if (provider.uri.empty() && provider.family == "ollama")
        provider.uri = "http://127.0.0.1:11434";
    return provider;
}

std::string extract_pairing_code(const std::string &text)
{
    std::string digits;
    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            digits += ch;
            if (digits.size() == 6)
                return digits;
        } else {
            digits.clear();
        }
    }
    return "";
}

}  // namespace zclaw
