// Actual production manager implementation, with explicit host substitutes.
// This checks callback ownership/reentrancy, not the native modal event loop.
#include "DownloadFile.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#define ASSERT(x) assert(x)
#define RELEASE_ASSERT(x) assert(x)
namespace WTF { using std::move; }
static bool isMainThread() { return true; }
static unsigned checks, liveObjects;
static void check(bool value, const char* name) { ++checks; if (!value) { std::printf("FAIL: %s\n",name); std::exit(1); } std::printf("PASS: %s\n",name); }
template<class T> class RefCounted {
public:
    RefCounted() { ++liveObjects; }
    ~RefCounted() { --liveObjects; }
    void ref() const { ++refs; }
    void deref() const { if (!--refs) delete static_cast<const T*>(this); }
private: mutable unsigned refs = 1;
};
template<class T> class Ref {
public:
    explicit Ref(T& value) : p(&value) { p->ref(); }
    Ref(T& value, bool) : p(&value) { }
    Ref(const Ref& r) : p(r.p) { p->ref(); }
    Ref(Ref&& r) : p(std::exchange(r.p,nullptr)) { }
    ~Ref() { if(p)p->deref(); }
    T* operator->() const { return p; }
    T& get() const { return *p; }
private: T* p;
};
template<class T> Ref(T&) -> Ref<T>;
template<class T> Ref<T> adoptRef(T& value) { return Ref<T>(value,true); }
template<class T> class RefPtr {
public:
    RefPtr(std::nullptr_t = nullptr) { }
    RefPtr(T* value) : p(value) { if(p)p->ref(); }
    RefPtr(const Ref<T>& r) : RefPtr(&r.get()) { }
    RefPtr(const RefPtr& r) : RefPtr(r.p) { }
    RefPtr(RefPtr&& r) : p(std::exchange(r.p,nullptr)) { }
    ~RefPtr() { if(p)p->deref(); }
    RefPtr& operator=(RefPtr other) { std::swap(p,other.p); return *this; }
    explicit operator bool() const { return !!p; }
    T* operator->() const { return p; }
    T& operator*() const { return *p; }
    T* get() const { return p; }
    bool operator==(const RefPtr& other) const { return p == other.p; }
private: T* p = nullptr;
};
class String {
public:
    String() = default;
    String(std::span<char16_t> s) : value(s.begin(),s.end()) { }
    String(const char* text) { while(*text)value+=static_cast<unsigned char>(*text++); }
    size_t length() const { return value.size(); }
    char16_t operator[](size_t i) const { return value[i]; }
    bool operator==(const String&) const = default;
    std::u16string value;
};
static String operator""_s(const char* text, size_t) { return String(text); }
class URL { public: const String& string() const { return text; } String text { "https://example.test/file" }; };
template<class T> class Vector { public: void reserveInitialCapacity(size_t n) { v.reserve(n); } void append(T t) { v.push_back(t); } auto span() const { return std::span<const T>(v); } size_t size() const { return v.size(); } private: std::vector<T> v; };
template<class T> using CompletionHandler = std::move_only_function<T>;
namespace WebCore { struct NetworkingContext { }; struct ResourceRequest { }; struct ResourceLoader { }; struct ResourceResponse { }; struct SecurityOrigin : RefCounted<SecurityOrigin> { }; }
namespace WebKitSymbian {
struct Text { const uint16_t* characters = nullptr; int length = 0; };
class TextResult { public: virtual void setText(Text) = 0; protected: ~TextResult() = default; };
struct DownloadResult { DownloadFailure failure = DownloadFailure::None; String description; int errorCode = 0; int cleanupError = 0; };
class SymbianDownload;
#include "download-client-under-test.inc"
class WebViewClient {
public:
    virtual void didStartDownload(uint64_t) { }
    virtual int chooseDownloadDestination(uint64_t, Text, Text, TextResult&) = 0;
    virtual int didCloseDownloadDestinationUI() { return 0; }
    virtual DownloadFile* createDownloadFile() { return nullptr; }
    virtual void didChangeDownloadProgress(uint64_t,uint64_t,int64_t) = 0;
    virtual void didFinishDownload(uint64_t,Text,DownloadFailure,int,int,Text) = 0;
};
class SymbianDownload : public RefCounted<SymbianDownload> {
public:
    static Ref<SymbianDownload> create(SymbianDownloadClient& client, String) { last = new SymbianDownload(client); return adoptRef(*last); }
    explicit SymbianDownload(SymbianDownloadClient& client) : client(&client) { }
    void start(WebCore::NetworkingContext*, const WebCore::ResourceRequest&, RefPtr<WebCore::SecurityOrigin>&&) { ++starts; }
    void convert(WebCore::ResourceLoader&, const WebCore::ResourceResponse&) { ++conversions; }
    void detachClient() { client=nullptr; cancel(); }
    bool finished() const { return terminal; }
    void cancel() { finish(DownloadFailure::Cancelled); }
    void destinationFailed(int error) { lastError=error;finish(DownloadFailure::Destination); }
    int lastError=0;
    void finish(DownloadFailure reason=DownloadFailure::None) { Ref protect(*this); if(terminal)return; terminal=true; if(auto* c=std::exchange(client,nullptr))c->downloadFinished(*this,{}, { reason, {} }); }
    void select() { Ref protect(*this); client->chooseDownloadDestination(*this,URL{},"file.bin"_s,[this](String&& result) { chosen=std::move(result); ++selections; }); }
    void progress(uint64_t bytes, std::optional<uint64_t> total) { Ref protect(*this); if(client)client->downloadProgress(*this,bytes,total); }
    inline static SymbianDownload* last = nullptr;
    inline static unsigned starts=0,conversions=0;
    String chosen; unsigned selections=0;
private: SymbianDownloadClient* client; bool terminal=false;
};
#include "manager-header-under-test.inc"
#include "manager-body-under-test.inc"
}
using namespace WebKitSymbian;
struct Client : WebViewClient {
    std::function<int(uint64_t,TextResult&)> choose;
    std::function<void(uint64_t)> onStart;
    std::function<void()> onFinish, onProgress;
    std::function<int()> onRestore;
    unsigned restoreCalls = 0;
    std::vector<std::pair<uint64_t,DownloadFailure>> finished;
    uint64_t selectedID=0,progressID=0,bytes=0; int64_t total=0;
    void didStartDownload(uint64_t id) override { if(onStart)onStart(id); }
    int chooseDownloadDestination(uint64_t id,Text,Text,TextResult& result) override { selectedID=id; return choose ? choose(id,result) : 1; }
    int didCloseDownloadDestinationUI() override { ++restoreCalls; return onRestore ? onRestore() : 0; }
    void didChangeDownloadProgress(uint64_t id,uint64_t value,int64_t length) override { progressID=id;bytes=value;total=length;if(onProgress)onProgress(); }
    void didFinishDownload(uint64_t id,Text,DownloadFailure failure,int,int,Text) override { finished.emplace_back(id,failure);if(onFinish)onFinish(); }
};
static void start(SymbianDownloadManager& manager) { manager.start(nullptr,{},nullptr,"file.bin"_s); }
int main()
{
    {
        Client client; auto manager=SymbianDownloadManager::create(client); start(manager.get());
        RefPtr<SymbianDownload> first(SymbianDownload::last);
        check(manager->hasActiveDownload() && SymbianDownload::starts==1,"one request starts and remains owned");
        start(manager.get());
        check(manager->hasActiveDownload() && SymbianDownload::starts==1 && client.finished.size()==1 && client.finished[0].second==DownloadFailure::Busy,"second request is rejected without replacing or repeating the first");
        const uint16_t path[]={'C',':','\\','a'};
        client.choose=[&](uint64_t,TextResult& result) { result.setText({path,4});return 0; };
        first->select(); check(first->chosen=="C:\\a"_s && first->selections==1,"accepted native result is copied exactly once");
        check(client.selectedID!=client.finished[0].first,"busy terminal ID cannot be mistaken for the active transfer");
        first->progress(25,100);check(client.progressID==client.selectedID && client.bytes==25 && client.total==100,"progress reports matching ID and known length");
        first->progress(30,std::nullopt);check(client.total==-1,"unknown length remains explicitly unknown");
        manager->cancel(client.selectedID+100);check(!first->finished(),"stale cancel ID cannot stop a different download");
        manager->cancel(client.selectedID);check(first->finished()&&!manager->hasActiveDownload()&&client.finished.size()==2&&client.finished.back().second==DownloadFailure::Cancelled,"cancel emits one terminal event and removes active ownership");
        manager->cancel(client.selectedID);check(client.finished.size()==2,"repeat cancel cannot emit another terminal event");
        manager->convert(nullptr,{});check(client.finished.back().second==DownloadFailure::Unsupported && !manager->hasActiveDownload() && !SymbianDownload::conversions,"missing live response is an explicit failure without repeating network load");
    }
    check(!liveObjects,"first scenario releases all controller and manager references");
    {
        Client client; RefPtr<SymbianDownloadManager> owner(SymbianDownloadManager::create(client)); start(*owner);
        RefPtr<SymbianDownload> download(SymbianDownload::last);
        client.choose=[&](uint64_t,TextResult& result) { owner->detach();owner=nullptr;check(liveObjects==2,"manager remains alive while closing its view inside native callback");const uint16_t path[]={'C',':','\\','x'};result.setText({path,4});return 0; };
        download->select();
        check(download->finished() && download->chosen.length()==0 && download->selections==1 && client.finished.empty(),"post-detach save result is discarded and no terminal UI callback escapes");
        check(liveObjects==1,"manager is released after the nested callback returns");
        download->progress(99,100);check(!client.progressID,"detached controller cannot report later progress");
    }
    check(!liveObjects,"nested close leaves no ownership cycle");
    {
        Client client; RefPtr<SymbianDownloadManager> owner(SymbianDownloadManager::create(client));start(*owner);
        RefPtr<SymbianDownload> download(SymbianDownload::last);
        client.choose=[&](uint64_t id,TextResult&) { owner->cancel(id);return 0; };
        download->select();check(download->chosen.length()==0 && client.finished.size()==1 && !owner->hasActiveDownload(),"cancel inside save callback cannot resume the cancelled transfer");
        start(*owner);RefPtr<SymbianDownload> next(SymbianDownload::last);
        client.onProgress=[&] { owner->detach();owner=nullptr; };
        next->progress(1,2);check(next->finished() && liveObjects==2,"progress can close the view without a callback use-after-free");
    }
    check(!liveObjects,"progress-close scenario releases both cancelled controllers");
    {
        Client client;auto manager=SymbianDownloadManager::create(client);start(manager.get());RefPtr<SymbianDownload> download(SymbianDownload::last);
        std::vector<uint16_t> huge(257,'x');
        client.choose=[&](uint64_t,TextResult& result) { result.setText({huge.data(),257});return 0; };
        download->select();check(download->chosen.length()==0,"oversized native path is rejected before allocation/copy");
        manager->detach();start(manager.get());check(!manager->hasActiveDownload() && client.finished.empty(),"detached manager cannot start another request or call the old client");
    }
    check(!liveObjects,"all earlier host substitutes are released");
    {
        Client client;auto manager=SymbianDownloadManager::create(client);
        auto starts=SymbianDownload::starts;
        client.onStart=[&](uint64_t id) { manager->cancel(id); };
        start(manager.get());
        check(!manager->hasActiveDownload() && SymbianDownload::starts==starts && client.finished.size()==1,"cancel from the initial progress event prevents even starting the network request");
    }
    check(!liveObjects,"pre-network cancellation releases the controller");
    {
        Client client;auto manager=SymbianDownloadManager::create(client);start(manager.get());RefPtr<SymbianDownload> download(SymbianDownload::last);
        client.choose=[](uint64_t,TextResult&) { return -26; };
        download->select();check(download->lastError==-26 && download->chosen.length()==0 && client.finished.size()==1 && client.finished.back().second==DownloadFailure::Destination,"native save error stays a destination failure, not user cancellation");
    }
    check(!liveObjects,"all host substitutes are released");
    {
        Client client;auto manager=SymbianDownloadManager::create(client);start(manager.get());RefPtr<SymbianDownload> download(SymbianDownload::last);
        const uint16_t path[]={'C',':','\\','r'};
        client.choose=[&](uint64_t,TextResult& result) { result.setText({path,4});return 0; };
        download->select();
        check(client.restoreCalls==1 && download->chosen=="C:\\r"_s && download->selections==1,"native controls restore once before an accepted path resumes");
    }
    check(!liveObjects,"accepted restore scenario releases all references");
    {
        Client client;RefPtr<SymbianDownloadManager> owner(SymbianDownloadManager::create(client));start(*owner);RefPtr<SymbianDownload> download(SymbianDownload::last);
        const uint16_t path[]={'C',':','\\','x'};
        client.choose=[&](uint64_t,TextResult& result) { result.setText({path,4});return 0; };
        client.onRestore=[&] { owner->detach();owner=nullptr;check(liveObjects==2,"manager is retained while restore callback closes the view");return 0; };
        download->select();
        check(client.restoreCalls==1 && download->finished() && download->chosen.length()==0 && download->selections==1 && client.finished.empty(),"restore-detach discards path and cannot dispatch terminal UI into the detached client");
        check(liveObjects==1,"restore-close releases manager after callback return");
    }
    check(!liveObjects,"restore-close scenario leaves no ownership cycle");
    {
        Client client;auto manager=SymbianDownloadManager::create(client);start(manager.get());RefPtr<SymbianDownload> download(SymbianDownload::last);
        const uint16_t path[]={'C',':','\\','x'};
        client.choose=[&](uint64_t,TextResult& result) { result.setText({path,4});return 0; };
        client.onRestore=[] { return -4; };
        download->select();
        check(client.restoreCalls==1 && download->lastError==-4 && download->chosen.length()==0 && download->selections==1 && client.finished.size()==1 && client.finished.back().second==DownloadFailure::Destination,"restore error rejects an accepted target as destination failure");
    }
    check(!liveObjects,"restore-error scenario releases all references");
    {
        Client client;auto manager=SymbianDownloadManager::create(client);start(manager.get());RefPtr<SymbianDownload> download(SymbianDownload::last);
        client.choose=[](uint64_t,TextResult&) { return 1; };client.onRestore=[] { return -4; };
        download->select();
        check(client.restoreCalls==1 && !download->lastError && download->chosen.length()==0 && download->selections==1 && client.finished.empty(),"genuine native Cancel keeps empty continuation instead of being replaced by restore error");
    }
    check(!liveObjects,"restore after Cancel releases all references");
    {
        Client client;auto manager=SymbianDownloadManager::create(client);start(manager.get());RefPtr<SymbianDownload> download(SymbianDownload::last);
        client.choose=[](uint64_t,TextResult&) { return -26; };client.onRestore=[] { return -4; };
        download->select();
        check(client.restoreCalls==1 && download->lastError==-26 && download->chosen.length()==0 && download->selections==1 && client.finished.size()==1 && client.finished.back().second==DownloadFailure::Destination,"original save error takes precedence over restore error");
    }
    check(!liveObjects,"save-error restore scenario releases all references");
    std::printf("RESULT checks=%u failures=0 scope=production-manager-with-host-callback-substitutes\n",checks);
}
