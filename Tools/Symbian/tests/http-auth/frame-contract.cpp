#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <WebKitSymbian.h>
using namespace WebKitSymbian;
using UChar = char16_t;
struct String : std::u16string {
    using std::u16string::u16string;
    String(std::span<const UChar> s) : std::u16string(s.data(), s.size()) { }
};
String emptyString() { return {}; }
struct UTF16Text {
    String value;
    UTF16Text(const String& s) : value(s) { }
    Text text() const { return {reinterpret_cast<const uint16_t*>(value.data()), static_cast<int>(value.size())}; }
};
template<class T> struct Ref {
    std::shared_ptr<T> value;
    Ref(T& v) : value(v.shared_from_this()) { }
    T* operator->() const { return value.get(); }
    T* ptr() const { return value.get(); }
};
template<class T> Ref(T&) -> Ref<T>;
template<class T> struct RefPtr {
    std::shared_ptr<T> value;
    RefPtr(T* v) : value(v ? v->shared_from_this() : nullptr) { }
    T* operator->() const { return value.get(); }
    explicit operator bool() const { return !!value; }
};
template<class T> RefPtr(T*) -> RefPtr<T>;
struct ResourceLoaderIdentifier {
    uint64_t value;
    uint64_t toUInt64() const { return value; }
    bool operator==(const ResourceLoaderIdentifier&) const = default;
};
struct URL { bool http = true; bool protocolIsInHTTPFamily() const { return http; } };
struct SecurityOrigin {
    static std::shared_ptr<SecurityOrigin> create(const URL&) { return std::make_shared<SecurityOrigin>(); }
    String toString() const { return u"https://example.test:8443"; }
};
struct ProtectionSpace {
    enum class AuthenticationScheme { HTTPBasic, HTTPDigest, ServerTrustEvaluationRequested, Unknown };
    bool proxy = false;
    AuthenticationScheme scheme = AuthenticationScheme::HTTPBasic;
    bool isProxy() const { return proxy; }
    AuthenticationScheme authenticationScheme() const { return scheme; }
};
struct ResourceResponse {
    int status = 401;
    URL location;
    int httpStatusCode() const { return status; }
    const URL& url() const { return location; }
};
struct ResourceError {};
enum class CredentialPersistence { None, ForSession };
struct Credential { String user, pass; CredentialPersistence persistence; };
struct AuthenticationChallenge;
struct AuthenticationClient : std::enable_shared_from_this<AuthenticationClient> {
    int continued = 0, cancelled = 0, supplied = 0;
    std::optional<Credential> received;
    void receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&) { ++continued; }
    void receivedCancellation(const AuthenticationChallenge&) { ++cancelled; }
    void receivedCredential(const AuthenticationChallenge&, const Credential& c) { ++supplied; received = c; }
};
struct AuthenticationChallenge {
    std::shared_ptr<AuthenticationClient> client = std::make_shared<AuthenticationClient>();
    ProtectionSpace space;
    ResourceResponse response;
    unsigned failures = 0;
    AuthenticationClient* authenticationClient() const { return client.get(); }
    const ProtectionSpace& protectionSpace() const { return space; }
    const ResourceResponse& failureResponse() const { return response; }
    unsigned previousFailureCount() const { return failures; }
};
struct LocalFrame;
struct DocumentLoader : std::enable_shared_from_this<DocumentLoader> {
    LocalFrame* owner = nullptr;
    LocalFrame* frame() const { return owner; }
};
struct FrameLoader {
    DocumentLoader* current = nullptr;
    DocumentLoader* provisional = nullptr;
    DocumentLoader* documentLoader() const { return current; }
    DocumentLoader* provisionalDocumentLoader() const { return provisional; }
};
struct LocalFrame : std::enable_shared_from_this<LocalFrame> {
    bool attached = true;
    FrameLoader load;
    void* page() const { return attached ? (void*)this : nullptr; }
    FrameLoader& loader() { return load; }
};
struct NativeClient {
    int prompts = 0;
    std::vector<uint64_t> dismissed;
    std::function<bool(TextResult&, TextResult&)> run;
    bool runAuthenticationChallenge(uint64_t id, Text origin, unsigned attempt, TextResult& user, TextResult& pass)
    {
        assert(id && attempt >= 1 && attempt <= 3);
        assert(std::u16string(reinterpret_cast<const UChar*>(origin.characters), origin.length) == u"https://example.test:8443");
        ++prompts;
        return run(user, pass);
    }
    void cancelAuthenticationChallenge(uint64_t id) { dismissed.push_back(id); }
};
struct WebViewImpl { NativeClient native; NativeClient& client() { return native; } };
struct SymbianFrameLoaderClient {
    WebViewImpl& m_view;
    LocalFrame* m_frame;
    std::optional<ResourceLoaderIdentifier> m_authenticationIdentifier;
    void cancelAuthenticationChallenge();
    void dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&);
    void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&);
};
#include "production-frame.inc"

