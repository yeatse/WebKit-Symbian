#include "MediaPlayers.h"

#include <coemain.h>
#include <e32debug.h>
#include <f32file.h>
#include <mdaaudiosampleplayer.h>
#include <videoplayer.h>
#include <w32std.h>

using namespace WebKitSymbian;

namespace {

const TReal KMicrosecondsPerSecond = 1000000;

// The engine's UTF-8 path, "C:/Data/x.mp3", as a file name.
TFileName FileName(const char* aPath)
{
    TFileName name;
    const unsigned char* c = reinterpret_cast<const unsigned char*>(aPath);
    while (*c && name.Length() < name.MaxLength()) {
        TUint character = *c++;
        if (character >= 0xe0 && c[0] && c[1]) {
            character = ((character & 0x0f) << 12) | ((c[0] & 0x3f) << 6) | (c[1] & 0x3f);
            c += 2;
        } else if (character >= 0xc0 && c[0]) {
            character = ((character & 0x1f) << 6) | (c[0] & 0x3f);
            c += 1;
        }
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

    bool Create(const char* aPath)
    {
        return Connect() == KErrNone
            && iWriter.Replace(iFs, FileName(aPath), EFileWrite | EFileShareReadersOrWriters) == KErrNone;
    }

    bool Append(const char* aData, int aLength)
    {
        return iWriter.Write(TPtrC8(reinterpret_cast<const TUint8*>(aData), aLength)) == KErrNone;
    }

    void CloseWriter() { iWriter.Close(); }

    // A handle to the file for the player, which must have closed the last one.
    const RFile& ReaderL(const char* aPath)
    {
        User::LeaveIfError(Connect());
        iReader.Close();
        User::LeaveIfError(iReader.Open(iFs, FileName(aPath), EFileRead | EFileShareReadersOrWriters));
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

    bool createFile(const char* aPath) final { return iFile.Create(aPath); }
    bool appendToFile(const char* aData, int aLength) final { return iFile.Append(aData, aLength); }
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
            iClient.mediaPlayerOpened(error, 0, 0, 0);
    }

    void play() final { iUtility->Play(); }
    void pause() final { iUtility->Pause(); }

    double position() const final
    {
        TTimeIntervalMicroSeconds position(0);
        iUtility->GetPosition(position);
        return Seconds(position);
    }

    void setPosition(double aSeconds) final { iUtility->SetPosition(Microseconds(aSeconds)); }

    void setVolume(double aVolume) final
    {
        iVolume = aVolume;
        if (iOpened)
            iUtility->SetVolume(static_cast<TInt>(aVolume * iUtility->MaxVolume() + 0.5));
    }

    // MMdaAudioPlayerCallback
    void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds& aDuration) final
    {
        iOpened = !aError;
        if (iOpened)
            setVolume(iVolume);
        iClient.mediaPlayerOpened(aError, aError ? 0 : Seconds(aDuration), 0, 0);
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
    CVideoPlayer(MediaPlayerClient& aClient, RWindowBase& aWindow)
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

    bool createFile(const char* aPath) final { return iFile.Create(aPath); }
    bool appendToFile(const char* aData, int aLength) final { return iFile.Append(aData, aLength); }
    void closeFile() final { iFile.CloseWriter(); }

    void open(const char* aPath) final
    {
        iPrepared = EFalse;
        iPlaying = EFalse;
        TRAPD(error, {
            if (iUtility) {
                iUtility->Close();
            } else {
                // Hidden (an empty clip) until the page paints the element.
                CCoeEnv* env = CCoeEnv::Static();
                iUtility = CVideoPlayerUtility::NewL(*this, EMdaPriorityNormal, EMdaPriorityPreferenceTimeAndQuality,
                    env->WsSession(), *env->ScreenDevice(), iWindow, ScreenRect(iRect), TRect());
            }
            iUtility->OpenFileL(iFile.ReaderL(aPath));
        });
        if (error)
            iClient.mediaPlayerOpened(error, 0, 0, 0);
    }

    void play() final
    {
        iUtility->Play();
        iPlaying = ETrue;
    }

    void pause() final
    {
        TRAP_IGNORE(iUtility->PauseL());
        iPlaying = EFalse;
    }

    double position() const final
    {
        TTimeIntervalMicroSeconds position(0);
        TRAP_IGNORE(position = iUtility->PositionL());
        return Seconds(position);
    }

    void setPosition(double aSeconds) final
    {
        // The utility only seeks while stopped or paused.
        TBool playing = iPlaying;
        if (playing)
            pause();
        TRAP_IGNORE(iUtility->SetPositionL(Microseconds(aSeconds)));
        if (playing)
            play();
    }

    void setVolume(double aVolume) final
    {
        iVolume = aVolume;
        if (iPrepared)
            TRAP_IGNORE(iUtility->SetVolumeL(static_cast<TInt>(aVolume * iUtility->MaxVolume() + 0.5)));
    }

    void setVideoRect(const Rect& aRect, const Rect& aClip) final
    {
        iRect = TRect(TPoint(aRect.x, aRect.y), TSize(aRect.width, aRect.height));
        iClip = TRect(TPoint(aClip.x, aClip.y), TSize(aClip.width, aClip.height));
        UpdateDisplayWindow();
    }

    // MVideoPlayerUtilityObserver
    void MvpuoOpenComplete(TInt aError) final
    {
        if (aError) {
            iClient.mediaPlayerOpened(aError, 0, 0, 0);
            return;
        }
        iUtility->Prepare();
    }

    void MvpuoPrepareComplete(TInt aError) final
    {
        TSize size;
        TTimeIntervalMicroSeconds duration(0);
        if (!aError) {
            TRAP_IGNORE(iUtility->VideoFrameSizeL(size));
            TRAP_IGNORE(duration = iUtility->DurationL());
            iPrepared = ETrue;
            setVolume(iVolume);
            // Again, for a file opened again.
            UpdateDisplayWindow();
        }
        iClient.mediaPlayerOpened(aError, Seconds(duration), size.iWidth, size.iHeight);
    }

    void MvpuoFrameReady(CFbsBitmap&, TInt) final { }

    void MvpuoPlayComplete(TInt aError) final
    {
        iPlaying = EFalse;
        iClient.mediaPlayerEnded(aError);
    }

    void MvpuoEvent(const TMMFEvent&) final { }

private:
    void UpdateDisplayWindow()
    {
        if (!iUtility)
            return;
        CCoeEnv* env = CCoeEnv::Static();
        TRAPD(error, iUtility->SetDisplayWindowL(env->WsSession(), *env->ScreenDevice(), iWindow,
            ScreenRect(iRect), ScreenRect(iClip)));
        if (error)
            RDebug::Printf("browser: video SetDisplayWindowL %d", error);
    }

    // The utility takes screen coordinates; the engine's are the window's.
    TRect ScreenRect(TRect aRect) const
    {
        aRect.Move(iWindow.AbsPosition());
        return aRect;
    }

    MediaPlayerClient& iClient;
    RWindowBase& iWindow;
    TMediaFile iFile;
    CVideoPlayerUtility* iUtility { nullptr };
    TRect iRect;
    TRect iClip;
    TBool iPrepared { EFalse };
    TBool iPlaying { EFalse };
    double iVolume { 1 };
};

} // namespace

MediaPlayer* CreateMediaPlayer(void* aWindow, MediaPlayerClient& aClient, bool aHasVideo)
{
    if (aHasVideo)
        return new CVideoPlayer(aClient, *static_cast<RWindowBase*>(aWindow));
    return new CAudioPlayer(aClient);
}
