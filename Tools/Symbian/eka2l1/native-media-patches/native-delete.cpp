// CBase::operator new(ELeave) uses User::AllocZL. These native MMF classes
// must free through the same native heap, including C++14 sized destructors.
// This stays inside the patch DLL and is not part of its frozen export table.
#include <cstddef>
#include <e32std.h>

void operator delete(void* pointer) noexcept { User::Free(pointer); }
void operator delete[](void* pointer) noexcept { User::Free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { User::Free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { User::Free(pointer); }
