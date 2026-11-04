#include <cassert>
#include <cstdint>
#include <iostream>
namespace eka2l1 {
namespace kernel { using handle = std::uint32_t; struct thread {}; struct process {}; }
template<class T> struct ptr {
    std::uintptr_t value;
    ptr(std::uintptr_t v = 0) : value(v) {}
    template<class U> ptr<U> cast() const { return ptr<U>(value); }
    T *get(kernel::process *) const { return reinterpret_cast<T *>(value); }
};
namespace epoc {
constexpr std::int32_t error_not_found = -1;
struct request_status {};
struct notify_info { notify_info(ptr<request_status> &, kernel::thread *) {} };
}
namespace ldd { struct channel {
    std::int32_t result = 0;
    unsigned calls = 0;
    std::int32_t do_control(kernel::thread *, std::uint32_t op, ptr<void> a, ptr<void> b) {
        assert(op == 4 && a.value == 7 && b.value == 9); ++calls; return result;
    }
    std::int32_t do_request(epoc::notify_info, std::uint32_t, ptr<void>, ptr<void>, bool) { return -5; }
}; }
struct kernel_system {
    ldd::channel *active;
    kernel::thread requester;
    kernel::process process;
    template<class T> T *get(kernel::handle h) { return h == 0x40003e ? static_cast<T *>(active) : nullptr; }
    kernel::thread *crr_thread() { return &requester; }
    kernel::process *crr_process() { return &process; }
};
namespace epoc {
#define BRIDGE_FUNC(ret, name, ...) ret name(kernel_system *kern, __VA_ARGS__)
BRIDGE_FUNC(std::int32_t, logical_channel_do_control, const kernel::handle h, const int func,
        eka2l1::ptr<void> arg1, eka2l1::ptr<void> arg2) {
        ldd::channel *chn = kern->get<ldd::channel>(h);

        if (!chn) {
            return epoc::error_not_found;
        }

        return chn->do_control(kern->crr_thread(), func, arg1, arg2);
    }
BRIDGE_FUNC(std::int32_t, logical_channel_do_request, const kernel::handle h, const int func,
        eka2l1::ptr<epoc::request_status> status, const std::uint32_t *args) {
        ldd::channel *chn = kern->get<ldd::channel>(h);

        if (!chn) {
            return epoc::error_not_found;
        }

        epoc::notify_info request_nof_info(status, kern->crr_thread());
        return chn->do_request(request_nof_info, func, args[0], args[1], false);
    }
BRIDGE_FUNC(std::int32_t, logical_channel_request, const kernel::handle h, const int func,
        eka2l1::ptr<void> arg1, eka2l1::ptr<void> arg2) {
        if (func < 0) {
            return logical_channel_do_request(kern, h, ~func, arg1.cast<epoc::request_status>(),
                arg2.cast<std::uint32_t>().get(kern->crr_process()));
        }
        return logical_channel_do_control(kern, h, func, arg1, arg2);
    }
}
}
int main() {
    eka2l1::ldd::channel channel;
    eka2l1::kernel_system kernel{&channel, {}, {}};
    auto registered = &eka2l1::epoc::logical_channel_request;
    for (const std::int32_t expected : {0, -6, 0x400099}) {
        channel.result = expected;
        assert(registered(&kernel, 0x40003e, 4, eka2l1::ptr<void>(7), eka2l1::ptr<void>(9)) == expected);
    }
    assert(channel.calls == 3);
    assert(registered(&kernel, 0x1234, 4, eka2l1::ptr<void>(7), eka2l1::ptr<void>(9)) == -1);
    assert(channel.calls == 3);
    std::cout << "PASS: exact production handlers preserve success, error, positive handle, and invalid-channel error\n";
}
