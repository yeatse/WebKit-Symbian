// JPEG decode probe: lists the ICL image decoder plug-ins (looking for a
// hardware JPEG decoder), then times ICL and libjpeg on baseline and
// progressive JPEGs of several sizes. The JPEGs are encoded here with
// libjpeg, or read from C:\Data\jpegprobe\*.jpg when that folder exists.
// Every ICL decode is cancelled after 15 s and the run stops after 3
// minutes, so the probe always exits. Results: C:\Data\jpegprobe.txt.
#include <e32base.h>
#include <e32std.h>
#include <f32file.h>
#include <fbs.h>
#include <hal.h>
#include <ecom/ecom.h>
#include <imageconversion.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
extern "C" {
#include <jpeglib.h>
}

namespace {

FILE* gLog;
void Log(const char* format, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(gLog, format, args);
    va_end(args);
    fflush(gLog);
}

TInt gCounterFrequency = 1000;
TUint32 gDeadline;
const TInt kRunSeconds = 180;
const TInt kDecodeTimeoutMicroseconds = 15000000;
TThreadId gMainThread;

double Milliseconds(TUint32 start, TUint32 end) { return (end - start) * 1000.0 / gCounterFrequency; }
bool PastDeadline() { return static_cast<TInt32>(User::NTickCount() - gDeadline) > 0; }

void LogDes(const TDesC& text)
{
    for (TInt i = 0; i < text.Length(); ++i)
        fputc(text[i] < 128 ? static_cast<char>(text[i]) : '?', gLog);
}

void LogBytes(const TDesC8& data)
{
    for (TInt i = 0; i < data.Length(); ++i) {
        TUint8 c = data[i];
        if (c >= 32 && c < 127)
            fputc(c, gLog);
        else
            fprintf(gLog, "\\x%02x", c);
    }
}

struct Jpeg {
    char name[48];
    unsigned char* data { nullptr };
    unsigned long size { 0 };
    int width { 0 };
    int height { 0 };
    bool progressive { false };
};

struct ErrorManager {
    jpeg_error_mgr manager;
    jmp_buf jump;
};

void OnJpegError(j_common_ptr info)
{
    char message[JMSG_LENGTH_MAX];
    info->err->format_message(info, message);
    Log("  libjpeg error: %s\n", message);
    longjmp(reinterpret_cast<ErrorManager*>(info->err)->jump, 1);
}

// Smooth gradients, edges and some noise, so the encoded size per pixel
// is close to a photo's.
bool Encode(Jpeg& jpeg, int width, int height, bool progressive)
{
    jpeg_compress_struct info;
    ErrorManager error;
    info.err = jpeg_std_error(&error.manager);
    error.manager.error_exit = OnJpegError;
    unsigned char* row = static_cast<unsigned char*>(malloc(width * 3));
    if (!row)
        return false;
    if (setjmp(error.jump)) {
        jpeg_destroy_compress(&info);
        free(row);
        return false;
    }
    jpeg_create_compress(&info);
    jpeg_mem_dest(&info, &jpeg.data, &jpeg.size);
    info.image_width = width;
    info.image_height = height;
    info.input_components = 3;
    info.in_color_space = JCS_RGB;
    jpeg_set_defaults(&info);
    jpeg_set_quality(&info, 85, TRUE);
    if (progressive)
        jpeg_simple_progression(&info);
    jpeg_start_compress(&info, TRUE);
    TUint32 seed = 12345;
    while (info.next_scanline < info.image_height) {
        int y = info.next_scanline;
        for (int x = 0; x < width; ++x) {
            seed = seed * 1103515245 + 12345;
            int noise = (seed >> 16) % 24;
            bool block = ((x * 7 / width) + (y * 5 / height)) & 1;
            row[x * 3] = (x * 255 / width + noise) & 255;
            row[x * 3 + 1] = (y * 255 / height + (block ? 60 : 0) + noise) & 255;
            row[x * 3 + 2] = (((x + y) * 128 / (width + height)) + (block ? 0 : 90) + noise) & 255;
        }
        jpeg_write_scanlines(&info, &row, 1);
    }
    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    free(row);
    jpeg.width = width;
    jpeg.height = height;
    jpeg.progressive = progressive;
    snprintf(jpeg.name, sizeof jpeg.name, "%dx%d %s", width, height, progressive ? "progressive" : "baseline");
    return true;
}

enum class LibjpegMode { WebKit, Fast, Half, Quarter };

const char* ModeName(LibjpegMode mode)
{
    switch (mode) {
    case LibjpegMode::WebKit: return "libjpeg islow+fancy -> BGRA (WebKit)";
    case LibjpegMode::Fast: return "libjpeg ifast, plain upsampling";
    case LibjpegMode::Half: return "libjpeg 1/2 DCT scaling";
    case LibjpegMode::Quarter: return "libjpeg 1/4 DCT scaling";
    }
    return "";
}

// One decode to 32-bit pixels, as WebKit's JPEGImageDecoder writes them.
bool DecodeLibjpeg(const Jpeg& jpeg, LibjpegMode mode, double& milliseconds, int& outWidth, int& outHeight)
{
    jpeg_decompress_struct info;
    ErrorManager error;
    info.err = jpeg_std_error(&error.manager);
    error.manager.error_exit = OnJpegError;
    // Set after setjmp, so volatile to survive the longjmp.
    unsigned char* volatile row = nullptr;
    TUint32* volatile pixels = nullptr;
    TUint32 start = User::FastCounter();
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&info);
        free(row);
        free(pixels);
        return false;
    }
    jpeg_create_decompress(&info);
    jpeg_mem_src(&info, jpeg.data, jpeg.size);
    jpeg_read_header(&info, TRUE);
    info.out_color_space = JCS_RGB;
    info.dct_method = mode == LibjpegMode::Fast ? JDCT_IFAST : JDCT_ISLOW;
    info.do_fancy_upsampling = mode == LibjpegMode::Fast ? FALSE : TRUE;
    if (mode == LibjpegMode::Half || mode == LibjpegMode::Quarter) {
        info.scale_num = 1;
        info.scale_denom = mode == LibjpegMode::Half ? 2 : 4;
    }
    if (info.progressive_mode)
        info.buffered_image = TRUE;
    jpeg_start_decompress(&info);
    outWidth = info.output_width;
    outHeight = info.output_height;
    row = static_cast<unsigned char*>(malloc(outWidth * 3));
    pixels = static_cast<TUint32*>(malloc(outWidth * outHeight * 4));
    if (!row || !pixels)
        longjmp(error.jump, 1);
    auto readRows = [&] {
        unsigned char* line = row;
        while (info.output_scanline < info.output_height) {
            TUint32* out = pixels + info.output_scanline * outWidth;
            jpeg_read_scanlines(&info, &line, 1);
            for (int x = 0; x < outWidth; ++x)
                out[x] = 0xff000000 | line[x * 3] << 16 | line[x * 3 + 1] << 8 | line[x * 3 + 2];
        }
    };
    if (info.buffered_image) {
        // WebKit shows each scan once all the data has arrived: one output pass.
        while (!jpeg_input_complete(&info))
            jpeg_consume_input(&info);
        jpeg_start_output(&info, info.input_scan_number);
        readRows();
        jpeg_finish_output(&info);
    } else
        readRows();
    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
    milliseconds = Milliseconds(start, User::FastCounter());
    free(row);
    free(pixels);
    return true;
}

