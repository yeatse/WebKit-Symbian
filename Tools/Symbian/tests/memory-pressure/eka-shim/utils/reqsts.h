#pragma once
#include <mem/ptr.h>
namespace eka2l1::kernel { class thread; }
namespace eka2l1::common { class chunkyseri; }
namespace eka2l1::epoc {
struct request_status { int status = 123; };
struct notify_info {
    ptr<request_status> sts;
    kernel::thread* requester = nullptr;
    bool is_eka1 = false;
    bool empty() const { return !sts; }
    void pending() { sts.get()->status = -1000; }
    void complete(int);
    void do_state(common::chunkyseri&) { }
};
}
