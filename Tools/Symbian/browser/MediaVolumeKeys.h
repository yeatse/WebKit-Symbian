// The volume keys set the browser's media volume (see MediaPlayers.h), which
// a bar over the page shows for a moment.
#pragma once

#include "NativeHeapDelete.h"

#include <e32base.h>
#include <remconcoreapitargetobserver.h>

class CRemConInterfaceSelector;
class CRemConCoreApiTarget;
class CMediaVolumeBar;

class CMediaVolumeKeys final : public CBase, public MRemConCoreApiTargetObserver {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    // Without the remote control server (the emulator), the keys do nothing.
    static CMediaVolumeKeys* NewL();
    ~CMediaVolumeKeys();

private:
    CMediaVolumeKeys() = default;
    void ConstructL();

    // MRemConCoreApiTargetObserver
    void MrccatoCommand(TRemConCoreApiOperationId aOperationId, TRemConCoreApiButtonAction aButtonAct) override;

    // Returns whether the level changed.
    TBool Step(TInt aDelta);
    static TInt Repeat(TAny* aSelf);

    CRemConInterfaceSelector* iSelector { nullptr };
    // Owned by iSelector.
    CRemConCoreApiTarget* iTarget { nullptr };
    CMediaVolumeBar* iBar { nullptr };
    // While a key is held, the level keeps going in iRepeatDelta's direction.
    CPeriodic* iRepeat { nullptr };
    TInt iRepeatDelta { 0 };
};
