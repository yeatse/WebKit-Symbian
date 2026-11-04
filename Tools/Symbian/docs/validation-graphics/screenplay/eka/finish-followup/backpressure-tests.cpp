int main() {
    // This is a bounded, owned counterexample for the exact production body.
    // A normal scheduler can enter legacy queue.push under kernel+screen locks.
    // An independent watchdog frees queue capacity; it never kills a thread.
    fixture f;
    f.screen.flags_ = epoc::screen::FLAG_SERVER_REDRAW_PENDING;
    eka2l1::request_queue<int> queue;
    queue.max_pending_count_ = 1;
    queue.push(1);
    std::atomic<bool> scheduler_entered{false};
    f.kernel.lock();
    std::thread scheduler([&]() {
        std::lock_guard<std::mutex> kernel_lock(f.kernel.mutex);
        std::lock_guard<std::mutex> screen_lock(f.screen.screen_mutex);
        scheduler_entered = true;
        queue.push(2);
        f.screen.flags_ = 0;
    });
    std::thread watchdog([&]() {
        while (!scheduler_entered) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(2250));
        auto command = queue.pop();
        assert(command && *command == 1);
    });
    auto before = clock_type::now();
    f.server.finish_rendering(f.ctx);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock_type::now() - before).count();
    assert(kernel_held);
    f.kernel.unlock();
    scheduler.join();
    watchdog.join();
    assert(elapsed >= 2200 && elapsed < 4000);
    assert(f.ctx.result == epoc::error_timed_out && f.driver.calls == 0);
    f.clean_pin();
    std::cout << "PASS documented stock backpressure limitation: actual Finish+queue elapsed beyond 2000ms polling budget\n";
}
