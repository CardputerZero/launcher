#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <utility>

class LoraSystem
{
public:
    void api_call(std::list<std::string> arg, std::function<void(int, std::string)> callback)
    {
        if (arg.empty()) {
            report(callback, -1, "missing lora api command\n");
            return;
        }

        const std::string command = arg.front();
        if (command == "Init") {
            initialized_ = true;
            hw_ready_ = true;
            std::snprintf(diag_, sizeof(diag_), "SDL simulated LoRa ready");
            report(callback, 0, "");
            return;
        }
        if (command == "Poll" || command == "Info") {
            cp0_lora_info_t info{};
            fill_info(&info, command == "Poll");
            report(callback, 0, std::string(reinterpret_cast<const char *>(&info), sizeof(info)));
            return;
        }
        if (command == "SendText") {
            std::snprintf(last_tx_, sizeof(last_tx_), "%s", first_arg_after_command(arg).c_str());
            std::snprintf(last_rx_, sizeof(last_rx_), "SDL echo: %.117s", last_tx_);
            has_sent_message_ = true;
            tx_event_ = true;
            rx_event_ = true;
            rssi_ = -42.0f;
            snr_ = 9.5f;
            std::snprintf(diag_, sizeof(diag_), "SDL simulated packet sent");
            report(callback, 0, "");
            return;
        }
        if (command == "StartReceive") {
            tx_mode_ = false;
            std::snprintf(diag_, sizeof(diag_), "SDL simulated receive mode");
            report(callback, 0, "");
            return;
        }
        if (command == "SetTxMode") {
            tx_mode_ = std::atoi(first_arg_after_command(arg).c_str()) != 0;
            std::snprintf(diag_, sizeof(diag_), tx_mode_ ? "SDL simulated TX mode" : "SDL simulated RX mode");
            report(callback, 0, "");
            return;
        }
        if (command == "Shutdown") {
            initialized_ = false;
            hw_ready_ = false;
            tx_mode_ = false;
            std::snprintf(diag_, sizeof(diag_), "SDL simulated LoRa shutdown");
            report(callback, 0, "");
            return;
        }

        report(callback, -1, "unknown lora api\n");
    }

private:
    bool initialized_ = false;
    bool hw_ready_ = false;
    bool tx_mode_ = false;
    bool has_sent_message_ = false;
    bool rx_event_ = false;
    bool tx_event_ = false;
    float rssi_ = -48.0f;
    float snr_ = 7.0f;
    char last_rx_[128] = "SDL simulated receive buffer";
    char last_tx_[128] = "Hello from SDL LoRa";
    char diag_[256] = "SDL simulated LoRa idle";

    static std::string first_arg_after_command(const std::list<std::string>& arg)
    {
        if (arg.size() < 2)
            return "";
        return *std::next(arg.begin());
    }

    void fill_info(cp0_lora_info_t *info, bool drain_events)
    {
        if (!info) return;
        std::memset(info, 0, sizeof(*info));
        info->initialized = initialized_ ? 1 : 0;
        info->hw_ready = hw_ready_ ? 1 : 0;
        info->tx_mode = tx_mode_ ? 1 : 0;
        info->tx_in_progress = 0;
        info->has_sent_message = has_sent_message_ ? 1 : 0;
        info->rx_event = rx_event_ ? 1 : 0;
        info->tx_event = tx_event_ ? 1 : 0;
        std::snprintf(info->spi_device, sizeof(info->spi_device), "sdl://lora");
        std::snprintf(info->last_rx, sizeof(info->last_rx), "%s", last_rx_);
        std::snprintf(info->last_tx, sizeof(info->last_tx), "%s", last_tx_);
        std::snprintf(info->diag, sizeof(info->diag), "%s", diag_);
        std::snprintf(info->probe_summary, sizeof(info->probe_summary), "SDL simulated SPI/GPIO");
        std::snprintf(info->probe_display, sizeof(info->probe_display), "LoRa: SDL simulation");
        std::snprintf(info->pi4io_status, sizeof(info->pi4io_status), "SDL no-op PI4IO");
        info->rssi = rssi_;
        info->snr = snr_;
        if (drain_events) {
            rx_event_ = false;
            tx_event_ = false;
        }
    }

    static void report(std::function<void(int, std::string)> callback, int code, const std::string& data)
    {
        if (callback) callback(code, data);
    }
};

extern "C" void init_lora(void)
{
    std::shared_ptr<LoraSystem> lora = std::make_shared<LoraSystem>();
    cp0_signal_lora_api.append([lora](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        lora->api_call(arg, callback);
    });
}
