#include "MediaPlayers.h"

#include <coemain.h>
#include <e32debug.h>
#include <f32file.h>
#include <mdaaudiosampleplayer.h>
#include <videoplayer2.h>
#include <cmath>
#include <limits>
#include <w32std.h>

using namespace WebKitSymbian;

namespace {

const TReal KMicrosecondsPerSecond = 1000000;

// The engine's UTF-8 path, "C:/Data/x.mp3", as a file name.
TFileName FileNameL(const char* aPath)
{
    TFileName name;
    const unsigned char* c = reinterpret_cast<const unsigned char*>(aPath);
    while (*c) {
        unsigned character = *c++;
        unsigned extra = 0, minimum = 0;
        if (character >= 0xf0 && character <= 0xf4) { extra = 3; minimum = 0x10000; character &= 7; }
        else if (character >= 0xe0 && character <= 0xef) { extra = 2; minimum = 0x800; character &= 15; }
        else if (character >= 0xc2 && character <= 0xdf) { extra = 1; minimum = 0x80; character &= 31; }
        else if (character >= 0x80) User::Leave(KErrArgument);
        for (unsigned i = 0; i < extra; ++i) {
            if ((*c & 0xc0) != 0x80) User::Leave(KErrArgument);
            character = (character << 6) | (*c++ & 63);
        }
        if (character < minimum || character > 0x10ffff || (character >= 0xd800 && character <= 0xdfff))
            User::Leave(KErrArgument);
        unsigned units = character > 0xffff ? 2 : 1;
        if (name.Length() + units > name.MaxLength()) User::Leave(KErrBadName);
        if (units == 2) {
            character -= 0x10000;
            name.Append(TChar(0xd800 | (character >> 10)));
            name.Append(TChar(0xdc00 | (character & 1023)));
        } else
            name.Append(TChar(character == '/' ? '\\' : character));
    }
    return name;
}

double Seconds(const TTimeIntervalMicroSeconds& aTime)
{
    return I64REAL(aTime.Int64()) / KMicrosecondsPerSecond;
}

TTimeIntervalMicroSeconds Microseconds(double aSeconds)
{
    return TTimeIntervalMicroSeconds(static_cast<TInt64>(aSeconds * KMicrosecondsPerSecond));
}

// The download, written and read through handles that share the file. The
// session is shared with the MMF server, which reads the file in its own
// process.
class TMediaFile {
public:
    void Close()
    {
        iReader.Close();
        iWriter.Close();
        iFs.Close();
    }

    TInt Create(const char* aPath)
    {
        TRAPD(error, {
            User::LeaveIfError(Connect());
            User::LeaveIfError(iWriter.Replace(iFs, FileNameL(aPath), EFileWrite | EFileShareReadersOrWriters));
        });
        return error;
    }

    TInt Append(const char* aData, unsigned aLength)
    {
        if (aLength > static_cast<unsigned>(KMaxTInt)) return KErrArgument;
        TInt error = iWriter.Write(TPtrC8(reinterpret_cast<const TUint8*>(aData), aLength));
        return error ? error : Flush();
    }

    TInt Flush() { return iWriter.SubSessionHandle() ? iWriter.Flush() : KErrNone; }

    void CloseWriter() { iWriter.Close(); }

    // A handle to the file for the player, which must have closed the last one.
    const RFile& ReaderL(const char* aPath)
    {
        User::LeaveIfError(Connect());
        iReader.Close();
        User::LeaveIfError(iReader.Open(iFs, FileNameL(aPath), EFileRead | EFileShareReadersOrWriters));
        return iReader;
    }

private:
    TInt Connect()
    {
        if (iFs.Handle())
            return KErrNone;
        TInt error = iFs.Connect();
        if (!error)
            error = iFs.ShareProtected();
        return error;
    }

    RFs iFs;
    RFile iWriter;
    RFile iReader;
};

class CAudioPlayer final : public CBase, public MediaPlayer, public MMdaAudioPlayerCallback {
public:
    explicit CAudioPlayer(MediaPlayerClient& aClient)
        : iClient(aClient)
    {
    }

    ~CAudioPlayer()
    {
        if (iUtility)
            iUtility->Close();
        delete iUtility;
        iFile.Close();
    }

