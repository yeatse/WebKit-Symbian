#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

namespace eka2l1 {
thread_local bool kernel_held = false;
struct kernel_system {
    std::mutex mutex;
    bool wiping = false;
    int unlocks = 0;
    int locks = 0;
    void lock() { assert(!kernel_held); mutex.lock(); kernel_held = true; ++locks; }
    void unlock() { assert(kernel_held); kernel_held = false; ++unlocks; mutex.unlock(); }
    bool wipeout_in_progress() const { return wiping; }
};
namespace kernel {
enum class thread_state { run, stop };
struct thread {
    thread_state state = thread_state::run;
    thread_state current_state() const { assert(kernel_held); return state; }
};
}
enum class ipc_message_status { accepted, completed };
namespace service {
struct session {
    int references = 1;
    bool destroyed = false;
    int pins = 0;
    int unpins = 0;
    void increase_access_count() { assert(kernel_held && !destroyed); ++references; ++pins; }
    void decrease_access_count() { assert(kernel_held && !destroyed && references > 0); --references; ++unpins; destroyed = references == 0; }
};
struct message {
    session *msg_session;
    ipc_message_status msg_status = ipc_message_status::accepted;
    kernel::thread *own_thr;
};
struct ipc_context {
    message *msg;
    int completions = 0;
    int result = 12345;
    void complete(int value) { assert(kernel_held); ++completions; result = value; msg->msg_status = ipc_message_status::completed; }
};
}
namespace drivers {
enum class finite_finish_result { success, timed_out, aborted, not_supported };
struct graphics_driver {
    std::atomic<bool> is_aborted{false};
    finite_finish_result response = finite_finish_result::success;
    int calls = 0;
    std::uint32_t timeout = 0;
    std::function<void()> entered;
    bool aborted() const { return is_aborted.load(); }
    finite_finish_result finite_finish(std::uint32_t timeout_ms) {
        assert(!kernel_held);
        ++calls;
        timeout = timeout_ms;
        assert(timeout_ms > 0 && timeout_ms <= 2000);
        if (entered) entered();
        return response;
    }
};
}
namespace epoc {
constexpr int error_none = 0;
constexpr int error_not_ready = -18;
constexpr int error_timed_out = -33;
constexpr int error_not_supported = -5;
struct screen {
    enum { FLAG_SERVER_REDRAW_PENDING = 8, FLAG_CLIENT_REDRAW_PENDING = 16 };
    std::mutex screen_mutex;
    int flags_ = 0;
    screen *next = nullptr;
};
}
class window_server {
public:
    kernel_system *kernel;
    drivers::graphics_driver *driver;
    epoc::screen *screens;
    kernel_system *get_kernel_system() { return kernel; }
    drivers::graphics_driver *get_graphics_driver() { return driver; }
    void finish_rendering(service::ipc_context &context);
};
