#undef ASSERT
#include "config.h"
#include "Color.h"
#include "Damage.h"
#include "FilterOperation.h"
#include "FilterOperations.h"
#include "GraphicsContext.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "GLContext.h"
#include "ImageBuffer.h"
#include "PlatformDisplay.h"
#include "PlatformDisplaySymbian.h"
#include "SkiaCompositingLayer.h"
#include "TextureMapperPlatformLayer.h"
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImageInfo.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkSurface.h>
#include <skia/gpu/ganesh/GrDirectContext.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <wtf/MainThread.h>
#include <e32debug.h>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

using namespace WebCore;

static FILE* output;
static unsigned checks;
static unsigned failures;
static uint32_t pixels[160 * 120];
static uint32_t previousPixels[160 * 120];

static void report(const char* line)
{
    TUint16 units[256];
    unsigned count = 0;
    while (line[count] && count < 256) {
        units[count] = static_cast<unsigned char>(line[count]);
        ++count;
    }
    RDebug::RawPrint(TPtrC16(units, count));
    if (output) {
        std::fprintf(output, "%s\n", line);
        std::fflush(output);
    }
}

static bool expect(bool passed, const char* name)
{
    ++checks;
    failures += !passed;
    char line[256];
    std::snprintf(line, sizeof(line), "%s: %s", passed ? "PASS" : "FAIL", name);
    report(line);
    return passed;
}

static Ref<SkiaCompositingLayer> layer(const FloatSize& size, const FloatPoint& position, const Color& color)
{
    auto result = SkiaCompositingLayer::create();
    result->setSize(size);
    result->setPosition(position);
    result->setAnchorPoint({ 0, 0, 0 });
    result->setContentsRect({ { }, size });
    result->setContentsSolidColor(color);
    result->setContentsVisible(true);
    result->setDamagePropagationEnabled(true);
    return result;
}

