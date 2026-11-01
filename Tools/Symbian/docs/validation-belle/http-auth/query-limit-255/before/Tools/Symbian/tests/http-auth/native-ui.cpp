#include "AuthenticationUI.h"
#include <AknQueryDialog.h>
#include <browser_ui.rsg>
#include <iostream>

using namespace WebKitSymbian;
struct Result : TextResult {
    void setText(Text text) override { ++calls; value.assign(reinterpret_cast<const char16_t*>(text.characters), text.length); }
    int calls = 0;
    std::u16string value;
};
static Text origin() { static const char16_t text[] = u"https://example.test:8443"; return { reinterpret_cast<const uint16_t*>(text), sizeof(text) / sizeof(char16_t) - 1 }; }
static void reset() { assert(dialogSteps.empty()); dialogResources.clear(); dialogHeadings.clear(); }
int main()
{
    auto* ui = new (ELeave) CAuthenticationUI;
    {
        Result user, password;
        dialogSteps = {{true, u"alice"}, {true, u"secret"}};
        assert(ui->Run(10, origin(), 1, user, password));
        assert(user.value == u"alice" && password.value == u"secret");
        assert((dialogResources == std::vector<int>{R_BROWSER_PROMPT, R_BROWSER_AUTH_PASSWORD}));
        assert(dialogHeadings[0].find(u"https://example.test:8443") != std::u16string::npos);
        reset();
    }
    {
        Result user, password;
        dialogSteps = {{true, u""}, {true, u""}};
        assert(ui->Run(10, origin(), 3, user, password));
        assert(user.calls == 1 && password.calls == 1 && password.value.empty());
        assert(dialogHeadings[0].find(u"(3/3)") != std::u16string::npos);
        reset();
    }
    for (bool cancelPassword : {false, true}) {
        Result user, password;
        if (cancelPassword) dialogSteps.push_back({true, u"alice"});
        dialogSteps.push_back({false, u""});
        assert(!ui->Run(10, origin(), 1, user, password));
        assert(!user.calls && !password.calls);
        reset();
    }
    for (int stage : {0, 1}) {
        Result user, password;
        if (stage) dialogSteps.push_back({true, u"alice"});
        dialogSteps.push_back({true, u"ignored", [&] { ui->Cancel(10); }});
        assert(!ui->Run(10, origin(), 1, user, password));
        assert(!user.calls && !password.calls);
        reset();
    }
    {
        Result user, password;
        dialogSteps = {{true, u"alice", [&] {
            ui->Cancel(11); // Another frame's cancellation must not close us.
            Result nestedUser, nestedPassword;
            assert(!ui->Run(11, origin(), 1, nestedUser, nestedPassword));
        }}, {true, u"secret"}};
        assert(ui->Run(10, origin(), 1, user, password));
        reset();
    }
    for (int stage : {0, 1}) {
        Result user, password;
        if (stage) dialogSteps.push_back({true, u"alice"});
        dialogSteps.push_back({true, u"", {}, -4});
        assert(!ui->Run(10, origin(), 1, user, password));
        assert(!user.calls && !password.calls);
        reset();
    }
    {
        Result user, password;
        assert(!ui->Run(10, origin(), 0, user, password));
        assert(!ui->Run(10, origin(), 4, user, password));
        assert(!ui->Run(0, origin(), 1, user, password));
        assert(!ui->Run(10, {nullptr, 10}, 1, user, password));
    }
    // The native container can cancel and release the controller while its
    // dialog is running; Run's reference must survive until it unwinds.
    {
        Result user, password;
        dialogSteps = {{true, u"ignored", [&] { ui->Cancel(0); ui->Release(); }}};
        assert(!ui->Run(10, origin(), 1, user, password));
        assert(!user.calls && !password.calls);
        reset();
    }
    assert(!nativeAllocations);
    std::cout << "native auth: success, empty values, cancel, nested lifetime, Leave and attempt bounds passed\n";
}
