/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
// Test the production ownership / paint-upload implementation. The callbacks
// replace only EGL make-current and GL deletion, not the lifetime algorithm.
#include "SymbianGLResourceState.h"
#include <cassert>
#include <cstdio>
#include <thread>

using State = WebCore::SymbianGLResourceState;
static thread_local void* currentContext;
static int angleContext;
struct Deletion { void* context; State::Type type; unsigned name; };
static std::vector<Deletion> deletions;

struct Context {
    bool available { true };
    unsigned makeCount { 0 };
    std::shared_ptr<State> state;
    Context()
        : state(std::make_shared<State>(this,
            [](void* owner) { return currentContext == owner; },
            [](void* owner) {
                auto& context = *static_cast<Context*>(owner);
                ++context.makeCount;
                if (!context.available)
                    return false;
                currentContext = owner;
                context.state->flush();
                return true;
            },
            [](State::Type type, unsigned name) { deletions.push_back({ currentContext, type, name }); }))
    { }
    ~Context() { state->abandon(); }
};

int main()
{
    Context first, second;
    currentContext = &angleContext;
    bool painted = false;
    unsigned uploads = 0;
    first.available = false;
    assert(!first.state->paintAndUpload([&] { painted = true; }, [&] { ++uploads; return true; }));
    assert(!painted && !uploads && currentContext == &angleContext);

    first.available = true;
    assert(!first.state->paintAndUpload([&] {
        painted = true;
        currentContext = &angleContext;
        first.available = false; // EGL restoration fails after WebGL readback.
    }, [&] { ++uploads; return true; }));
    assert(painted && !uploads && currentContext == &angleContext);

    first.available = true;
    assert(first.state->paintAndUpload([&] { currentContext = &angleContext; }, [&] {
        assert(currentContext == &first);
        ++uploads;
        return true;
    }));
    assert(uploads == 1);
    assert(!first.state->paintAndUpload([] { }, [] { return false; })); // CPU image allocation / upload failure propagates.

    // Matching numeric names in two unshared contexts are distinct objects.
    currentContext = &second;
    first.state->release(State::Type::Texture, 7);
    second.state->release(State::Type::Texture, 7);
    assert(deletions.size() == 1 && deletions[0].context == &second);
    first.state->flush(); // A timer in another context must not drain this queue.
    assert(deletions.size() == 1);
    currentContext = &angleContext;
    first.state->release(State::Type::Buffer, 8);
    assert(deletions.size() == 1);
    assert(first.state->paintAndUpload([] { }, [] { return true; }));
    assert(deletions.size() == 3);
    assert(deletions[1].context == &first && deletions[1].name == 7);
    assert(deletions[2].context == &first && deletions[2].name == 8);
    first.state->flush(); // Exactly once.
    assert(deletions.size() == 3);

    // Thread-safe refcounted textures may be released off the owning thread.
    std::thread releaser([state = first.state] { state->release(State::Type::Framebuffer, 9); });
    releaser.join();
    assert(deletions.size() == 3);
    first.state->flush();
    assert(deletions.size() == 4 && deletions.back().context == &first && deletions.back().name == 9);
    first.state->release(State::Type::Texture, 0);
    assert(deletions.size() == 4);

    // Failed teardown abandons GPU names to eglDestroyContext. Every C++ object
    // can still be freed, including a pool entry surviving its native context.
    std::shared_ptr<State> pooled;
    std::weak_ptr<State> lifetime;
    {
        Context lost;
        pooled = lost.state;
        lifetime = pooled;
        currentContext = &angleContext;
        for (auto type : { State::Type::Texture, State::Type::Framebuffer, State::Type::Renderbuffer,
                 State::Type::Buffer, State::Type::Shader, State::Type::Program })
            lost.state->release(type, 10);
        lost.available = false;
        lost.state->abandon();
        lost.state->flush();
        assert(!lost.state->paintAndUpload([] { assert(false); }, [] { assert(false); return true; }));
    }
    assert(!lifetime.expired());
    pooled->release(State::Type::Texture, 11);
    pooled->flush();
    assert(deletions.size() == 4);
    pooled.reset();
    assert(lifetime.expired());

    // Reusing a native context address cannot resurrect its old pool token.
    alignas(Context) unsigned char storage[sizeof(Context)];
    auto* old = new (storage) Context;
    auto oldToken = old->state;
    old->~Context();
    auto* replacement = new (storage) Context;
    assert(oldToken != replacement->state);
    currentContext = replacement;
    oldToken->release(State::Type::Texture, 12);
    replacement->state->release(State::Type::Texture, 12);
    assert(deletions.size() == 5 && deletions.back().context == replacement);
    replacement->~Context();

    // Composited page resources can outlive their window context while Ganesh
    // and the EGL sharing root continue rendering.
    {
        Context root, raster;
        raster.state->setSharingState(root.state);
        std::shared_ptr<State> lateOwner;
        currentContext = &angleContext;
        {
            Context compositor;
            compositor.state->setSharingState(root.state);
            lateOwner = compositor.state;
            compositor.state->release(State::Type::Texture, 20);
            compositor.state->release(State::Type::Renderbuffer, 21);
            compositor.state->release(State::Type::Framebuffer, 25);
        }
        std::thread lateRelease([lateOwner] { lateOwner->release(State::Type::Buffer, 22); });
        lateRelease.join();
        lateOwner->release(State::Type::Shader, 23);
        lateOwner->release(State::Type::Program, 24);
        lateOwner->release(State::Type::Texture, 25);
        lateOwner->release(State::Type::Framebuffer, 25);
        root.state->release(State::Type::Framebuffer, 25);
        root.state->release(State::Type::Texture, 26);
        assert(deletions.size() == 5);
        assert(!lateOwner->paintAndUpload([] { assert(false); }, [] { assert(false); return true; }));
        assert(raster.state->paintAndUpload([] { }, [] { return true; }));
        assert(deletions.size() == 12);
        for (size_t i = 5; i < 12; ++i) {
            assert(deletions[i].context == &raster);
            assert(deletions[i].type != State::Type::Framebuffer);
            assert(deletions[i].name == i + 15);
        }
        root.state->flush();
        assert(deletions.size() == 12);
        currentContext = &root;
        root.state->flush();
        assert(deletions.size() == 13);
        assert(deletions.back().context == &root && deletions.back().type == State::Type::Framebuffer);
        raster.state->flush();
        assert(deletions.size() == 13);
    }
    std::puts("PASS: context restoration, deferred deletion, shared resource lifetime, framebuffer ownership, thread release and context reuse");
}
