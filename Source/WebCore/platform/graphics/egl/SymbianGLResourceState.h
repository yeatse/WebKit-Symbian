/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace WebCore {

// Symbian native contexts do not share objects with each other or with ANGLE.
// This token outlives its context, so pooled resources never dereference a dead
// GLContext or delete a coincidentally equal object name in a different context.
class SymbianGLResourceState final {
public:
    enum class Type : uint8_t { Texture, Framebuffer, Renderbuffer, Buffer, Shader, Program };
    using CurrentFunction = bool (*)(void*);
    using DeleteFunction = void (*)(Type, unsigned);

    SymbianGLResourceState(void* owner, CurrentFunction isCurrent, CurrentFunction makeCurrent, DeleteFunction deleteObject)
        : m_owner(owner)
        , m_isCurrent(isCurrent)
        , m_makeCurrent(makeCurrent)
        , m_deleteObject(deleteObject)
    { }

    bool isCurrent() const
    {
        std::lock_guard lock(m_mutex);
        return m_owner && m_isCurrent(m_owner);
    }

    // Painting and context destruction run on the owning thread. Resource
    // release may run on another thread, which can only queue deletion.
    template<typename Paint, typename Upload> bool paintAndUpload(Paint&& paint, Upload&& upload)
    {
        if (!makeCurrent())
            return false;
        paint();
        // WebGL readback can leave an ANGLE context current, including when
        // restoring the native one failed. Never upload into that context.
        if (!makeCurrent())
            return false;
        return upload();
    }

    void release(Type type, unsigned object)
    {
        if (!object)
            return;
        std::lock_guard lock(m_mutex);
        if (!m_owner)
            return;
        if (m_isCurrent(m_owner))
            m_deleteObject(type, object);
        else
            m_pending.push_back({ type, object });
    }

    void flush()
    {
        std::lock_guard lock(m_mutex);
        if (!m_owner || !m_isCurrent(m_owner))
            return;
        for (auto& object : m_pending)
            m_deleteObject(object.type, object.name);
        m_pending.clear();
    }

    void abandon()
    {
        std::lock_guard lock(m_mutex);
        // eglDestroyContext owns the remaining driver allocations. C++ owners
        // still destruct normally, without GL calls or a retained GLContext.
        m_owner = nullptr;
        m_pending.clear();
    }

private:
    bool makeCurrent()
    {
        void* owner;
        {
            std::lock_guard lock(m_mutex);
            owner = m_owner;
        }
        // Do not hold the lock: makeCurrent flushes this token's deletion queue.
        return owner && m_makeCurrent(owner);
    }

    struct Object { Type type; unsigned name; };
    void* m_owner;
    CurrentFunction m_isCurrent;
    CurrentFunction m_makeCurrent;
    DeleteFunction m_deleteObject;
    mutable std::mutex m_mutex;
    std::vector<Object> m_pending;
};

} // namespace WebCore