class CTimeout : public CTimer {
public:
    static CTimeout* NewL(CActiveSchedulerWait& wait)
    {
        CTimeout* self = new (ELeave) CTimeout(wait);
        CleanupStack::PushL(self);
        self->ConstructL();
        CleanupStack::Pop(self);
        return self;
    }
    bool iFired { false };

private:
    CTimeout(CActiveSchedulerWait& wait) : CTimer(EPriorityStandard), iWait(wait) { CActiveScheduler::Add(this); }
    void RunL() override
    {
        iFired = true;
        if (iWait.IsStarted())
            iWait.AsyncStop();
    }
    CActiveSchedulerWait& iWait;
};

class CDecodeWaiter : public CActive {
public:
    CDecodeWaiter(CActiveSchedulerWait& wait) : CActive(EPriorityStandard), iWait(wait) { CActiveScheduler::Add(this); }
    ~CDecodeWaiter() { Cancel(); }
    void Start() { SetActive(); }
    TRequestStatus& Status() { return iStatus; }

private:
    void RunL() override
    {
        if (iWait.IsStarted())
            iWait.AsyncStop();
    }
    void DoCancel() override { }
    CActiveSchedulerWait& iWait;
};

struct IclResult {
    TInt error { KErrNone };
    double createMilliseconds { 0 };
    double convertMilliseconds { 0 };
    TUid implementation { KNullUid };
};