    int createFile(const char* aPath) final { return iFile.Create(aPath); }
    int appendToFile(const char* aData, unsigned aLength) final { return iFile.Append(aData, aLength); }
    int flushFile() final { return iFile.Flush(); }
    void closeFile() final { iFile.CloseWriter(); }

    void open(const char* aPath) final
    {
        iOpened = EFalse;
        TRAPD(error, {
            if (iUtility)
                iUtility->Close();
            else
                iUtility = CMdaAudioPlayerUtility::NewL(*this);
            iUtility->OpenFileL(iFile.ReaderL(aPath));
        });
        if (error)
            iClient.mediaPlayerOpened(error, 0, 0, 0, false);
    }

    int play() final { if (!iOpened) return KErrNotReady; iUtility->Play(); return KErrNone; }
    int pause() final { return iOpened ? iUtility->Pause() : KErrNotReady; }

    int position(double& aSeconds) const final
    {
        if (!iOpened) return KErrNotReady;
        TTimeIntervalMicroSeconds position(0);
        TInt error = iUtility->GetPosition(position);
        if (!error) aSeconds = Seconds(position);
        return error;
    }

    int setPosition(double aSeconds) final
    {
        if (!iOpened) return KErrNotReady;
        if (!std::isfinite(aSeconds) || aSeconds < 0) return KErrArgument;
        iUtility->SetPosition(Microseconds(aSeconds));
        return KErrNone;
    }

    int setVolume(double aVolume) final
    {
        if (!std::isfinite(aVolume) || aVolume < 0 || aVolume > 1) return KErrArgument;
        iVolume = aVolume;
        return iOpened ? iUtility->SetVolume(static_cast<TInt>(aVolume * iUtility->MaxVolume() + 0.5)) : KErrNone;
    }

    // MMdaAudioPlayerCallback
    void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds& aDuration) final
    {
        iOpened = !aError;
        if (iOpened) aError = setVolume(iVolume);
        iClient.mediaPlayerOpened(aError, aError ? 0 : Seconds(aDuration), 0, 0, !aError);
    }

    void MapcPlayComplete(TInt aError) final { iClient.mediaPlayerEnded(aError); }

private:
    MediaPlayerClient& iClient;
    TMediaFile iFile;
    CMdaAudioPlayerUtility* iUtility { nullptr };
    TBool iOpened { EFalse };
    double iVolume { 1 };
};

class CVideoPlayer final : public CBase, public MediaPlayer, public MVideoPlayerUtilityObserver {
public:
    CVideoPlayer(MediaPlayerClient& aClient, RWindow& aWindow)
        : iClient(aClient)
        , iWindow(aWindow)
    {
    }

    ~CVideoPlayer()
    {
        if (iUtility)
            iUtility->Close();
        delete iUtility;
        iFile.Close();
    }

    int createFile(const char* aPath) final { return iFile.Create(aPath); }
    int appendToFile(const char* aData, unsigned aLength) final { return iFile.Append(aData, aLength); }
    int flushFile() final { return iFile.Flush(); }
    void closeFile() final { iFile.CloseWriter(); }

    void open(const char* aPath) final
    {
        iPrepared = EFalse;
        iPlaying = EFalse;
        if (iWindowAdded && iUtility) iUtility->RemoveDisplayWindow(iWindow);
        iWindowAdded = EFalse;
        TRAPD(error, {
            if (iUtility) {
                iUtility->Close();
            } else {
                // Hidden (an empty clip) until the page paints the element.
                iUtility = CVideoPlayerUtility2::NewL(*this, EMdaPriorityNormal, EMdaPriorityPreferenceTimeAndQuality);
            }
            iUtility->OpenFileL(iFile.ReaderL(aPath));
        });
        if (error)
            iClient.mediaPlayerOpened(error, 0, 0, 0, false);
    }

    int play() final
    {
        if (!iPrepared) return KErrNotReady;
        iUtility->Play();
        iPlaying = ETrue;
        return KErrNone;
    }

    int pause() final
    {
        if (!iPrepared) return KErrNotReady;
        TRAPD(error, iUtility->PauseL());
        if (!error) iPlaying = EFalse;
        return error;
    }

    int position(double& aSeconds) const final
    {
        if (!iPrepared) return KErrNotReady;
        TTimeIntervalMicroSeconds position(0);
        TRAPD(error, position = iUtility->PositionL());
        if (!error) aSeconds = Seconds(position);
        return error;
    }

