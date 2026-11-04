    void window_server::finish_rendering(service::ipc_context &context) {
        auto *kernel = get_kernel_system();
        auto *driver = get_graphics_driver();
        if (!driver || driver->aborted() || kernel->wipeout_in_progress()) {
            context.complete(epoc::error_not_ready);
            return;
        }
        // Keep the request's session and windows alive while the timing thread
        // runs. HLE servers/screens themselves outlive the joined OS thread.
        struct retained_session {
            service::session *session;
            explicit retained_session(service::session *owner) : session(owner) {
                if (session) session->increase_access_count();
            }
            ~retained_session() {
                if (session) session->decrease_access_count();
            }
        } retained(context.msg->msg_session);
        struct unlocked_kernel {
            kernel_system *kernel;
            explicit unlocked_kernel(kernel_system *owner) : kernel(owner) { kernel->unlock(); }
            ~unlocked_kernel() { kernel->lock(); }
        };
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
        auto result = drivers::finite_finish_result::timed_out;
        auto *const first_screen = screens;
        {
            // The normal timing-thread scheduler must be allowed to consume
            // pending WServ drawing before the ordered GPU completion fence.
            // Do not enter its legacy (potentially blocking) submission here.
            unlocked_kernel unlocked(kernel);
            for (;;) {
                if (driver->aborted()) {
                    result = drivers::finite_finish_result::aborted;
                    break;
                }
                bool pending = false;
                for (auto *screen = first_screen; screen; screen = screen->next) {
                    const std::unique_lock<std::mutex> guard(screen->screen_mutex, std::try_to_lock);
                    if (!guard.owns_lock() || (screen->flags_ & (epoc::screen::FLAG_SERVER_REDRAW_PENDING | epoc::screen::FLAG_CLIENT_REDRAW_PENDING))) {
                        pending = true;
                        break;
                    }
                }
                const auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    break;
                }
                if (!pending) {
                    // A successful screen lock also orders this fence after a
                    // scheduler that cleared flags before submitting its list.
                    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
                    if (remaining > 0) {
                        result = driver->finite_finish(static_cast<std::uint32_t>(remaining));
                    }
                    break;
                }
                // No kernel/screen lock is held during either bounded wait.
                std::this_thread::sleep_until(std::min(deadline, now + std::chrono::milliseconds(1)));
            }
        }
        std::int32_t error = epoc::error_not_supported;
        switch (result) {
        case drivers::finite_finish_result::success: error = epoc::error_none; break;
        case drivers::finite_finish_result::timed_out: error = epoc::error_timed_out; break;
        case drivers::finite_finish_result::aborted: error = epoc::error_not_ready; break;
        case drivers::finite_finish_result::not_supported: break;
        }
        // Shutdown joins the emulation thread before driver/server destruction.
        // Avoid signaling a client stopped or completed by another host callback.
        if (context.msg->msg_status != ipc_message_status::completed && context.msg->own_thr
            && context.msg->own_thr->current_state() != kernel::thread_state::stop) {
            context.complete(error);
        }
    }