// Decodes into an EColor16MU bitmap of the frame size divided by
// |reduction|, the way an ICL client would.
IclResult DecodeIcl(RFs& fs, const Jpeg& jpeg, TUid decoder, int reduction, CImageDecoder::TOptions options = CImageDecoder::EOptionNone)
{
    IclResult result;
    TPtrC8 source(jpeg.data, jpeg.size);
    TUint32 start = User::FastCounter();
    CImageDecoder* image = nullptr;
    TRAP(result.error, image = CImageDecoder::DataNewL(fs, source, options, KNullUid, KNullUid, decoder));
    TUint32 created = User::FastCounter();
    result.createMilliseconds = Milliseconds(start, created);
    if (result.error)
        return result;
    result.implementation = image->ImplementationUid();
    TSize size = image->FrameInfo(0).iOverallSizeInPixels;
    size.iWidth = (size.iWidth + reduction - 1) / reduction;
    size.iHeight = (size.iHeight + reduction - 1) / reduction;
    CFbsBitmap* bitmap = new CFbsBitmap;
    result.error = bitmap ? bitmap->Create(size, EColor16MU) : KErrNoMemory;
    if (result.error) {
        delete bitmap;
        delete image;
        return result;
    }
    // CBase objects belong on the heap: CBase's operator new zero-fills them
    // and their constructors may not. A CActiveSchedulerWait on the stack can
    // look started already, and Start() panics with E32USER-CBase 91.
    CActiveSchedulerWait* wait = new CActiveSchedulerWait;
    CDecodeWaiter* waiter = wait ? new CDecodeWaiter(*wait) : nullptr;
    CTimeout* timeout = nullptr;
    if (waiter) {
        TRAP(result.error, timeout = CTimeout::NewL(*wait));
    } else
        result.error = KErrNoMemory;
    if (result.error) {
        delete waiter;
        delete wait;
        delete bitmap;
        delete image;
        return result;
    }
    start = User::FastCounter();
    image->Convert(&waiter->Status(), *bitmap);
    waiter->Start();
    timeout->After(kDecodeTimeoutMicroseconds);
    wait->Start();
    result.convertMilliseconds = Milliseconds(start, User::FastCounter());
    if (timeout->iFired) {
        // Cancelling a stuck plug-in could block too: stop the whole probe.
        if (RThread().Id() != gMainThread)
            User::Exit(KErrTimedOut); // only this worker thread
        Log("  ICL decode of %s still running after %d s: stopping the probe\n", jpeg.name, kDecodeTimeoutMicroseconds / 1000000);
        fclose(gLog);
        User::Exit(KErrTimedOut);
    } else {
        timeout->Cancel();
        result.error = waiter->Status().Int();
    }
    delete timeout;
    delete waiter;
    delete wait;
    delete bitmap;
    delete image;
    return result;
}

struct Plugin {
    TUid uid;
    char name[64];
};

const int kMaxPlugins = 8;
Plugin gJpegPlugins[kMaxPlugins];
int gJpegPluginCount;

bool ContainsJpeg(const TDesC8& data)
{
    static const char* const needles[] = { "jpeg", "JPEG", "jpg", "JPG" };
    for (const char* needle : needles) {
        TPtrC8 key(reinterpret_cast<const TUint8*>(needle), strlen(needle));
        if (data.Find(key) != KErrNotFound)
            return true;
    }
    return false;
}

void DestroyImplementations(TAny* array)
{
    static_cast<RImplInfoPtrArray*>(array)->ResetAndDestroy();
}