static bool accept(TextResult& user, TextResult& pass)
{
    static const char16_t u[] = u"alice", p[] = u"secret";
    user.setText({reinterpret_cast<const uint16_t*>(u), 5});
    pass.setText({reinterpret_cast<const uint16_t*>(p), 6});
    return true;
}
struct Fixture {
    WebViewImpl view;
    std::shared_ptr<LocalFrame> frame = std::make_shared<LocalFrame>();
    std::shared_ptr<DocumentLoader> loader = std::make_shared<DocumentLoader>();
    SymbianFrameLoaderClient client { view, frame.get(), {} };
    AuthenticationChallenge challenge;
    Fixture() { loader->owner = frame.get(); frame->load.current = loader.get(); view.native.run = accept; }
    void run() { client.dispatchDidReceiveAuthenticationChallenge(loader.get(), {10}, challenge); }
};
int main()
{
    for (auto scheme : {ProtectionSpace::AuthenticationScheme::HTTPBasic, ProtectionSpace::AuthenticationScheme::HTTPDigest}) {
        Fixture f; f.challenge.space.scheme = scheme; f.challenge.failures = 2; f.run();
        assert(f.challenge.client->supplied == 1);
        assert(f.challenge.client->received->persistence == CredentialPersistence::None);
        assert(f.challenge.client->received->user == u"alice");
    }
    for (int mode = 0; mode < 6; ++mode) {
        Fixture f;
        if (mode == 0) f.challenge.space.proxy = true;
        if (mode == 1) f.challenge.response.status = 407;
        if (mode == 2) f.challenge.space.scheme = ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested;
        if (mode == 3) f.challenge.space.scheme = ProtectionSpace::AuthenticationScheme::Unknown;
        if (mode == 4) f.challenge.failures = 3;
        if (mode == 5) f.challenge.response.location.http = false;
        f.run(); assert(!f.view.native.prompts && f.challenge.client->continued == 1 && !f.challenge.client->supplied);
    }
    for (int mode = 0; mode < 6; ++mode) {
        Fixture f;
        f.view.native.run = [&](TextResult& user, TextResult& pass) {
            accept(user, pass);
            if (mode == 0) return false;
            if (mode == 1) f.client.cancelAuthenticationChallenge();
            if (mode == 2) f.client.dispatchDidFailLoading(f.loader.get(), {10}, {});
            if (mode == 3) f.frame->attached = false;
            if (mode == 4) f.loader->owner = nullptr;
            if (mode == 5) f.frame->load.current = nullptr;
            return true;
        };
        f.run(); assert(f.challenge.client->cancelled == 1 && !f.challenge.client->supplied);
    }
    {
        Fixture f;
        f.view.native.run = [&](TextResult& user, TextResult& pass) {
            f.client.dispatchDidFailLoading(f.loader.get(), {11}, {});
            AuthenticationChallenge nested;
            f.client.dispatchDidReceiveAuthenticationChallenge(f.loader.get(), {11}, nested);
            assert(nested.client->continued == 1);
            return accept(user, pass);
        };
        f.run(); assert(f.challenge.client->supplied == 1 && f.view.native.prompts == 1 && f.view.native.dismissed.empty());
    }
    {
        Fixture f;
        f.view.native.run = [&](TextResult&, TextResult&) { return true; };
        f.run(); assert(f.challenge.client->cancelled == 1);
    }
    {
        Fixture f; auto originalClient = f.challenge.client;
        f.view.native.run = [&](TextResult& user, TextResult& pass) {
            f.challenge.client.reset(); // ResourceHandle::clearAuthentication().
            return accept(user, pass);
        };
        f.run(); assert(originalClient->supplied == 1);
    }
    std::cout << "frame auth: scheme/origin policy, retry bound, cancellation, stale frame/document and retained challenge passed\n";
}
