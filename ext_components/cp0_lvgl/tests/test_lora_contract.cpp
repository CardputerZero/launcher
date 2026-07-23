#include "cp0_lora_contract.hpp"

#include <atomic>
#include <cassert>
#include <cstring>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    cp0::lora::Operations operations;
    bool initialized = false;
    bool drained = false;
    bool tx_mode = false;
    bool shutdown = false;
    int polls = 0;
    std::string sent;
    operations.initialize = [&] {
        initialized = true;
        return true;
    };
    operations.poll = [&] { ++polls; };
    operations.read_info = [&](cp0_lora_info_t *info, bool drain_events) {
        info->initialized = initialized ? 1 : 0;
        std::strcpy(info->last_rx, "received");
        drained = drain_events;
    };
    operations.send_text = [&](const std::string &payload) {
        sent = payload;
        return true;
    };
    operations.start_receive = [&] { tx_mode = false; };
    operations.set_tx_mode = [&](bool enabled) { tx_mode = enabled; };
    operations.shutdown = [&] { shutdown = true; };

    cp0::lora::Service service(operations);
    auto result = service.call({"Init"});
    assert(result.code == 0 && initialized && result.payload.empty());
    result = service.call({"Info"});
    assert(result.code == 0 && result.payload.size() == sizeof(cp0_lora_info_t));
    cp0_lora_info_t info{};
    assert(cp0::lora::decode_info(result.payload, &info));
    assert(info.initialized == 1 && std::strcmp(info.last_rx, "received") == 0);
    assert(!drained && polls == 0);
    result = service.call({"Poll"});
    assert(result.code == 0 && drained && polls == 1);

    const std::string maximum(cp0::lora::kMaxTextPayloadBytes, 'x');
    assert(service.call({"SendText", maximum}).code == 0 && sent == maximum);
    assert(service.call({"SendText"}).code == -1);
    assert(service.call({"SendText", ""}).code == -1);
    assert(service.call({"SendText", std::string(128, 'x')}).code == -1);
    assert(service.call({"SendText", std::string("ab\0cd", 5)}).code == -1);

    assert(service.call({"SetTxMode", "1"}).code == 0 && tx_mode);
    assert(service.call({"SetTxMode", "0"}).code == 0 && !tx_mode);
    for (const std::string invalid : {"", "-1", "+1", "2", "1junk", "999999999999999999"})
        assert(service.call({"SetTxMode", invalid}).code == -1);
    tx_mode = true;
    assert(service.call({"StartReceive"}).code == 0 && !tx_mode);
    assert(service.call({"Shutdown"}).code == 0 && shutdown);
    assert(service.call({}).payload == "missing lora api command\n");
    assert(service.call({"Unknown"}).payload == "unknown lora api\n");

    assert(!cp0::lora::decode_info({}, &info));
    assert(!cp0::lora::decode_info(std::string(sizeof(info) - 1, '\0'), &info));
    assert(!cp0::lora::decode_info(result.payload, nullptr));

    std::atomic<int> active{0};
    std::atomic<int> maximum_active{0};
    cp0::lora::Operations concurrent_operations;
    concurrent_operations.set_tx_mode = [&](bool) {
        const int now = active.fetch_add(1) + 1;
        int observed = maximum_active.load();
        while (now > observed && !maximum_active.compare_exchange_weak(observed, now)) {}
        for (int index = 0; index < 1000; ++index)
            std::this_thread::yield();
        active.fetch_sub(1);
    };
    cp0::lora::Service concurrent_service(concurrent_operations);
    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index)
        threads.emplace_back([&] { assert(concurrent_service.call({"SetTxMode", "1"}).code == 0); });
    for (auto &thread : threads)
        thread.join();
    assert(maximum_active.load() == 1);

    cp0::lora::Operations throwing_operations;
    throwing_operations.initialize = []() -> bool { throw std::runtime_error("init"); };
    throwing_operations.read_info = [](cp0_lora_info_t *, bool) {
        throw std::runtime_error("info");
    };
    cp0::lora::Service throwing_service(throwing_operations);
    result = throwing_service.call({"Init"});
    assert(result.code == -1 && result.payload == "lora backend failure\n");
    result = throwing_service.call({"Info"});
    assert(result.code == -1 && result.payload == "lora backend failure\n");
}