void ListDecodersL()
{
    RImplInfoPtrArray implementations;
    CleanupStack::PushL(TCleanupItem(DestroyImplementations, &implementations));
    REComSession::ListImplementationsL(TUid::Uid(KDecoderInterfaceUidValue), implementations);
    Log("ICL image decoders: %d\n", implementations.Count());
    for (TInt i = 0; i < implementations.Count(); ++i) {
        CImplementationInformation* info = implementations[i];
        bool jpeg = ContainsJpeg(info->DataType()) || ContainsJpeg(info->OpaqueData()) || info->DisplayName().FindF(_L("jpeg")) != KErrNotFound;
        Log("%s 0x%08x v%d drive %d rom %d \"", jpeg ? "*" : " ", static_cast<unsigned>(info->ImplementationUid().iUid),
            info->Version(), static_cast<int>(info->Drive()), info->RomBased() ? 1 : 0);
        LogDes(info->DisplayName());
        Log("\" data \"");
        LogBytes(info->DataType());
        Log("\"\n    opaque \"");
        LogBytes(info->OpaqueData());
        Log("\"\n");
        if (jpeg) {
            RUidDataArray properties;
            TRAPD(error, CImageDecoder::GetPluginPropertiesL(info->ImplementationUid(), properties));
            Log("    properties (%d):", error);
            for (TInt p = 0; p < properties.Count(); ++p)
                Log(" 0x%08x", static_cast<unsigned>(properties[p].iUid));
            Log("\n");
            properties.Close();
            if (gJpegPluginCount < kMaxPlugins) {
                Plugin& plugin = gJpegPlugins[gJpegPluginCount++];
                plugin.uid = info->ImplementationUid();
                TBuf8<63> name;
                name.Copy(info->DisplayName().Left(63));
                memcpy(plugin.name, name.Ptr(), name.Length());
                plugin.name[name.Length()] = 0;
            }
        }
    }
    CleanupStack::PopAndDestroy(&implementations);
}

int Repeats(const Jpeg& jpeg)
{
    int pixels = jpeg.width * jpeg.height;
    int repeats = 3000000 / (pixels ? pixels : 1);
    return repeats < 2 ? 2 : repeats > 20 ? 20 : repeats;
}

void TimeLibjpeg(const Jpeg& jpeg, LibjpegMode mode)
{
    int repeats = Repeats(jpeg);
    double best = 1e9, total = 0;
    int width = 0, height = 0, done = 0;
    for (int i = 0; i < repeats && !PastDeadline(); ++i) {
        double milliseconds;
        if (!DecodeLibjpeg(jpeg, mode, milliseconds, width, height)) {
            Log("  %-40s failed\n", ModeName(mode));
            return;
        }
        best = milliseconds < best ? milliseconds : best;
        total += milliseconds;
        ++done;
    }
    if (done)
        Log("  %-40s %4dx%-4d best %7.1f ms  mean %7.1f ms  (%d runs)\n", ModeName(mode), width, height, best, total / done, done);
}

void TimeIcl(RFs& fs, const Jpeg& jpeg, TUid decoder, const char* decoderName, int reduction)
{
    int repeats = Repeats(jpeg);
    double bestConvert = 1e9, totalConvert = 0, totalCreate = 0;
    int done = 0;
    TUid used = KNullUid;
    for (int i = 0; i < repeats && !PastDeadline(); ++i) {
        IclResult result = DecodeIcl(fs, jpeg, decoder, reduction);
        if (result.error) {
            Log("  ICL %-24s 1/%d error %d (create %.1f ms, convert %.1f ms)\n", decoderName, reduction, result.error,
                result.createMilliseconds, result.convertMilliseconds);
            return;
        }
        used = result.implementation;
        bestConvert = result.convertMilliseconds < bestConvert ? result.convertMilliseconds : bestConvert;
        totalConvert += result.convertMilliseconds;
        totalCreate += result.createMilliseconds;
        ++done;
    }
    if (done)
        Log("  ICL %-24s 1/%d plugin 0x%08x  create %6.1f ms  convert best %7.1f ms  mean %7.1f ms  (%d runs)\n",
            decoderName, reduction, static_cast<unsigned>(used.iUid), totalCreate / done, bestConvert, totalConvert / done, done);
}

void TimeJpeg(RFs& fs, const Jpeg& jpeg)
{
    Log("\n%s: %lu bytes (%.2f bits/pixel)\n", jpeg.name, jpeg.size, jpeg.size * 8.0 / (jpeg.width * jpeg.height));
    TimeLibjpeg(jpeg, LibjpegMode::WebKit);
    TimeLibjpeg(jpeg, LibjpegMode::Fast);
    TimeLibjpeg(jpeg, LibjpegMode::Half);
    TimeLibjpeg(jpeg, LibjpegMode::Quarter);
    TimeIcl(fs, jpeg, KNullUid, "default", 1);
    TimeIcl(fs, jpeg, KNullUid, "default", 2);
    for (int i = 0; i < gJpegPluginCount && !PastDeadline(); ++i)
        TimeIcl(fs, jpeg, gJpegPlugins[i].uid, gJpegPlugins[i].name, 1);
    if (PastDeadline())
        Log("  stopped: past the %d s limit\n", kRunSeconds);
}

