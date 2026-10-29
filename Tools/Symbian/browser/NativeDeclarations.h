// Declarations of OS exports the SDK ships import proxies for but no headers.
// Only the members the browser calls are declared; layouts are not relied on.
#pragma once

#include <apaapp.h>
#include <e32base.h>

// eikstart.h
namespace EikStart {
IMPORT_C TInt RunApplication(TApaApplicationFactory aApplicationFactory);
}

// aknextendedinputcapabilities.h (eikctl.dll): how a custom editor asks the
// AVKON FEP for the touch keyboard. The FEP finds the object through the
// editor's MObjectProvider chain by this type UID.
class CAknExtendedInputCapabilities : public CBase {
public:
    enum { ETypeId = 0x10282348 };
    enum TEvent { EActivatePenInputRequest = 0, EPointerEventReceived, EControlContentUpdatedInternally };
    enum TEditorType { EUndefined, EEdwinBased };
    IMPORT_C static CAknExtendedInputCapabilities* NewL();
    IMPORT_C void SetEditorType(TInt aEditorType);
    IMPORT_C void ReportEventL(TInt aEvent, TAny* aParams);
};
