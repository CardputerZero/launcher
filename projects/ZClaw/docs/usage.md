# ZClaw Usage Guide

This document describes the behavior implemented by the current ZClaw UI.

## Startup requirements

ZClaw checks for public internet access before enabling the main interface. It tries several HTTPS endpoints with short timeouts. If none responds, the application shows an offline screen; press `Enter` to exit, restore networking, and launch ZClaw again.

On the first successful start, ZClaw opens the quickstart panel automatically. Quickstart is required when any of the following is true:

- `~/.zeroclaw` does not exist.
- `~/.zeroclaw/config.toml` does not exist.
- The UI has not saved a completed setup state.
- The UI has no saved bearer token.

## Quickstart

Quickstart configures a local ZeroClaw gateway and pairs the UI with it.

1. Choose a model provider.
2. Enter the required credentials or endpoint fields.
3. Select **Confirm / Quickstart**.
4. Wait while ZClaw downloads ZeroClaw if necessary, writes its configuration, installs and starts the service, creates a pairing code, and pairs automatically.

The available presets are:

| Provider | Default model | Default API URL | Required input |
| --- | --- | --- | --- |
| OpenAI | `gpt-4.1-mini` | `https://api.openai.com/v1` | API key |
| OpenRouter | `openrouter/auto` | `https://openrouter.ai/api/v1` | API key |
| Anthropic | `claude-sonnet-4` | `https://api.anthropic.com` | API key |
| Ollama | `llama3.1` | `http://127.0.0.1:11434` | None |
| DeepSeek | `deepseek-chat` | `https://api.deepseek.com` | API key |
| Custom | Empty | `https://api.example.com/v1` | API URL, API key, and model name |

For a custom provider, the API URL must begin with `http://` or `https://`. Quickstart uses the agent alias `zclaw` unless the saved UI configuration contains another alias.

Model Settings saves the selected provider and each API URL, API key, or model value as soon as the entry is confirmed with `Enter`. Confirmed values are restored the next time ZClaw starts, even if Quickstart has not been run.

Switching provider families preserves each family's saved values. Selecting a family that has been configured before restores that record; defaults are used only for a family with no saved record.

The generated ZeroClaw configuration binds the gateway to `127.0.0.1:42617`, requires pairing, uses a 180-second request timeout, and allows 600 seconds for long-running requests. The UI endpoint is reset to `http://127.0.0.1:42617/webhook`.

## Chat

- Press `Enter` from the chat view to open the message editor.
- Press `Enter` in the editor to send.
- Press `Shift+Enter` to insert a newline.
- Press `Esc` to close the editor without sending.
- Use the arrow keys, `Backspace`, and `Delete` for normal text editing.
- Use `Up` and `Down` from the chat view to scroll the conversation.

After quickstart, chat uses a WebSocket connection to the gateway at `/ws/chat`. The saved agent alias, bearer token, session ID `zclaw-ui`, and client name `ZClaw` are included in the connection. Responses may arrive as streamed chunks before the final response.

If the bearer token is cleared, the UI falls back to an HTTP webhook request. Webhook mode supports normal chat responses but does not provide interactive tool approvals.

## Tool approvals

When ZeroClaw requests permission to run a tool, ZClaw shows **Yes**, **Always**, and **No** choices.

- Use `Left` and `Right` to select a choice, then press `Enter`.
- Press `Y` for **Yes**.
- Press `A` for **Always**.
- Press `N`, `Esc`, or `Backspace` for **No**.
- `Z`, `X`, `C`, and `F` are alternate left, down, right, and up navigation keys.

If no decision is made before the request timeout, ZClaw denies the request.

## Settings

Press `Tab` to open or close settings. Inside settings, use `Up` and `Down` to move, `Enter` to select or edit, and `Esc` or `Backspace` to return.

The main settings page shows:

- **Setup**: rerun quickstart.
- **Authorization**: enter a pairing code, inspect pairing state, or clear the saved token.
- **Providers**: add, edit, or delete provider definitions.
- **Agent**: display the active agent alias.
- **Transport**: display `WS` when paired or `Webhook` when no bearer token is saved.

Provider records contain an alias, provider family, model, URI, and API key. Press `Delete` from a provider detail page to remove that record.

## Local files and secrets

ZClaw stores its state under `~/.zeroclaw`:

| Path | Purpose |
| --- | --- |
| `~/.zeroclaw/bin/zeroclaw` | Managed ZeroClaw executable |
| `~/.zeroclaw/config.toml` | ZeroClaw gateway, agent, and provider configuration |
| `~/.zeroclaw/zclaw_ui.tsv` | UI endpoint, agent alias, webhook secret, bearer token, and setup state |
| `~/.zeroclaw/zclaw_providers.tsv` | Provider aliases, models, endpoints, and API keys |

The TSV files are plain text. Protect the home directory and avoid copying these files into logs, bug reports, or source control because they may contain API keys and authorization tokens.

The bundled installer currently downloads the AArch64 GNU/Linux ZeroClaw `v0.8.2` archive. Existing executable files at `~/.zeroclaw/bin/zeroclaw` are reused instead of downloaded again.