// Reads the size and kind from the SOF marker; false when it isn't a JPEG.
bool ParseJpeg(Jpeg& jpeg)
{
    const unsigned char* p = jpeg.data;
    unsigned long n = jpeg.size;
    if (n < 4 || p[0] != 0xff || p[1] != 0xd8)
        return false;
    unsigned long i = 2;
    while (i + 9 < n) {
        if (p[i] != 0xff) {
            ++i;
            continue;
        }
        unsigned char marker = p[i + 1];
        unsigned long length = p[i + 2] << 8 | p[i + 3];
        if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
            jpeg.height = p[i + 5] << 8 | p[i + 6];
            jpeg.width = p[i + 7] << 8 | p[i + 8];
            jpeg.progressive = marker == 0xc2 || marker == 0xc6 || marker == 0xca || marker == 0xce;
            return jpeg.width > 0 && jpeg.height > 0;
        }
        i += 2 + length;
    }
    return false;
}

int LoadUserJpegs(RFs& fs, Jpeg* jpegs, int maxCount)
{
    CDir* entries = nullptr;
    if (fs.GetDir(_L("C:\\Data\\jpegprobe\\*.jpg"), KEntryAttNormal, ESortByName, entries) != KErrNone || !entries)
        return 0;
    int count = 0;
    for (TInt i = 0; i < entries->Count() && count < maxCount; ++i) {
        const TEntry& entry = (*entries)[i];
        if (entry.iSize <= 0 || entry.iSize > 8 * 1024 * 1024)
            continue;
        TFileName path(_L("C:\\Data\\jpegprobe\\"));
        path.Append(entry.iName);
        RFile file;
        if (file.Open(fs, path, EFileRead | EFileShareReadersOnly) != KErrNone)
            continue;
        Jpeg& jpeg = jpegs[count];
        jpeg.data = static_cast<unsigned char*>(malloc(entry.iSize));
        TPtr8 buffer(jpeg.data, entry.iSize);
        TInt error = jpeg.data ? file.Read(buffer) : KErrNoMemory;
        file.Close();
        jpeg.size = buffer.Length();
        if (error || !ParseJpeg(jpeg) || jpeg.width * jpeg.height > 8000000) {
            free(jpeg.data);
            jpeg = Jpeg();
            continue;
        }
        TBuf8<24> name;
        name.Copy(entry.iName.Left(24));
        snprintf(jpeg.name, sizeof jpeg.name, "%.*s %dx%d %s", name.Length(), reinterpret_cast<const char*>(name.Ptr()),
            jpeg.width, jpeg.height, jpeg.progressive ? "progressive" : "baseline");
        ++count;
    }
    delete entries;
    return count;
}


// WebKit decodes images on worker threads. Repeat the ICL decode on worker
// threads, one at a time or at once behind one mutex, and report how each
// thread ended.
struct WorkerJob {
    const Jpeg* jpeg;
    RMutex* mutex;
    CImageDecoder::TOptions options;
    TInt error;
    int done;
};

TInt WorkerMain(TAny* argument)
{
    WorkerJob& job = *static_cast<WorkerJob*>(argument);
    CTrapCleanup* cleanup = CTrapCleanup::New();
    CActiveScheduler* scheduler = new CActiveScheduler;
    CActiveScheduler::Install(scheduler);
    RFs fs;
    job.error = fs.Connect();
    TInt fbs = job.error ? job.error : RFbsSession::Connect();
    if (!job.error)
        job.error = fbs;
    for (int i = 0; i < 10 && !job.error && !PastDeadline(); ++i) {
        if (job.mutex)
            job.mutex->Wait();
        job.error = DecodeIcl(fs, *job.jpeg, KNullUid, 1, job.options).error;
        if (job.mutex)
            job.mutex->Signal();
        if (!job.error)
            ++job.done;
    }
    if (!fbs)
        RFbsSession::Disconnect();
    fs.Close();
    CActiveScheduler::Install(nullptr);
    delete scheduler;
    delete cleanup;
    return 0;
}

