#pragma once
#include "e32base.h"
#include "eikseced.h"
#include "browser_ui.rsg"
#include <deque>
#include <functional>
#include <memory>

struct DialogStep {
    bool accepted = true;
    std::u16string text;
    std::function<void()> nested;
    int leave = 0;
};
inline std::deque<DialogStep> dialogSteps;
inline std::vector<int> dialogResources;
inline std::vector<int> dialogMaxLengths;
inline std::vector<std::u16string> dialogHeadings;
struct DialogState { bool cancelled = false; bool destroyed = false; };
class CAknTextQueryDialog : public CBase {
public:
    explicit CAknTextQueryDialog(TDes& value) : value(value) { }
    ~CAknTextQueryDialog() { if (state) state->destroyed = true; }
    void SetMaxLength(TInt n) { assert(n > 0 && n <= value.MaxLength()); maximumLength = n; }
    void SetPredictiveTextInputPermitted(bool allowed) { assert(!allowed); }
    void MakeLeftSoftkeyVisible(bool visible) { leftVisible = visible; }
    bool ExecuteLD(int resource, const TDesC& heading)
    {
        assert(!dialogSteps.empty());
        auto step = std::move(dialogSteps.front()); dialogSteps.pop_front();
        // Model the real secret editor bound, independently of buffer capacity.
        if (resource == R_BROWSER_AUTH_PASSWORD)
            assert(maximumLength <= CEikSecretEditor::EMaxSecEdSecArrayLength);
        dialogResources.push_back(resource);
        dialogMaxLengths.push_back(maximumLength);
        dialogHeadings.emplace_back(heading.Ptr(), heading.Length());
        UpdateLeftSoftKeyL();
        assert(leftVisible); // Empty credentials must remain acceptable.
        auto execution = state = std::make_shared<DialogState>();
        auto* output = &value;
        if (step.nested) step.nested();
        bool accepted = step.accepted && !execution->cancelled && !step.leave;
        if (accepted) {
            assert(step.text.length() <= static_cast<size_t>(maximumLength));
            output->Copy(step.text);
        }
        if (!execution->destroyed) delete this;
        if (step.leave) throw step.leave;
        return accepted;
    }
protected:
    void TryExitL(int) { state->cancelled = true; delete this; }
    virtual void UpdateLeftSoftKeyL() { leftVisible = false; }
private:
    TDes& value;
    bool leftVisible = false;
    TInt maximumLength = 0;
    std::shared_ptr<DialogState> state;
};
