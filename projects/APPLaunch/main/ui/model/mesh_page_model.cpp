#include "mesh_page_model.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

void MeshPageModel::initialize(uint32_t seed)
{
    random_.seed(seed ? seed : 1);
    char node_id[16];
    std::snprintf(node_id, sizeof(node_id), "0x%04X", seed & 0xffff);
    node_id_ = node_id;
    neighbors_.clear();
    messages_.clear();
    heartbeat_count_ = 0;
}

void MeshPageModel::refresh(const std::string &timestamp)
{
    neighbors_.clear();
    const int count = static_cast<int>(next_random() % 4);
    for (int index = 0; index < count; ++index) {
        char node_id[16];
        std::snprintf(node_id, sizeof(node_id), "0x%04X", next_random() & 0xffff);
        neighbors_.push_back({node_id, -(40 + static_cast<int>(next_random() % 60)),
                              1 + static_cast<int>(next_random() % 3), timestamp});
    }

    char status[64];
    if (count > 0) {
        std::snprintf(status, sizeof(status), "Scan: %d node(s) found", count);
    } else {
        std::snprintf(status, sizeof(status), "Scan: no nodes nearby");
    }
    add_message(timestamp, "SYS", status);
}

void MeshPageModel::heartbeat(const std::string &timestamp)
{
    ++heartbeat_count_;
    if (heartbeat_count_ % 2 == 0 && !neighbors_.empty()) {
        static constexpr const char *messages[] = {"PING", "ACK", "HELLO", "STATUS:OK", "HEARTBEAT"};
        const auto &neighbor = neighbors_[next_random() % neighbors_.size()];
        add_message(timestamp, neighbor.node_id, messages[next_random() % 5]);
    }
    for (auto &neighbor : neighbors_) {
        neighbor.rssi += static_cast<int>(next_random() % 7) - 3;
        neighbor.rssi = std::max(-100, std::min(-30, neighbor.rssi));
        neighbor.last_seen = timestamp;
    }
}

void MeshPageModel::add_message(std::string timestamp, std::string sender, std::string text)
{
    messages_.push_back({std::move(timestamp), std::move(sender), std::move(text)});
    if (messages_.size() > MAX_MESSAGES) messages_.erase(messages_.begin());
}

uint32_t MeshPageModel::next_random()
{
    return random_();
}