void RunWorkers(const Jpeg& jpeg, int count, bool together, bool serialized, CImageDecoder::TOptions options = CImageDecoder::EOptionNone)
{
    static int serial;
    const int kMaxWorkers = 3;
    RThread threads[kMaxWorkers];
    TRequestStatus status[kMaxWorkers];
    WorkerJob jobs[kMaxWorkers];
    RMutex mutex;
    if (serialized && mutex.CreateLocal() != KErrNone)
        serialized = false;
    Log("\nICL on %d worker thread(s), %s%s, options 0x%x, %s:\n", count, together ? "at once" : "one at a time",
        serialized ? " behind one mutex" : "", options, jpeg.name);
    bool started[kMaxWorkers] = { };
    for (int i = 0; i < count; ++i) {
        jobs[i] = { &jpeg, serialized ? &mutex : nullptr, options, KErrNone, 0 };
        TBuf<16> name;
        name.Format(_L("iclworker%d"), ++serial);
        TInt error = threads[i].Create(name, WorkerMain, 0x10000, nullptr, &jobs[i]);
        if (error) {
            Log("  thread %d: create failed %d\n", i, error);
            continue;
        }
        started[i] = true;
        threads[i].Logon(status[i]);
        threads[i].Resume();
        if (!together) {
            // Waits for this thread before starting the next; at most 40 s.
            for (int t = 0; t < 400 && status[i] == KRequestPending; ++t)
                User::After(100000);
        }
    }
    for (int t = 0; t < 400; ++t) {
        bool running = false;
        for (int i = 0; i < count; ++i)
            running |= started[i] && status[i] == KRequestPending;
        if (!running)
            break;
        User::After(100000);
    }
    for (int i = 0; i < count; ++i) {
        if (!started[i])
            continue;
        if (status[i] == KRequestPending) {
            Log("  thread %d: still running after 40 s, killed (%d decodes)\n", i, jobs[i].done);
            threads[i].LogonCancel(status[i]);
            threads[i].Kill(KErrTimedOut);
            User::WaitForRequest(status[i]);
        } else {
            TExitCategoryName category = threads[i].ExitCategory();
            TBuf8<32> category8;
            category8.Copy(category.Left(31));
            Log("  thread %d: %d decodes, error %d, exit type %d %s %d\n", i, jobs[i].done, jobs[i].error,
                threads[i].ExitType(), reinterpret_cast<const char*>(category8.PtrZ()), threads[i].ExitReason());
        }
        threads[i].Close();
    }
    if (serialized)
        mutex.Close();
}

void RunL()
{
    RFs fs;
    User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);
    TInt error = RFbsSession::Connect();
    Log("RFbsSession::Connect %d\n", error);
    User::LeaveIfError(error);

    TRAP(error, ListDecodersL());
    if (error)
        Log("listing decoders failed: %d\n", error);

    Jpeg jpegs[12];
    int count = LoadUserJpegs(fs, jpegs, 6);
    Log("\n%d JPEGs from C:\\Data\\jpegprobe\n", count);
    const int sizes[][2] = { { 170, 96 }, { 640, 360 }, { 1280, 720 }, { 2048, 1536 } };
    for (auto& size : sizes) {
        for (int progressive = 0; progressive < 2; ++progressive) {
            if (count < 12 && Encode(jpegs[count], size[0], size[1], progressive))
                ++count;
            else
                Log("encoding %dx%d failed\n", size[0], size[1]);
        }
    }
    // WebKit decodes on worker threads.
    for (int i = 0; i < count && !PastDeadline(); ++i) {
        if (jpegs[i].width == 640 && !jpegs[i].progressive) {
            RunWorkers(jpegs[i], 1, false, false);
            RunWorkers(jpegs[i], 2, true, true);
            break;
        }
    }
    for (int i = 0; i < count && !PastDeadline(); ++i)
        TimeJpeg(fs, jpegs[i]);
    for (int i = 0; i < count; ++i)
        free(jpegs[i].data);

    RFbsSession::Disconnect();
    CleanupStack::PopAndDestroy(&fs);
}

}

int main()
{
    gLog = fopen("c:/data/jpegprobe.txt", "w");
    if (!gLog)
        return 1;
    HAL::Get(HALData::EFastCounterFrequency, gCounterFrequency);
    TInt tickPeriod = 1000;
    HAL::Get(HALData::ENanoTickPeriod, tickPeriod);
    gDeadline = User::NTickCount() + kRunSeconds * (1000000 / (tickPeriod > 0 ? tickPeriod : 1000));
    gMainThread = RThread().Id();
    Log("jpegprobe start, fast counter %d Hz\n", gCounterFrequency);

    CTrapCleanup* cleanup = CTrapCleanup::New();
    CActiveScheduler* scheduler = new CActiveScheduler;
    CActiveScheduler::Install(scheduler);
    TRAPD(error, RunL());
    Log("\ndone %d\n", error);
    delete scheduler;
    delete cleanup;
    REComSession::FinalClose();
    fclose(gLog);
    return 0;
}
