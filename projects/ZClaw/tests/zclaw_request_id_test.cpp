#include "zclaw_request_id.h"

#include <cassert>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

int main()
{
    zclaw::RequestIdGenerator fixed([] { return 1234567890U; });
    assert(fixed.next() == "zclaw-ui-1234567890-0");
    assert(fixed.next() == "zclaw-ui-1234567890-1");

    constexpr int kThreadCount = 8;
    constexpr int kIdsPerThread = 500;
    zclaw::RequestIdGenerator concurrent([] { return 42U; });
    std::mutex ids_mutex;
    std::vector<std::string> ids;
    ids.reserve(kThreadCount * kIdsPerThread);
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int thread = 0; thread < kThreadCount; ++thread) {
        threads.emplace_back([&] {
            std::vector<std::string> local;
            local.reserve(kIdsPerThread);
            for (int index = 0; index < kIdsPerThread; ++index)
                local.push_back(concurrent.next());
            std::lock_guard<std::mutex> lock(ids_mutex);
            ids.insert(ids.end(), local.begin(), local.end());
        });
    }
    for (std::thread &thread : threads)
        thread.join();

    const std::set<std::string> unique(ids.begin(), ids.end());
    assert(ids.size() == kThreadCount * kIdsPerThread);
    assert(unique.size() == ids.size());
    for (const std::string &id : ids)
        assert(id.rfind("zclaw-ui-42-", 0) == 0);
    return 0;
}
