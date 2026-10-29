// Test boundary only. The ALF implementation and error constants are real EKA sources.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace eka2l1 {
namespace epoc { struct version {}; }
namespace kernel {
using uid = std::uint64_t;
enum class entity_exit_type { pending, kill };
struct process {};
struct thread {
    uid id;
    entity_exit_type exit{entity_exit_type::pending};
    process owner;
    uid unique_id() const { return id; }
    entity_exit_type get_exit_type() const { return exit; }
    process* owning_process() { return &owner; }
};
}
struct kernel_system {
    bool wiping{false};
    std::set<kernel::thread*> alive;
    bool is_wiping() const { return wiping; }
    bool is_thread_alive(kernel::thread* thread) const { return alive.count(thread); }
};
struct system {
    kernel_system kernel;
    kernel_system* get_kernel_system() { return &kernel; }
};
namespace service {
struct completion {
    int count{0};
    int code{123456};
    std::optional<std::int32_t> output;
    std::function<void()> on_complete;
};
struct request_pointer {
    bool mapped{true};
    void* get(kernel::process*) { return mapped ? this : nullptr; }
};
struct ipc_msg {
    int function;
    kernel::thread* own_thr;
    request_pointer request_sts;
};
struct ipc_context {
    eka2l1::system* sys;
    std::shared_ptr<ipc_msg> msg;
    std::optional<std::uint32_t> plugin;
    std::optional<std::uint32_t> command;
    std::optional<std::string> input;
    std::size_t output_capacity{4};
    bool output_writable{true};
    std::shared_ptr<completion> result{std::make_shared<completion>()};
    template<class T> std::optional<T> get_argument_value(int index) {
        if constexpr (std::is_same_v<T, std::string>)
            return index == 1 ? input : std::nullopt;
        else
            return index == 0 ? plugin : index == 1 ? command : std::nullopt;
    }
    std::size_t get_argument_max_data_size(int index) { return index == 2 ? output_capacity : 0; }
    bool write_data_to_descriptor_argument(int index, std::int32_t value) {
        if (index != 2 || !output_writable || output_capacity < 4)
            return false;
        result->output = value;
        return true;
    }
    void complete(int code) {
        ++result->count;
        result->code = code;
        if (result->on_complete)
            result->on_complete();
    }
    std::unique_ptr<ipc_context> move_to_new() { return std::make_unique<ipc_context>(*this); }
};
struct typical_server {
    typical_server(eka2l1::system*, const std::string&) {}
    virtual ~typical_server() = default;
    virtual void connect(ipc_context&) = 0;
    template<class T> void create_session(ipc_context*) {}
};
struct typical_session {
    typical_session(typical_server*, kernel::uid, epoc::version) {}
    virtual ~typical_session() = default;
    virtual void fetch(ipc_context*) = 0;
};
}
}
