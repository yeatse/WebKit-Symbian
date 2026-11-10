// Compile the actual ALF session implementation; fake only kernel/IPC boundaries.
#include <services/audio/alf/alf.h>
#include <utils/err.h>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace eka2l1;
constexpr std::uint32_t plugin_uid = 0x2001e2cf;
static int passed;
static void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAIL: " << name << '\n';
        std::exit(1);
    }
    ++passed;
    std::cout << "PASS: " << name << '\n';
}
static std::string little(std::uint64_t value, unsigned size) {
    std::string result;
    for (unsigned i = 0; i < size; ++i)
        result.push_back(static_cast<char>(value >> (8 * i)));
    return result;
}
struct fixture {
    eka2l1::system sys;
    kernel::thread first{0x100000005ULL}, second{0x200000005ULL};
    alf_streamer_server server{&sys};
    std::unique_ptr<alf_streamer_session> session{std::make_unique<alf_streamer_session>(&server, 1, epoc::version{})};
    fixture() { sys.kernel.alive = {&first, &second}; }
    service::ipc_context context(int function, kernel::thread* owner = nullptr, std::uint32_t plugin = plugin_uid) {
        service::ipc_context ctx;
        ctx.sys = &sys;
        ctx.msg = std::make_shared<service::ipc_msg>();
        ctx.msg->function = function;
        ctx.msg->own_thr = owner ? owner : &first;
        ctx.plugin = plugin;
        return ctx;
    }
    service::ipc_context subscribe(kernel::thread* owner = nullptr, std::uint32_t plugin = plugin_uid, int function = 3) {
        auto ctx = context(function, owner, plugin);
        ctx.input = little(33, 4);
        session->fetch(&ctx);
        return ctx;
    }
    service::ipc_context cancellation(kernel::thread* owner = nullptr, std::uint32_t plugin = plugin_uid, int function = 2) {
        auto ctx = context(function, owner, plugin);
        ctx.input = little(35, 4) + little(ctx.msg->own_thr->id, 8);
        return ctx;
    }
    void send(service::ipc_context& ctx) { session->fetch(&ctx); }
};
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--exit-contract-only") {
        fixture f;
        auto pending = f.subscribe();
        auto cancel = f.cancellation();
        f.send(cancel);
        check(cancel.result->code == 0 && pending.result->count == 1 && pending.result->code == epoc::error_cancel,
            "official CancelPolicyUpdates completes original status before CActive::Cancel");
        std::cout << "RESULT: passed=" << passed << " failed=0\n";
        return 0;
    }
    {
        fixture f;
        auto pending = f.subscribe();
        check(!pending.result->count, "official request 33 remains pending");
        auto wrong_plugin = f.cancellation(nullptr, plugin_uid + 1);
        f.send(wrong_plugin);
        check(wrong_plugin.result->code == 0 && !pending.result->count, "wrong plugin cannot cancel subscription");
        auto forged = f.cancellation(&f.second);
        forged.input = little(35, 4) + little(f.first.id, 8);
        f.send(forged);
        check(forged.result->code == epoc::error_permission_denied && !pending.result->count, "payload cannot impersonate requester thread");
        auto other_thread = f.cancellation(&f.second);
        f.send(other_thread);
        check(other_thread.result->code == 0 && !pending.result->count, "same low 32 bits do not match different 64 bit thread IDs");
        auto duplicate = f.subscribe();
        check(duplicate.result->code == epoc::error_in_use && !pending.result->count, "duplicate cannot replace pending request");
        for (auto length : {4U, 11U, 13U}) {
            auto malformed = f.cancellation();
            malformed.input->resize(length);
            f.send(malformed);
            check(malformed.result->code == epoc::error_argument && !pending.result->count, "cancel requires exact 12 byte stream");
        }
        auto small = f.cancellation();
        small.output_capacity = 0;
        f.send(small);
        check(small.result->code == epoc::error_bad_descriptor && !pending.result->count, "short output cannot cancel pending request");
        auto unknown = f.cancellation();
        unknown.input = little(99, 4);
        f.send(unknown);
        check(unknown.result->code == epoc::error_not_supported && !pending.result->count, "unknown sync operation does not cancel or fake success");
        auto cancel = f.cancellation();
        bool old_before_sync = false;
        pending.result->on_complete = [&] {
            old_before_sync = !cancel.result->count;
            auto nested = f.cancellation();
            f.send(nested);
        };
        f.send(cancel);
        check(old_before_sync && pending.result->count == 1 && pending.result->code == epoc::error_cancel && cancel.result->code == 0, "cancel clears pending before completion and replies after original status");
        pending.result->on_complete = {};
        f.send(cancel);
        check(pending.result->count == 1, "repeated cancel never completes original twice");
        auto again = f.subscribe();
        check(!again.result->count, "new subscription accepted after cancellation");
    }
    {
        fixture f;
        auto a = f.subscribe();
        auto b = f.subscribe(&f.second);
        auto c = f.subscribe(nullptr, plugin_uid + 1);
        auto unload = f.context(1);
        f.send(unload);
        check(a.result->code == epoc::error_cancel && b.result->code == epoc::error_cancel && !c.result->count, "unload cancels only matching plugin across its threads");
        f.session.reset();
        check(c.result->code == epoc::error_cancel && a.result->count == 1 && b.result->count == 1, "close cancels remaining request without duplicates");
    }
    {
        fixture f;
        auto a = f.subscribe();
        auto b = f.subscribe(&f.second);
        auto cancel = f.context(4);
        cancel.command = 99;
        f.send(cancel);
        check(cancel.result->code == epoc::error_not_supported && !a.result->count, "unknown explicit cancellation command is rejected");
        cancel.command = 33;
        f.send(cancel);
        check(cancel.result->code == epoc::error_not_supported && !a.result->count && !b.result->count, "outer message ID 33 is not misinterpreted as policy opcode");
    }
    for (int mode = 0; mode < 5; ++mode) {
        fixture f;
        auto pending = f.subscribe();
        if (mode == 0) f.sys.kernel.alive.erase(&f.first);
        if (mode == 1) f.first.id += 1;
        if (mode == 2) f.first.exit = kernel::entity_exit_type::kill;
        if (mode == 3) f.sys.kernel.wiping = true;
        if (mode == 4) pending.msg->request_sts.mapped = false;
        f.session.reset();
        const char* names[] = {"dead requester not completed", "reused thread address with new UID not completed", "exited requester not completed", "kernel wipe does not signal guest", "unmapped status not dereferenced"};
        check(!pending.result->count, names[mode]);
    }
    {
        fixture f;
        auto p = f.subscribe(nullptr, plugin_uid, 20003);
        auto c = f.cancellation(nullptr, plugin_uid, 20002);
        f.send(c);
        check(p.result->code == epoc::error_cancel && c.result->code == 0, "legacy outer opcode offset uses same protocol");
        auto unknown = f.context(999);
        f.send(unknown);
        check(unknown.result->code == epoc::error_not_supported, "unknown outer operation rejected");
        auto no_input = f.context(3);
        f.send(no_input);
        check(no_input.result->code == epoc::error_argument, "invalid async input rejected");
        auto unknown_async = f.context(3);
        unknown_async.input = little(34, 4);
        f.send(unknown_async);
        check(unknown_async.result->code == epoc::error_not_supported, "unsupported async command not retained");
        auto malformed_async = f.context(3);
        malformed_async.input = little(33, 8);
        f.send(malformed_async);
        check(malformed_async.result->code == epoc::error_argument, "subscription must have exact official input length");
        auto query = f.context(2);
        query.input = little(34, 4);
        f.send(query);
        check(query.result->code == 0 && query.result->output == 0, "disabled effects known synchronous response retained");
        auto write_error = f.context(2);
        write_error.input = little(34, 4);
        write_error.output_writable = false;
        f.send(write_error);
        check(write_error.result->code == epoc::error_bad_descriptor, "descriptor write failure not reported as success");
    }
    std::cout << "RESULT: passed=" << passed << " failed=0\n";
}
