#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/private/SkMalloc.h"
#include <e32debug.h>
#include <cstdint>
#include <cstdio>
#include <sys/stat.h>

static FILE* output;
static unsigned checks;
static unsigned failures;

static void report(const char* line)
{
    TUint16 units[128];
    unsigned count = 0;
    while (line[count] && count < 128) {
        units[count] = static_cast<unsigned char>(line[count]);
        ++count;
    }
    RDebug::RawPrint(TPtrC16(units, count));
    if (output) {
        std::fprintf(output, "%s\n", line);
        std::fflush(output);
    }
}

static void expect(bool passed, const char* name)
{
    ++checks;
    failures += !passed;
    char line[128];
    std::snprintf(line, sizeof(line), "%s: %s", passed ? "PASS" : "FAIL", name);
    report(line);
}

int main()
{
    mkdir("C:/Data", 0777);
    mkdir("C:/Data/WebKit", 0777);
    output = std::fopen("C:/Data/WebKit/skia-probe.log", "w");
    void* allocation = sk_malloc_flags(64, 0);
    expect(allocation && sk_malloc_size(allocation, 64) >= 64, "allocation capacity");
    sk_free(allocation);
    static uint32_t pixels[64 * 64];
    auto info = SkImageInfo::Make(64, 64, kBGRA_8888_SkColorType,
        kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::WrapPixels(info, pixels, 64 * sizeof(uint32_t));
    expect(!!surface, "BGRA raster surface");
    if (surface) {
        auto* canvas = surface->getCanvas();
        canvas->clear(SK_ColorWHITE);
        SkPaint paint;
        paint.setColor(SK_ColorRED);
        canvas->drawRect(SkRect::MakeXYWH(8, 8, 16, 16), paint);
        expect(pixels[10 * 64 + 10] == 0xffff0000 && pixels[0] == 0xffffffff,
            "opaque rectangle pixels");
        canvas->save();
        canvas->clipRect(SkRect::MakeXYWH(32, 0, 16, 16));
        paint.setColor(SK_ColorGREEN);
        canvas->drawPaint(paint);
        canvas->restore();
        expect(pixels[5 * 64 + 35] == 0xff00ff00 && pixels[5 * 64 + 50] == 0xffffffff,
            "clipped drawing");
        paint.setColor(0x800000ff);
        canvas->drawRect(SkRect::MakeXYWH(8, 8, 16, 16), paint);
        uint32_t blended = pixels[10 * 64 + 10];
        expect((blended >> 24) == 255 && ((blended >> 16) & 255) == 127
            && (blended & 255) == 128, "alpha blending");
        paint.setColor(SK_ColorBLUE);
        canvas->drawCircle(48, 48, 8, paint);
        expect(pixels[48 * 64 + 48] == 0xff0000ff && pixels[60 * 64 + 60] == 0xffffffff,
            "path rasterization");
    }
    surface.reset();
    char result[128];
    std::snprintf(result, sizeof(result), "RESULT {\"probe\":\"skia-probe\",\"passed\":%u,\"failed\":%u}",
        checks - failures, failures);
    report(result);
    report(failures ? "FAIL: skia-probe" : "PASS: skia-probe");
    std::snprintf(result, sizeof(result), "skia-probe exiting %u", failures ? 1 : 0);
    report(result);
    if (output)
        std::fclose(output);
    return failures ? 1 : 0;
}
