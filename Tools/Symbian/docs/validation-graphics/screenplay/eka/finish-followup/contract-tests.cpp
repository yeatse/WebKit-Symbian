}
using namespace eka2l1;
using clock_type = std::chrono::steady_clock;
struct fixture {
    kernel_system kernel;
    drivers::graphics_driver driver;
    epoc::screen screen;
    service::session session;
    kernel::thread thread;
    service::message msg{&session, ipc_message_status::accepted, &thread};
    service::ipc_context ctx{&msg};
    window_server server{&kernel, &driver, &screen};
    void call() {
        kernel.lock();
        server.finish_rendering(ctx);
        assert(kernel_held);
        kernel.unlock();
    }
    void clean_pin() { assert(session.references == 1 && !session.destroyed && session.pins == 1 && session.unpins == 1); }
};
int main() {
    int cases = 0;
    { fixture f; f.driver.entered = [&]() { assert(f.session.references == 2); assert(f.screen.screen_mutex.try_lock()); f.screen.screen_mutex.unlock(); }; f.call(); assert(f.ctx.result == 0 && f.ctx.completions == 1 && f.driver.calls == 1); f.clean_pin(); ++cases; }
    // A timing-thread analogue cannot enter until Finish releases the kernel.
    // It clears pending flags before its ordered queue submission, still under
    // screen_mutex. The fence must remain behind that submission.
    { fixture f; f.screen.flags_ = epoc::screen::FLAG_SERVER_REDRAW_PENDING; bool submitted = false;
      f.kernel.lock();
      std::thread scheduler([&]() { std::lock_guard<std::mutex> k(f.kernel.mutex); std::lock_guard<std::mutex> s(f.screen.screen_mutex); std::this_thread::sleep_for(std::chrono::milliseconds(20)); f.screen.flags_ = 0; std::this_thread::sleep_for(std::chrono::milliseconds(20)); submitted = true; });
      f.driver.entered = [&]() { assert(submitted); assert(f.session.references == 2); assert(f.screen.screen_mutex.try_lock()); f.screen.screen_mutex.unlock(); };
      f.server.finish_rendering(f.ctx); assert(kernel_held); f.kernel.unlock(); scheduler.join();
      assert(f.ctx.result == 0 && f.driver.calls == 1 && f.driver.timeout < 1980); f.clean_pin(); ++cases; }
    // A contended screen cannot be mistaken for an idle screen.
    { fixture f; std::atomic<bool> held{false}; bool released = false;
      std::thread holder([&]() { std::lock_guard<std::mutex> s(f.screen.screen_mutex); held = true; std::this_thread::sleep_for(std::chrono::milliseconds(25)); released = true; });
      while (!held) { std::this_thread::yield(); } f.driver.entered = [&]() { assert(released); }; f.call(); holder.join(); assert(f.ctx.result == 0 && f.driver.timeout < 1990); f.clean_pin(); ++cases; }
    { fixture f; f.screen.flags_ = epoc::screen::FLAG_CLIENT_REDRAW_PENDING; auto before = clock_type::now(); f.call(); auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock_type::now()-before).count(); assert(ms >= 1990 && ms < 2600 && f.ctx.result == epoc::error_timed_out && f.driver.calls == 0); f.clean_pin(); ++cases; }
    { fixture f; f.driver.is_aborted = true; f.call(); assert(f.ctx.result == epoc::error_not_ready && f.driver.calls == 0 && f.session.pins == 0); ++cases; }
    { fixture f; f.kernel.wiping = true; f.call(); assert(f.ctx.result == epoc::error_not_ready && f.driver.calls == 0 && f.session.pins == 0); ++cases; }
    { fixture f; f.screen.flags_ = epoc::screen::FLAG_CLIENT_REDRAW_PENDING; std::thread aborter([&]() { std::this_thread::sleep_for(std::chrono::milliseconds(25)); f.driver.is_aborted = true; }); auto before=clock_type::now(); f.call(); aborter.join(); assert(clock_type::now()-before < std::chrono::milliseconds(1000) && f.ctx.result == epoc::error_not_ready && f.driver.calls == 0); f.clean_pin(); ++cases; }
    { fixture f; f.driver.response = drivers::finite_finish_result::timed_out; f.call(); assert(f.ctx.result == epoc::error_timed_out); f.clean_pin(); ++cases; }
    { fixture f; f.driver.response = drivers::finite_finish_result::not_supported; f.call(); assert(f.ctx.result == epoc::error_not_supported); f.clean_pin(); ++cases; }
    { fixture f; f.driver.response = drivers::finite_finish_result::aborted; f.call(); assert(f.ctx.result == epoc::error_not_ready); f.clean_pin(); ++cases; }
    { fixture f; f.driver.entered = [&]() { f.thread.state=kernel::thread_state::stop; }; f.call(); assert(f.ctx.completions == 0); f.clean_pin(); ++cases; }
    { fixture f; f.driver.entered = [&]() { f.msg.msg_status=ipc_message_status::completed; }; f.call(); assert(f.ctx.completions == 0); f.clean_pin(); ++cases; }
    // Closing the client's original handle while the kernel is released leaves
    // the explicit session pin alive until the kernel has been restored.
    { fixture f; f.driver.entered = [&]() { std::lock_guard<std::mutex> k(f.kernel.mutex); kernel_held = true; f.session.decrease_access_count(); kernel_held = false; assert(f.session.references==1 && !f.session.destroyed); }; f.call(); assert(f.ctx.result==0 && f.session.references==0 && f.session.destroyed && f.session.pins==1 && f.session.unpins==2); ++cases; }
    { fixture f; f.server.screens = nullptr; f.call(); assert(f.ctx.result == 0 && f.driver.calls == 1); f.clean_pin(); ++cases; }
    { fixture f; f.server.driver = nullptr; f.call(); assert(f.ctx.result == epoc::error_not_ready && f.session.pins==0); ++cases; }
    assert(cases == 15); std::cout << "PASS " << cases << " actual-body Finish scenarios\n";
}
