// Compile the actual EKA change_notifier.cpp against the minimal request/scheduler
// shim. This tests kernel state/lifetime rules, not guest ABI or real delivery.
#include <kernel/change_notifier.h>
#include <kernel/kernel.h>
#include <kernel/memory_pressure.h>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace eka2l1;
static unsigned checks;
static void check(bool value)
{
    ++checks;
    if (!value) {
        std::cerr << "FAIL check=" << checks << '\n';
        std::exit(1);
    }
}

int main()
{
    using namespace kernel;
    constexpr auto low = memory_low_threshold;
    constexpr auto good = memory_good_threshold;
    check(remaining_guest_ram(256, 0) == 256);
    check(remaining_guest_ram(256, 64) == 192);
    check(remaining_guest_ram(256, 256) == 0);
    check(remaining_guest_ram(256, std::numeric_limits<uint64_t>::max()) == 0);
    check(memory_threshold_changes(low + 1, low) == 0);
    check(memory_threshold_changes(low, low - 1) == 0xA0);
    check(memory_threshold_changes(low - 1, 0) == 0);
    check(memory_threshold_changes(0, good - 1) == 0);
    check(memory_threshold_changes(good - 1, good) == 0x20);
    check(memory_threshold_changes(good, good + 1) == 0);
    check(memory_threshold_changes(good, low - 1) == 0xA0);
    check(memory_threshold_changes(low - 1, good) == 0x20);

    kernel_system kernel;
    change_notifier notifier(&kernel);
    epoc::request_status first, pending, duplicate;
    check(notifier.logon(ptr(&first)));
    check(first.status == 0x17F && !(first.status & 0x80) && kernel.owner.signals == 1);
    check(notifier.logon(ptr(&pending)));
    check(pending.status == -1000);
    check(!notifier.logon(ptr(&duplicate)) && duplicate.status == 123);
    kernel.current = &kernel.other;
    check(!notifier.logon_cancel() && pending.status == -1000);
    kernel.current = &kernel.owner;
    notifier.notify_change_requester(0xA0);
    check(pending.status == 0xA0 && kernel.owner.signals == 2);
    notifier.notify_change_requester(0x20);
    notifier.notify_change_requester(0x80);
    check(notifier.logon(ptr(&pending)));
    check(pending.status == 0xA0 && kernel.owner.signals == 3);
    check(notifier.logon(ptr(&pending)));
    check(notifier.logon_cancel());
    check(pending.status == -3 && kernel.owner.signals == 4);
    check(!notifier.logon_cancel() && kernel.owner.signals == 4);
    // An event after cancellation belongs to the next Logon, never the old status.
    notifier.notify_change_requester(0x20);
    check(pending.status == -3);
    check(notifier.logon(ptr(&pending)) && pending.status == 0x20);
    check(notifier.logon(ptr(&pending)));
    notifier.destroy();
    check(pending.status == -2 && kernel.owner.signals == 6);

    // A stale/exited requester or kernel wipeout must not write its guest status
    // or call into its semaphore. Test each independent lifetime guard.
    for (unsigned guard = 0; guard < 4; ++guard) {
        kernel_system dying;
        change_notifier abandoned(&dying);
        epoc::request_status status;
        check(abandoned.logon(ptr(&status)));
        check(abandoned.logon(ptr(&status)));
        if (guard == 0)
            dying.alive = false;
        else if (guard == 1)
            dying.owner.exit = entity_exit_type::kill;
        else if (guard == 2)
            dying.wiping = true;
        else {
            // Same address reused for a new thread must not inherit the request.
            ++dying.owner.identity;
            check(!abandoned.logon_cancel());
        }
        abandoned.notify_change_requester(0xA0);
        check(status.status == -1000 && dying.owner.signals == 1);
        abandoned.destroy();
        check(status.status == -1000 && dying.owner.signals == 1);
    }
    std::cout << "PASS " << checks << " production notifier/accounting checks\n";
}
