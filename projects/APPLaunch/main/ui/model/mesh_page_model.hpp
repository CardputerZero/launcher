#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct MeshMessage {
    std::string timestamp;
    std::string sender;
    std::string text;
};

struct MeshNeighbor {
    std::string node_id;
    int rssi = 0;
    int hops = 0;
    std::string last_seen;
};

class MeshPageModel
{
public:
    static constexpr size_t MAX_MESSAGES = 20;

    void initialize(uint32_t seed);
    void refresh(const std::string &timestamp);
    void heartbeat(const std::string &timestamp);
    void add_message(std::string timestamp, std::string sender, std::string text);

    const std::string &node_id() const { return node_id_; }
    const std::vector<MeshNeighbor> &neighbors() const { return neighbors_; }
    const std::vector<MeshMessage> &messages() const { return messages_; }
    int channel() const { return 7; }
    int frequency_mhz() const { return 915; }
    bool lora_detected() const { return false; }

private:
    uint32_t next_random();

    std::string node_id_ = "0x0000";
    std::vector<MeshNeighbor> neighbors_;
    std::vector<MeshMessage> messages_;
    std::minstd_rand random_{1};
    int heartbeat_count_ = 0;
};
