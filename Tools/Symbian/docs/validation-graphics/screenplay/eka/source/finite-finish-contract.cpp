// Host-only contract checks for the actual inline finish state machine.
// This test never creates GL, an emulator, a guest, a display or a server.
#ifdef NDEBUG
#error Build this contract check with assertions enabled, for example -UNDEBUG.
#endif
#include <drivers/graphics/graphics.h>
#include <common/queue.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>

using namespace eka2l1;
using namespace eka2l1::drivers;

class finish_driver final : public graphics_driver {
    std::atomic<bool> stopped_{false};
    std::mutex queued_mutex_;
    std::condition_variable queued_condition_;
    std::deque<command_list> queued_;

    bool try_submit_finite_finish(command_list &commands) override {
        assert(commands.size_ == 1);
        assert(commands.base_[0].opcode_ == graphics_driver_finish);
        assert(commands.base_[0].status_ == nullptr);
        if (reject_admission || stopped_) {
            return false;
        }
        if (synchronous_completion) {
            complete_finite_finish(commands.base_[0].data_[0], finite_finish_result::success);
            delete[] commands.base_;
            return true;
        }
        {
            const std::lock_guard<std::mutex> lock(queued_mutex_);
            queued_.push_back(commands);
        }
        queued_condition_.notify_all();
        return true;
    }

public:
    bool reject_admission = false;
    bool synchronous_completion = false;

    finish_driver() : graphics_driver(graphic_api::opengl) {}
    ~finish_driver() override {
        for (auto &commands : queued_) {
            delete[] commands.base_;
        }
    }
    finite_finish_result finite_finish(std::uint32_t timeout = 2000) override {
        return queued_finite_finish(timeout);
    }
    void abort() override {
        stopped_ = true;
        abort_finite_finishes();
    }
    bool aborted() const override { return stopped_; }
    bool wait_queued(std::size_t count) {
        std::unique_lock<std::mutex> lock(queued_mutex_);
        return queued_condition_.wait_for(lock, std::chrono::seconds(1),
            [&]() { return queued_.size() >= count; });
    }
    bool complete_next() {
        command_list commands;
        {
            const std::lock_guard<std::mutex> lock(queued_mutex_);
            assert(!queued_.empty());
            commands = queued_.front();
            queued_.pop_front();
        }
        const auto token = commands.base_[0].data_[0];
        const bool pending = finite_finish_pending(token);
        if (pending) {
            complete_finite_finish(token, finite_finish_result::success);
        }
        delete[] commands.base_;
        return pending;
    }
    void run() override {}
    void update_bitmap(handle, std::size_t, const vec2 &, const vec2 &, const void *, std::size_t) override {}
    void set_viewport(const rect &) override {}
    void update_surface(void *) override {}
    void update_surface_size(const vec2 &) override {}
    void submit_command_list(command_list &commands) override { delete[] commands.base_; }
    void set_upscale_shader(const std::string &) override {}
    std::string get_active_upscale_shader() const override { return {}; }
    bool support_extension(graphics_driver_extension) override { return false; }
    bool query_extension_value(graphics_driver_extension_query, void *) override { return false; }
};

int main() {
    static_assert(graphics_driver_finish == graphics_driver_get_float + 1);
    {
        finish_driver driver;
        assert(driver.graphics_driver::finite_finish() == finite_finish_result::not_supported);
        assert(driver.finite_finish(0) == finite_finish_result::timed_out);
        driver.synchronous_completion = true;
        assert(driver.finite_finish(1000) == finite_finish_result::success);
    }
    {
        finish_driver driver;
        driver.reject_admission = true;
        const auto start = std::chrono::steady_clock::now();
        assert(driver.finite_finish(1000) == finite_finish_result::timed_out);
        assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
    }
    {
        // Legacy synchronous submission can hold driver::mut_ while waiting
        // for a full queue. The new deadline must not depend on that mutex.
        finish_driver driver;
        const std::lock_guard<std::mutex> hold_legacy(driver.mut_);
        auto pending = std::async(std::launch::async, [&]() { return driver.finite_finish(5); });
        assert(pending.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready);
        assert(pending.get() == finite_finish_result::timed_out);
        assert(!driver.complete_next());
    }
    {
        finish_driver driver;
        assert(driver.finite_finish(5) == finite_finish_result::timed_out);
        assert(!driver.complete_next()); // A late token cannot complete another caller.
        auto next = std::async(std::launch::async, [&]() { return driver.finite_finish(1000); });
        assert(driver.wait_queued(1));
        assert(driver.complete_next());
        assert(next.get() == finite_finish_result::success);
    }
    {
        finish_driver driver;
        auto first = std::async(std::launch::async, [&]() { return driver.finite_finish(1000); });
        auto second = std::async(std::launch::async, [&]() { return driver.finite_finish(1000); });
        assert(driver.wait_queued(2));
        assert(driver.complete_next());
        assert(driver.complete_next());
        assert(first.get() == finite_finish_result::success);
        assert(second.get() == finite_finish_result::success);
    }
    {
        finish_driver driver;
        auto pending = std::async(std::launch::async, [&]() { return driver.finite_finish(1000); });
        assert(driver.wait_queued(1));
        driver.abort();
        assert(pending.get() == finite_finish_result::aborted);
        assert(!driver.complete_next());
        assert(driver.finite_finish(1000) == finite_finish_result::aborted);
    }
    {
        request_queue<int> queue;
        queue.max_pending_count_ = 1;
        assert(queue.try_push(1));
        assert(!queue.try_push(2));
        assert(queue.pop() == 1);
        assert(queue.try_push(2));
        queue.abort();
        assert(!queue.try_push(3));
    }
    std::cout << "PASS finite-finish host-only state and queue-admission contracts\n";
}
