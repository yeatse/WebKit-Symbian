// Native HTTP authentication UI. Kept alive across AVKON's nested event loop.
#pragma once

#include "NativeHeapDelete.h"
#include <WebKitSymbian.h>

class CAuthenticationQueryDialog;

class CAuthenticationUI : public CBase {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    bool Run(uint64_t, WebKitSymbian::Text origin, unsigned attempt, WebKitSymbian::TextResult& username, WebKitSymbian::TextResult& password);
    void Cancel(uint64_t identifier);
    void Release();

private:
    TBool RunL(WebKitSymbian::Text origin, unsigned attempt, TDes& username, TDes& password);
    unsigned iReferences { 1 };
    uint64_t iIdentifier { 0 };
    bool iBusy { false };
    bool iCancelled { false };
    CAuthenticationQueryDialog* iDialog { nullptr };
};
