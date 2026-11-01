#pragma once
#include <kernel/kernel_obj.h>
#include <utils/reqsts.h>
namespace eka2l1::kernel {
class thread {
public:
    unsigned signals = 0;
    std::uint64_t identity = 1;
    std::uint64_t unique_id() const { return identity; }
    entity_exit_type exit = entity_exit_type::pending;
    entity_exit_type get_exit_type() const { return exit; }
};
}
namespace eka2l1 {
class kernel_system {
public:
    kernel::thread owner, other;
    kernel::thread* current = &owner;
    bool wiping = false, alive = true;
    bool is_eka1() const { return false; }
    kernel::thread* crr_thread() { return current; }
    bool is_wiping() const { return wiping; }
    bool is_thread_alive(kernel::thread* t) const { return alive && t == &owner; }
};
inline void epoc::notify_info::complete(int code) {
    sts.get()->status = code;
    ++requester->signals;
    sts = 0;
}
}
