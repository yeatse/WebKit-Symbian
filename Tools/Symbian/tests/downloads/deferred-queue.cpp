// Executes the production ResourceHandle queue, cancellation and download-data
// methods extracted by run-deferred-queue.py. Lightweight type substitutes do
// not claim to test WebCore refcounting, the scheduler, TLS or the native UI.
#include "CurlDownloadBuffer.h"
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <utility>

namespace WTF { using std::move; }
#define WTF_DEPRECATED_MAKE_FAST_ALLOCATED(...)
template<class T> using CompletionHandler = std::move_only_function<T>;
#include "completion-scope-under-test.inc"
template<class T> using Function = std::move_only_function<T>;
template<class T> class Ref {
public:
    explicit Ref(T& t) : value(&t, [](T*) { }) { }
    explicit Ref(std::shared_ptr<T> t) : value(std::move(t)) { }
    operator T&() const { return *value; }
    T* operator->() const { return value.get(); }
    T& get() const { return *value; }
private:
    std::shared_ptr<T> value;
};
template<class T> Ref(T&) -> Ref<T>;
template<class T> class Deque {
public:
    void append(T&& t) { values.push_back(std::move(t)); }
    bool isEmpty() const { return values.empty(); }
    T takeFirst() { auto first = std::move(values.front()); values.pop_front(); return first; }
    void clear() { values.clear(); }
    size_t size() const { return values.size(); }
private:
    std::deque<T> values;
};
static unsigned checks;
static void check(bool value, const char* name)
{
    ++checks;
    if (!value) { std::printf("FAIL: %s\n", name); std::exit(1); }
    std::printf("PASS: %s\n", name);
}
static size_t retainedBytes;
struct SharedBuffer {
    explicit SharedBuffer(size_t bytes) : length(bytes) { retainedBytes += bytes; }
    ~SharedBuffer() { retainedBytes -= length; }
    size_t size() const { return length; }
    size_t length;
};
struct CurlRequest { void cancel() { cancelled = true; } bool cancelled = false; };
class ResourceHandle;
struct Client {
    Function<void(ResourceHandle&, const SharedBuffer&)> sink;
    void didReceiveBuffer(ResourceHandle* handle, const SharedBuffer& buffer, int) { sink(*handle, buffer); }
};
struct Timer { void stop() { } };
struct MessageQueue { void kill() { } };
class ResourceHandle {
public:
    enum { NoFailure };
    struct Data {
        bool m_cancelled = false, m_drainingCallbacks = false, m_defersLoading = false;
        Timer m_failureTimer;
        int m_scheduledFailureType = NoFailure;
        std::shared_ptr<CurlRequest> m_curlRequest = std::make_shared<CurlRequest>();
        Deque<Function<void(ResourceHandle&)>> m_deferredCallbacks;
        MessageQueue* m_messageQueue = nullptr;
    };
    std::unique_ptr<Data> d = std::make_unique<Data>();
    Client* receiver = nullptr;
    Client* client() const { return receiver; }
    void clearAuthentication() { }
    void cancel();
    bool acceptsCurlCallback(const CurlRequest&) const;
    void dispatchCurlCallback(Function<void(ResourceHandle&)>&&);
    void drainCurlCallbacks();
    void curlDidReceiveDownloadData(CurlRequest&, Ref<SharedBuffer>&&, CompletionHandler<void()>&&);
};
#include "resource-handle-queue-under-test.inc"

