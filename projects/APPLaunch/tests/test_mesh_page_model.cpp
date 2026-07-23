#include "../main/ui/model/mesh_page_model.hpp"
#include "../main/ui/model/mesh_page_contract.hpp"

#include <cassert>
#include <string>

int main()
{
    static_assert(noexcept(mesh_heartbeat_callback_allowed(true, true)));
    static_assert(mesh_heartbeat_callback_allowed(true, true));
    static_assert(!mesh_heartbeat_callback_allowed(false, true));
    static_assert(!mesh_heartbeat_callback_allowed(true, false));

    int delete_target = 0;
    int bubbled_target = 0;
    static_assert(noexcept(mesh_owned_delete_callback_allowed(
        &delete_target, &delete_target)));
    assert(mesh_owned_delete_callback_allowed(&delete_target, &delete_target));
    assert(!mesh_owned_delete_callback_allowed(&delete_target, &bubbled_target));
    assert(!mesh_owned_delete_callback_allowed(
        static_cast<int *>(nullptr), &delete_target));

    int root_screen = 0;
    int stale_root = 0;
    static_assert(noexcept(mesh_root_event_callback_allowed(
        &root_screen, &root_screen)));
    assert(mesh_root_event_callback_allowed(&root_screen, &root_screen));
    assert(!mesh_root_event_callback_allowed(&stale_root, &root_screen));
    assert(!mesh_root_event_callback_allowed(
        static_cast<int *>(nullptr), &root_screen));

    uint32_t random_value = 7;
    assert(mesh_parse_random_u32("4294967295", random_value));
    assert(random_value == UINT32_MAX);
    assert(!mesh_parse_random_u32("4294967296", random_value));
    assert(!mesh_parse_random_u32("12junk", random_value));
    assert(random_value == UINT32_MAX);
    int hour = -1;
    int minute = -1;
    int second = -1;
    assert(mesh_parse_local_time("2026,7,22,9,8,7", hour, minute, second));
    assert(hour == 9 && minute == 8 && second == 7);
    assert(!mesh_parse_local_time("2026,13,22,9,8,7", hour, minute, second));
    assert(!mesh_parse_local_time("2026,7,22,24,8,7", hour, minute, second));
    assert(!mesh_parse_local_time("2026,7,22,9,8,7,extra", hour, minute, second));

    int background = 0;
    int neighbor_area = 0;
    int message_area = 0;
    assert(mesh_page_ui_ready(&background, &neighbor_area, &message_area));
    assert(!mesh_page_ui_ready(nullptr, &neighbor_area, &message_area));
    assert(!mesh_page_ui_ready(&background, nullptr, &message_area));
    assert(!mesh_page_ui_ready(&background, &neighbor_area, nullptr));
    assert(mesh_page_ui_ready(&background, &neighbor_area, &message_area, &hour));
    assert(!mesh_page_ui_ready(&background, &neighbor_area, &message_area, nullptr));

    MeshPageModel model;
    model.initialize(0x12345);
    assert(model.node_id() == "0x2345");
    assert(model.neighbors().empty() && model.messages().empty());

    uint32_t seed = 1;
    do {
        model.initialize(seed++);
        model.refresh("10:00:00");
    } while (model.neighbors().empty() && seed < 100);
    assert(!model.neighbors().empty() && model.neighbors().size() <= 3);
    assert(model.messages().size() == 1);
    assert(model.messages().back().sender == "SYS");
    for (const auto &neighbor : model.neighbors()) {
        assert(neighbor.rssi >= -99 && neighbor.rssi <= -40);
        assert(neighbor.hops >= 1 && neighbor.hops <= 3);
        assert(neighbor.last_seen == "10:00:00");
    }

    model.heartbeat("10:00:01");
    assert(model.messages().size() == 1);
    model.heartbeat("10:00:02");
    assert(model.messages().size() == 2);
    for (const auto &neighbor : model.neighbors()) {
        assert(neighbor.rssi >= -100 && neighbor.rssi <= -30);
        assert(neighbor.last_seen == "10:00:02");
    }

    model.initialize(9);
    for (int index = 0; index < 25; ++index) {
        model.add_message("now", "ME", std::to_string(index));
    }
    assert(model.messages().size() == MeshPageModel::MAX_MESSAGES);
    assert(model.messages().front().text == "5");
    assert(model.messages().back().text == "24");
}
