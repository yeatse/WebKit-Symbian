// Link against the actual EKA connection.cpp, not a copy of its state logic.
#include <services/socket/connection.h>
#include <cassert>
#include <cstdio>
#include <tuple>

using namespace eka2l1::epoc::socket;
static unsigned checks;
static void check(bool value) { assert(value); ++checks; }

int main()
{
    connection_registry registry;
    std::vector<std::tuple<unsigned, int, bool, bool>> events;
    registry.events()->observers[&events] = [&](const connection_state& state, bool previous) {
        events.emplace_back(state.monitor_id, state.stage, previous, state.active);
    };
    auto first = registry.create();
    first->info = {1, 1, 10};
    check(first->monitor_id && first->monitor_id < 2000000 && registry.monitor_snapshot().empty());
    first->advance(conn_progress_connection_opened);
    check(events.size() == 1 && !std::get<2>(events.back()) && std::get<3>(events.back()));
    first->advance(conn_progress_link_layer_open);
    check(events.size() == 2 && std::get<2>(events.back()) && std::get<3>(events.back()));
    first->advance(conn_progress_link_layer_open);
    check(events.size() == 2);
    check(registry.monitor_snapshot() == std::vector<unsigned>{first->monitor_id});
    check(registry.find(first->info) == first && registry.monitor_find(first->monitor_id) == first);
    auto second = registry.create();
    second->info = {1, 2, 20};
    second->advance(conn_progress_link_layer_open);
    check(second->monitor_id != first->monitor_id && registry.monitor_snapshot().size() == 2);
    const auto snapshot = registry.monitor_snapshot();
    first->authoritative_stop = true;
    first->advance(conn_progress_link_layer_closed);
    first->advance(conn_progress_connection_closed);
    check(first->authoritative_stop);
    check(snapshot.size() == 2 && registry.monitor_snapshot() == std::vector<unsigned>{second->monitor_id});
    check(!registry.find(first->info) && registry.monitor_find(first->monitor_id)->stage == 4500);
    second.reset(); // Last real owner disappears, emitting close without a synthetic switch.
    check(registry.monitor_snapshot().empty());
    check(std::get<1>(events[events.size() - 2]) == 8000 && std::get<2>(events[events.size() - 2]));
    check(std::get<1>(events.back()) == 4500 && !std::get<3>(events.back()));

    auto third = registry.create();
    third->info = {1, 3, 30};
    third->advance(conn_progress_link_layer_open);
    auto normalAttachment = registry.find(third->info);
    std::weak_ptr<connection_state> monitorAttachment = third;
    auto eventCount = events.size();
    third.reset();
    check(registry.monitor_snapshot().size() == 1 && events.size() == eventCount);
    normalAttachment.reset();
    check(monitorAttachment.expired() && registry.monitor_snapshot().empty() && events.size() == eventCount + 2);

    unsigned removedCallbackCalls = 0;
    auto notifications = registry.events();
    auto* firstKey = reinterpret_cast<void*>(1);
    auto* removedKey = reinterpret_cast<void*>(2);
    notifications->observers[firstKey] = [&](const connection_state&, bool) { notifications->observers.erase(removedKey); };
    notifications->observers[removedKey] = [&](const connection_state&, bool) { ++removedCallbackCalls; };
    first->advance(conn_progress_link_layer_open);
    check(!first->authoritative_stop);
    check(!removedCallbackCalls);
    notifications->observers.erase(firstKey);
    notifications->observers.erase(&events);
    eventCount = events.size();
    first->advance(conn_progress_link_layer_closed);
    check(events.size() == eventCount);

    std::shared_ptr<connection_state> surviving;
    {
        connection_registry shortLived;
        surviving = shortLived.create();
        surviving->advance(conn_progress_link_layer_open);
    }
    surviving.reset(); // No dangling registry callback after registry destruction.
    check(true);
    std::printf("PASS: %u real connection registry checks\n", checks);
}
