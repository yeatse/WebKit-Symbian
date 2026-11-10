#pragma once
#include <string>
namespace eka2l1 { class kernel_system; namespace common { class chunkyseri; } }
namespace eka2l1::kernel {
enum class object_type { change_notifier };
enum class access_type { local_access };
enum class entity_exit_type { pending, kill };
class kernel_obj {
protected:
    kernel_system* kern;
    object_type obj_type;
public:
    kernel_obj(kernel_system* k, const std::string&, void*, access_type) : kern(k) { }
    virtual ~kernel_obj() = default;
    virtual int destroy() { return 0; }
    virtual void do_state(common::chunkyseri&) { }
};
}