    int setPosition(double aSeconds) final
    {
        if (!iPrepared) return KErrNotReady;
        if (!std::isfinite(aSeconds) || aSeconds < 0) return KErrArgument;
        TBool playing = iPlaying;
        TInt error = playing ? pause() : KErrNone;
        if (error) return error;
        TRAP(error, iUtility->SetPositionL(Microseconds(aSeconds)));
        if (!error && playing) error = play();
        return error;
    }

    int setVolume(double aVolume) final
    {
        if (!std::isfinite(aVolume) || aVolume < 0 || aVolume > 1) return KErrArgument;
        iVolume = aVolume;
        if (!iPrepared || !iHasAudio) return KErrNone;
        TRAPD(error, iUtility->SetVolumeL(static_cast<TInt>(aVolume * iUtility->MaxVolume() + 0.5)));
        return error;
    }

    int setVideoRect(const Rect& aRect, const Rect& aClip) final
    {
        iRect = TRect(TPoint(aRect.x, aRect.y), TSize(aRect.width, aRect.height));
        iClip = TRect(TPoint(aClip.x, aClip.y), TSize(aClip.width, aClip.height));
        return UpdateDisplayWindow();
    }

    // MVideoPlayerUtilityObserver
    void MvpuoOpenComplete(TInt aError) final
    {
        if (aError) {
            iClient.mediaPlayerOpened(aError, 0, 0, 0, false);
            return;
        }
        iUtility->Prepare();
    }

    void MvpuoPrepareComplete(TInt aError) final
    {
        TSize size(0, 0);
        TTimeIntervalMicroSeconds duration(0);
        if (!aError) {
            TRAP(aError, {
                iUtility->VideoFrameSizeL(size);
                duration = iUtility->DurationL();
                iHasAudio = iUtility->AudioEnabledL();
            });
        }
        if (!aError) {
            iPrepared = ETrue;
            aError = setVolume(iVolume);
            if (!aError) aError = UpdateDisplayWindow();
        }
        if (aError) iPrepared = EFalse;
        iClient.mediaPlayerOpened(aError, aError ? 0 : Seconds(duration), size.iWidth, size.iHeight, !aError && iHasAudio);
    }

    void MvpuoFrameReady(CFbsBitmap&, TInt) final { }

    void MvpuoPlayComplete(TInt aError) final
    {
        iPlaying = EFalse;
        iClient.mediaPlayerEnded(aError);
    }

    void MvpuoEvent(const TMMFEvent& aEvent) final
    {
        if (aEvent.iErrorCode != KErrNone) iClient.mediaPlayerEnded(aEvent.iErrorCode);
    }

private:
    TInt UpdateDisplayWindow()
    {
        if (!iUtility || !iPrepared) return KErrNone;
        if (iClip.IsEmpty()) {
            if (iWindowAdded) iUtility->RemoveDisplayWindow(iWindow);
            iWindowAdded = EFalse;
            return KErrNone;
        }
        CCoeEnv* env = CCoeEnv::Static();
        TRAPD(error, {
            if (!iWindowAdded) {
                iUtility->AddDisplayWindowL(env->WsSession(), *env->ScreenDevice(), iWindow, iRect, iClip);
                iWindowAdded = ETrue;
            } else {
                iUtility->SetVideoExtentL(iWindow, iRect);
                iUtility->SetWindowClipRectL(iWindow, iClip);
            }
        });
        return error;
    }

    MediaPlayerClient& iClient;
    RWindow& iWindow;
    TMediaFile iFile;
    CVideoPlayerUtility2* iUtility { nullptr };
    TRect iRect;
    TRect iClip;
    TBool iPrepared { EFalse };
    TBool iWindowAdded { EFalse };
    TBool iHasAudio { EFalse };
    TBool iPlaying { EFalse };
    double iVolume { 1 };
};

} // namespace

MediaPlayer* CreateMediaPlayer(void* aWindow, MediaPlayerClient& aClient, bool aHasVideo)
{
    if (aHasVideo)
        return new CVideoPlayer(aClient, *static_cast<RWindow*>(aWindow));
    return new CAudioPlayer(aClient);
}
