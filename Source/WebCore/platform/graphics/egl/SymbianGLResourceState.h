/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace WebCore {

// Framebuffers belong to their creating context. Other GL objects can be
// released through the sharing group after their creating context closes.
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

    void setSharingState(std::shared_ptr<SymbianGLResourceState> state)
    {
        std::lock_guard lock(m_mutex);
        m_sharingState = std::move(state);
    }

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
        std::shared_ptr<SymbianGLResourceState> sharing;
        {
            std::lock_guard lock(m_mutex);
            if (m_owner) {
                if (m_isCurrent(m_owner))
                    m_deleteObject(type, object);
                else
                    m_pending.push_back({ type, object });
                return;
            }
            if (isShareable(type))
                sharing = m_sharingState;
        }
        if (sharing)
            sharing->release(type, object);
    }

    void flush()
    {
        std::shared_ptr<SymbianGLResourceState> sharing;
        {
            std::lock_guard lock(m_mutex);
            if (!m_owner || !m_isCurrent(m_owner))
                return;
            for (auto& object : m_pending)
                m_deleteObject(object.type, object.name);
            m_pending.clear();
            sharing = m_sharingState;
        }
        if (sharing)
            sharing->flushShared();
    }

    void abandon()
    {
        std::vector<Object> pending;
        std::shared_ptr<SymbianGLResourceState> sharing;
        {
            std::lock_guard lock(m_mutex);
            m_owner = nullptr;
            pending.swap(m_pending);
            sharing = m_sharingState;
        }
        if (sharing) {
            for (auto& object : pending) {
                if (isShareable(object.type))
                    sharing->release(object.type, object.name);
            }
        }
    }

private:
    static bool isShareable(Type type) { return type != Type::Framebuffer; }

    void flushShared()
    {
        std::shared_ptr<SymbianGLResourceState> sharing;
        {
            std::lock_guard lock(m_mutex);
            if (!m_owner)
                return;
            std::erase_if(m_pending, [&](const Object& object) {
                if (!isShareable(object.type))
                    return false;
                m_deleteObject(object.type, object.name);
                return true;
            });
            sharing = m_sharingState;
        }
        if (sharing)
            sharing->flushShared();
    }

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
    std::shared_ptr<SymbianGLResourceState> m_sharingState;
    mutable std::mutex m_mutex;
    std::vector<Object> m_pending;
};

} // namespace WebCore