static sk_sp<SkSurface> target(GrDirectContext& context, const IntSize& size)
{
    auto info = SkImageInfo::Make(size.width(), size.height(), kBGRA_8888_SkColorType,
        kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    return SkSurfaces::RenderTarget(&context, skgpu::Budgeted::kYes, info,
        0, kTopLeft_GrSurfaceOrigin, nullptr);
}

static std::optional<Damage> compose(SkiaCompositingLayer& root, SkSurface& surface, GrDirectContext& context, bool fresh = false)
{
    IntSize size { surface.width(), surface.height() };
    std::optional<Damage> frame { std::in_place, size };
    std::optional<Damage> prior;
    if (!fresh)
        prior.emplace(size);
    root.paint(*surface.getCanvas(), frame, prior, SK_ColorWHITE);
    context.flushAndSubmit(&surface, GrSyncCpu::kYes);
    return frame;
}

static bool read(SkSurface& surface)
{
    auto info = SkImageInfo::Make(surface.width(), surface.height(),
        kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    return surface.readPixels(info, pixels, surface.width() * sizeof(uint32_t), 0, 0);
}

static uint32_t pixel(const SkSurface& surface, int x, int y)
{
    return pixels[y * surface.width() + x];
}

static bool near(uint32_t actual, uint32_t expected)
{
    for (unsigned shift : { 0, 8, 16, 24 }) {
        if (std::abs(int((actual >> shift) & 255) - int((expected >> shift) & 255)) > 1)
            return false;
    }
    return true;
}

static bool canvasDisplayDelegatePixels(GrDirectContext& context)
{
    auto buffer = ImageBuffer::create({ 256, 192 }, RenderingMode::Accelerated,
        RenderingPurpose::Canvas, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer || buffer->renderingMode() != RenderingMode::Accelerated)
        return false;
    buffer->context().fillRect({ 0, 0, 256, 96 }, Color::red);
    buffer->context().fillRect({ 0, 96, 256, 96 }, Color::blue);
    buffer->prepareForDisplay();
    auto delegate = buffer->layerContentsDisplayDelegate();
    if (!delegate || !delegate->platformLayer())
        return false;
    auto image = delegate->platformLayer()->skiaImage();
    auto pixelInfo = SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    auto imagePixel = [&](const sk_sp<SkImage>& candidate, int y, uint32_t expected) {
        uint32_t actual = 0;
        return candidate && candidate->isTextureBacked()
            && candidate->readPixels(&context, pixelInfo, &actual, sizeof(actual), 64, y)
            && actual == expected;
    };
    if (!imagePixel(image, 16, SK_ColorRED) || !imagePixel(image, 160, SK_ColorBLUE))
        return false;
    buffer->context().fillRect({ 0, 96, 256, 96 }, Color::green);
    buffer->prepareForDisplay();
    image = delegate->platformLayer()->skiaImage();
    return imagePixel(image, 16, SK_ColorRED) && imagePixel(image, 160, SK_ColorGREEN);
}

static void run(GrDirectContext& context)
{
    const IntSize size { 128, 96 };
    auto surface = target(context, size);
    if (!surface) {
        expect(false, "native GPU composition target");
        return;
    }
    auto root = layer(FloatSize(size), { }, Color::white);
    auto moving = layer({ 24, 20 }, { 8, 8 }, Color::red);
    auto fixed = layer({ 112, 16 }, { 8, 72 }, Color::blue);
    root->setChildren({ moving.copyRef(), fixed.copyRef() });
    compose(root, *surface, context, true);
    expect(surface->recordingContext() == &context && read(*surface) && pixel(*surface, 12, 12) == SK_ColorRED
        && pixel(*surface, 20, 80) == SK_ColorBLUE && pixel(*surface, 100, 20) == SK_ColorWHITE,
        "upstream Skia layer composition pixels");

    SkPaint sentinel;
    sentinel.setColor(SK_ColorMAGENTA);
    surface->getCanvas()->drawRect(SkRect::MakeXYWH(120, 0, 8, 8), sentinel);
    context.flushAndSubmit(surface.get(), GrSyncCpu::kYes);
    moving->setPosition({ 48, 8 });
    auto moved = compose(root, *surface, context);
    expect(read(*surface) && moved && !moved->isEmpty()
        && moved->bounds().width() * moved->bounds().height() < size.width() * size.height()
        && pixel(*surface, 12, 12) == SK_ColorWHITE && pixel(*surface, 52, 12) == SK_ColorRED,
        "partial damage repairs moved layer bounds");
    expect(pixel(*surface, 124, 4) == SK_ColorMAGENTA && pixel(*surface, 20, 80) == SK_ColorBLUE,
        "undamaged target pixels are preserved");

    std::memcpy(previousPixels, pixels, size.width() * size.height() * sizeof(uint32_t));
    auto idle = compose(root, *surface, context);
    expect(read(*surface) && idle && idle->isEmpty()
        && !std::memcmp(previousPixels, pixels, size.width() * size.height() * sizeof(uint32_t)),
        "static frame has empty damage and unchanged pixels");

    root->setChildren({ fixed.copyRef() });
    auto removed = compose(root, *surface, context);
    expect(read(*surface) && removed && !removed->isEmpty()
        && pixel(*surface, 52, 12) == SK_ColorWHITE && pixel(*surface, 124, 4) == SK_ColorMAGENTA,
        "removed layer repairs its previous bounds");

    moving->setPosition({ 8, 8 });
    moving->setOpacity(0.5);
    root->setChildren({ moving.copyRef(), fixed.copyRef() });
    compose(root, *surface, context);
    expect(read(*surface) && near(pixel(*surface, 12, 12), 0xffff8080), "layer opacity pixels");

    auto clipped = layer({ 20, 20 }, { 80, 8 }, Color::transparentBlack);
    auto clippedChild = layer({ 32, 16 }, { 12, 0 }, Color::green);
    clipped->setMasksToBounds(true);
    clipped->setChildren({ clippedChild.copyRef() });
    root->setChildren({ moving.copyRef(), fixed.copyRef(), clipped.copyRef() });
    compose(root, *surface, context);
    expect(read(*surface) && pixel(*surface, 96, 12) == SK_ColorGREEN
        && pixel(*surface, 108, 12) == SK_ColorWHITE, "layer clipping pixels");

    auto masked = layer({ 24, 20 }, { 80, 40 }, Color::green);
    auto mask = layer({ 12, 20 }, { }, Color::white);
    mask->setClipPath(SkPath::Rect(SkRect::MakeWH(12, 20)));
    masked->setMask(RefPtr<SkiaCompositingLayer> { mask.ptr() });
    root->setChildren({ moving.copyRef(), fixed.copyRef(), clipped.copyRef(), masked.copyRef() });
    compose(root, *surface, context);
    expect(read(*surface) && pixel(*surface, 84, 44) == SK_ColorGREEN
        && pixel(*surface, 100, 44) == SK_ColorWHITE, "layer path mask pixels");

    auto filtered = layer({ 24, 20 }, { 8, 40 }, Color::green);
    FilterOperations filters { { BasicComponentTransferFilterOperation::create(0.5, FilterOperation::Type::Brightness) } };
    filtered->setFilters(filters);
    root->setChildren({ moving.copyRef(), fixed.copyRef(), clipped.copyRef(), masked.copyRef(), filtered.copyRef() });
    compose(root, *surface, context);
    expect(read(*surface) && near(pixel(*surface, 12, 44), 0xff008000), "layer filter pixels");

    auto depth = layer({ 40, 24 }, { 40, 40 }, Color::transparentBlack);
    auto back = layer({ 24, 20 }, { }, Color::blue);
    auto front = layer({ 24, 20 }, { 8, 0 }, Color::green);
    TransformationMatrix backTransform;
    backTransform.translate3d(0, 0, -10);
    back->setTransform(backTransform);
    TransformationMatrix frontTransform;
    frontTransform.translate3d(0, 0, 10);
    front->setTransform(frontTransform);
    depth->setPreserves3D(true);
    depth->setChildren({ front.copyRef(), back.copyRef() });
    root->setChildren({ moving.copyRef(), fixed.copyRef(), clipped.copyRef(), masked.copyRef(), filtered.copyRef(), depth.copyRef() });
    compose(root, *surface, context);
    expect(read(*surface) && pixel(*surface, 52, 44) == SK_ColorGREEN
        && pixel(*surface, 42, 44) == SK_ColorBLUE, "preserve 3D composition pixels");

    auto lower = layer({ 24, 20 }, { 40, 8 }, Color::blue);
    auto upper = layer({ 24, 20 }, { 40, 8 }, Color::green);
    root->setChildren({ moving.copyRef(), fixed.copyRef(), clipped.copyRef(), masked.copyRef(), filtered.copyRef(), depth.copyRef(), lower.copyRef(), upper.copyRef() });
    compose(root, *surface, context);
    const bool initialOrder = read(*surface) && pixel(*surface, 44, 12) == SK_ColorGREEN;
    root->setChildren({ moving.copyRef(), fixed.copyRef(), clipped.copyRef(), masked.copyRef(), filtered.copyRef(), depth.copyRef(), upper.copyRef(), lower.copyRef() });
    auto reordered = compose(root, *surface, context);
    expect(initialOrder && read(*surface) && pixel(*surface, 44, 12) == SK_ColorBLUE
        && reordered && !reordered->isEmpty()
        && reordered->bounds().width() * reordered->bounds().height() < size.width() * size.height(),
        "same bounds child reorder damages overlap");

    const IntSize resized { 160, 120 };
    surface = target(context, resized);
    if (!surface) {
        expect(false, "resized target full repaint");
        return;
    }
    surface->getCanvas()->clear(SK_ColorMAGENTA);
    root->setSize(FloatSize(resized));
    root->setContentsRect({ { }, FloatSize(resized) });
    root->setChildren({ fixed.copyRef() });
    compose(root, *surface, context, true);
    expect(read(*surface) && pixel(*surface, 150, 110) == SK_ColorWHITE
        && pixel(*surface, 124, 4) == SK_ColorWHITE && pixel(*surface, 20, 80) == SK_ColorBLUE,
        "resized target full repaint");
    expect(canvasDisplayDelegatePixels(context), "accelerated Canvas display delegate pixels");
}

int main()
{
    mkdir("C:/Data", 0777);
    mkdir("C:/Data/WebKit", 0777);
    output = std::fopen("C:/Data/WebKit/skia-compositor-probe.log", "w");
    WTF::initializeMainThread();
    auto display = PlatformDisplaySymbian::create();
    if (display) {
        PlatformDisplay::setSharedDisplay(WTF::move(display));
        auto* gl = PlatformDisplay::sharedDisplay().skiaGLContext();
        auto* context = gl ? PlatformDisplay::sharedDisplay().skiaGrContext() : nullptr;
        if (expect(gl && context && gl->makeContextCurrent(), "native Ganesh GLES2 context"))
            run(*context);
        PlatformDisplay::destroySharedDisplay();
    } else
        expect(false, "native Ganesh GLES2 context");
    char result[256];
    std::snprintf(result, sizeof(result), "RESULT {\"probe\":\"skia-compositor-probe\",\"passed\":%u,\"failed\":%u}",
        checks - failures, failures);
    report(result);
    report(failures ? "FAIL: skia-compositor-probe" : "PASS: skia-compositor-probe");
    std::snprintf(result, sizeof(result), "skia-compositor-probe exiting %u", failures ? 1 : 0);
    report(result);
    if (output)
        std::fclose(output);
    return failures ? 1 : 0;
}