struct Test {
    WebCore::CurlDownloadBuffer budget;
    ResourceHandle handle;
    Client client;
    unsigned acknowledgements = 0, deliveries = 0;
    Test()
    {
        budget.setLimit(256 * 1024);
        handle.receiver = &client;
        client.sink = [this](ResourceHandle&, const SharedBuffer&) { ++deliveries; };
    }
    void deliver(size_t size = 16 * 1024)
    {
        check(budget.reserve(size) == WebCore::CurlDownloadBuffer::Reservation::Accepted, "actual queue input reserves a complete buffer");
        Ref<SharedBuffer> buffer(std::make_shared<SharedBuffer>(size));
        handle.curlDidReceiveDownloadData(*handle.d->m_curlRequest, std::move(buffer), [this, size] {
            // This checks destruction ordering too, not just eventual counts.
            check(retainedBytes <= budget.pending() - size, "ack follows release of its retained buffer");
            check(budget.release(size), "ack returns exactly its reservation");
            ++acknowledgements;
        });
    }
};

int main()
{
    {
        Test test; test.handle.d->m_defersLoading = true;
        for (unsigned i = 0; i < 16; ++i) test.deliver();
        check(!test.deliveries && !test.acknowledgements && test.budget.pending() == 256 * 1024 && retainedBytes == 256 * 1024,
            "second-level deferred queue retains the complete byte budget");
        check(test.budget.reserve(1) == WebCore::CurlDownloadBuffer::Reservation::Paused, "second-level buffering cannot bypass the curl high-water mark");
        test.handle.d->m_defersLoading = false; test.handle.drainCurlCallbacks();
        check(test.deliveries == 16 && test.acknowledgements == 16 && !test.budget.pending() && !retainedBytes,
            "resume delivers and releases every deferred buffer once");
    }
    {
        Test test; bool nested = false;
        test.client.sink = [&](ResourceHandle&, const SharedBuffer&) {
            ++test.deliveries;
            if (nested) return;
            nested = true; test.deliver();
            check(test.deliveries == 1 && !test.acknowledgements && test.budget.pending() == 32 * 1024 && retainedBytes == 32 * 1024,
                "nested main-loop delivery remains charged until the outer sink returns");
        };
        test.deliver();
        check(test.deliveries == 2 && test.acknowledgements == 2 && !test.budget.pending(), "nested delivery drains in FIFO order after the outer callback");
    }
    {
        Test test; test.handle.d->m_defersLoading = true;
        for (unsigned i = 0; i < 3; ++i) test.deliver();
        test.handle.cancel();
        check(!test.deliveries && test.acknowledgements == 3 && !test.budget.pending() && !retainedBytes,
            "actual cancel clears queued buffers and returns all acknowledgements");
        test.handle.cancel();
        check(test.acknowledgements == 3, "repeated cancel cannot double-release bytes");
    }
    {
        Test test; test.handle.d->m_defersLoading = true;
        for (unsigned i = 0; i < 3; ++i) test.deliver();
        test.client.sink = [&](ResourceHandle& handle, const SharedBuffer&) {
            ++test.deliveries; handle.cancel();
            check(test.acknowledgements == 2 && test.budget.pending() == 16 * 1024 && retainedBytes == 16 * 1024,
                "cancel inside a sink retains the current buffer until that sink returns");
        };
        test.handle.d->m_defersLoading = false; test.handle.drainCurlCallbacks();
        check(test.deliveries == 1 && test.acknowledgements == 3 && !test.budget.pending(), "in-callback cancellation never delivers discarded later chunks");
    }
    {
        Test test; test.handle.d->m_defersLoading = true; test.deliver();
        auto keepOldRequestAlive = test.handle.d->m_curlRequest;
        test.handle.d->m_curlRequest = std::make_shared<CurlRequest>();
        test.handle.d->m_defersLoading = false; test.handle.drainCurlCallbacks();
        check(!test.deliveries && test.acknowledgements == 1 && !test.budget.pending(), "stale redirect-generation data is discarded with acknowledgement");
    }
    {
        Test test; test.handle.receiver = nullptr; test.deliver();
        check(!test.deliveries && test.acknowledgements == 1 && !test.budget.pending(), "detached client rejects a new callback without retaining its buffer");
    }
    check(!retainedBytes, "all data buffers are released at test completion");
    std::printf("RESULT checks=%u failures=0 scope=production-resourcehandle-queue-bodies\n", checks);
}
