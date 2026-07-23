#include "zclaw_provider_catalog.h"

#include <array>

namespace zclaw {
namespace {

struct ProviderPreset {
    const char *display_name;
    ProviderConfig config;
};

const std::array<ProviderPreset, 6> kProviderPresets = {{
    {"OpenAI", {"zclaw", "openai", "gpt-4.1-mini", "https://api.openai.com/v1", ""}},
    {"OpenRouter", {"zclaw", "openrouter", "openrouter/auto", "https://openrouter.ai/api/v1", ""}},
    {"Anthropic", {"zclaw", "anthropic", "claude-sonnet-4", "https://api.anthropic.com", ""}},
    {"Ollama", {"zclaw", "ollama", "llama3.1", "http://127.0.0.1:11434", ""}},
    {"DeepSeek", {"zclaw", "deepseek", "deepseek-chat", "https://api.deepseek.com", ""}},
    {"Custom", {"zclaw", "custom", "", "https://api.example.com/v1", ""}},
}};

}  // namespace

std::size_t provider_preset_count()
{
    return kProviderPresets.size();
}

ProviderConfig provider_preset(std::size_t index)
{
    if (index >= kProviderPresets.size())
        index = kProviderPresets.size() - 1;
    return kProviderPresets[index].config;
}

std::size_t provider_preset_index(const std::string &family)
{
    for (std::size_t index = 0; index < kProviderPresets.size(); ++index) {
        if (kProviderPresets[index].config.family == family)
            return index;
    }
    return kProviderPresets.size() - 1;
}

const char *provider_display_name(const std::string &family)
{
    for (const ProviderPreset &preset : kProviderPresets) {
        if (preset.config.family == family)
            return preset.display_name;
    }
    return "Custom";
}

std::vector<ProviderConfig> default_provider_configs()
{
    std::vector<ProviderConfig> configs;
    configs.reserve(kProviderPresets.size());
    for (const ProviderPreset &preset : kProviderPresets)
        configs.push_back(preset.config);
    return configs;
}

}  // namespace zclaw
