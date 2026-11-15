// Reports what the phone's EGL and GLES2 driver gives Skia: configurations,
// the GL interface Ganesh assembles, Ganesh on offscreen and window contexts,
// and one context drawing into both a pbuffer and the window.
#include "window.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"
#include "src/gpu/ganesh/gl/GrGLCoreFunctions.h"
#include "src/gpu/ganesh/gl/GrGLDefines.h"
#include "src/gpu/ganesh/gl/GrGLUtil.h"
#include <EGL/egl.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GLES2/gl2.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

static FILE* output;

static void report(const char* format, ...)
{
    if (!output)
        return;
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(output, format, arguments);
    va_end(arguments);
    std::fputc('\n', output);
    std::fflush(output);
}

// Long strings in lines the log viewer on the phone can show.
static void reportList(const char* label, const char* list)
{
    if (!list) {
        report("%s: null", label);
        return;
    }
    std::string line;
    unsigned index = 0;
    for (const char* cursor = list; ; ) {
        size_t length = std::strcspn(cursor, " ");
        if (length) {
            if (line.size() + length > 180) {
                report("%s[%u]: %s", label, index++, line.c_str());
                line.clear();
            }
            line.append(cursor, length);
            line.push_back(' ');
        }
        cursor += length;
        if (!*cursor)
            break;
        ++cursor;
    }
    report("%s[%u]: %s", label, index, line.c_str());
}

// A line of validate.inc; the same requirement is 76 lines further down GrGLInterfaceAutogen.cpp.
static unsigned validateFailureLine;

struct ProbeInterface : GrGLInterface {
    bool probeValidate() const;
};

#define RETURN_FALSE_INTERFACE \
    do { validateFailureLine = __LINE__; return false; } while (0)
#include "validate.inc"
#undef RETURN_FALSE_INTERFACE

static std::string missingNames;
static std::string foundNames;

static GrGLFuncPtr lookUp(void*, const char name[])
{
#define M(X) if (!std::strcmp(#X, name)) return reinterpret_cast<GrGLFuncPtr>(X);
    GR_GL_CORE_FUNCTIONS_EACH(M)
#undef M
    auto function = reinterpret_cast<GrGLFuncPtr>(eglGetProcAddress(name));
    auto& names = function ? foundNames : missingNames;
    names.append(name);
    names.push_back(' ');
    return function;
}

static EGLDisplay display;

static int attribute(EGLConfig config, EGLint name)
{
    EGLint value = -1;
    eglGetConfigAttrib(display, config, name, &value);
    return value;
}

// As GLContext::getEGLConfig chooses: the first RGBA8888 configuration with a stencil buffer.
static EGLConfig choose(EGLint surfaceType, const char* label)
{
    const EGLint attributes[] { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_SURFACE_TYPE, surfaceType, EGL_DEPTH_SIZE, 0, EGL_NONE };
    EGLConfig configs[64];
    EGLint count = 0;
    if (!eglChooseConfig(display, attributes, configs, 64, &count) || !count) {
        report("choose %s: none (EGL error 0x%x)", label, eglGetError());
        return { };
    }
    for (EGLint i = 0; i < count; ++i) {
        if (attribute(configs[i], EGL_RED_SIZE) == 8 && attribute(configs[i], EGL_GREEN_SIZE) == 8
            && attribute(configs[i], EGL_BLUE_SIZE) == 8 && attribute(configs[i], EGL_ALPHA_SIZE) == 8) {
            report("choose %s: configuration %d of %d candidates", label, attribute(configs[i], EGL_CONFIG_ID), count);
            return configs[i];
        }
    }
    report("choose %s: %d candidates, none RGBA8888", label, count);
    return { };
}

static void listConfigurations()
{
    EGLConfig configs[160];
    EGLint count = 0;
    eglGetConfigs(display, configs, 160, &count);
    report("configurations: %d", count);
    for (EGLint i = 0; i < count; ++i) {
        auto config = configs[i];
        if (!(attribute(config, EGL_RENDERABLE_TYPE) & EGL_OPENGL_ES2_BIT))
            continue;
        report("config %d: rgba=%d%d%d%d depth=%d stencil=%d surface=0x%x renderable=0x%x caveat=0x%x samples=%d native=%d",
            attribute(config, EGL_CONFIG_ID), attribute(config, EGL_RED_SIZE), attribute(config, EGL_GREEN_SIZE),
            attribute(config, EGL_BLUE_SIZE), attribute(config, EGL_ALPHA_SIZE), attribute(config, EGL_DEPTH_SIZE),
            attribute(config, EGL_STENCIL_SIZE), attribute(config, EGL_SURFACE_TYPE), attribute(config, EGL_RENDERABLE_TYPE),
            attribute(config, EGL_CONFIG_CAVEAT), attribute(config, EGL_SAMPLES), attribute(config, EGL_NATIVE_RENDERABLE));
    }
}

static void reportPrecision(const char* label, GLenum shader, GLenum type)
{
    GLint range[2] { -1, -1 };
    GLint precision = -1;
    glGetShaderPrecisionFormat(shader, type, range, &precision);
    report("precision %s: range=%d,%d precision=%d (GL error 0x%x)", label, range[0], range[1], precision, glGetError());
}

static void reportDriver()
{
    report("GL_VENDOR: %s", glGetString(GL_VENDOR));
    report("GL_RENDERER: %s", glGetString(GL_RENDERER));
    report("GL_VERSION: %s", glGetString(GL_VERSION));
    report("GL_SHADING_LANGUAGE_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    reportList("GL_EXTENSIONS", reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
    GLint value = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    report("GL_MAX_TEXTURE_SIZE: %d", value);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &value);
    report("GL_MAX_RENDERBUFFER_SIZE: %d", value);
    reportPrecision("fragment highp float", GL_FRAGMENT_SHADER, GL_HIGH_FLOAT);
    reportPrecision("fragment mediump float", GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT);
    reportPrecision("fragment highp int", GL_FRAGMENT_SHADER, GL_HIGH_INT);
    reportPrecision("vertex highp float", GL_VERTEX_SHADER, GL_HIGH_FLOAT);
}

static void reportLookups()
{
    static const char* const names[] { "glGetString", "glMapBufferOES", "glEGLImageTargetTexture2DOES",
        "eglCreateImageKHR", "eglDestroyImageKHR", "eglCreateSyncKHR", "eglClientWaitSyncKHR", "eglDestroySyncKHR",
        "eglQueryString", "eglGetCurrentDisplay", "glDiscardFramebufferEXT", "glNoSuchFunction" };
    std::string line;
    for (auto* name : names) {
        char entry[96];
        std::snprintf(entry, sizeof(entry), "%s=%p ", name, reinterpret_cast<void*>(eglGetProcAddress(name)));
        line.append(entry);
        if (line.size() > 150) {
            report("eglGetProcAddress: %s", line.c_str());
            line.clear();
        }
    }
    if (!line.empty())
        report("eglGetProcAddress: %s", line.c_str());
}

static sk_sp<GrDirectContext> startGanesh(const char* label)
{
    missingNames.clear();
    foundNames.clear();
    validateFailureLine = 0;
    auto interface = GrGLMakeAssembledInterface(nullptr, lookUp);
    reportList("found through eglGetProcAddress", foundNames.c_str());
    reportList("missing", missingNames.c_str());
    if (!interface) {
        report("%s: Skia assembles no GL interface (GL error 0x%x)", label, glGetError());
        return nullptr;
    }
    bool valid = static_cast<const ProbeInterface*>(interface.get())->probeValidate();
    report("%s: interface standard=%d extensions-initialized=%d version=0x%x validate=%d probe-validate=%d failing-line=%u",
        label, static_cast<int>(interface->fStandard), interface->fExtensions.isInitialized(),
        static_cast<unsigned>(GrGLGetVersion(interface.get())), interface->validate(), valid, validateFailureLine);

    auto shipped = GrGLInterfaces::MakeEGL();
    report("%s: MakeEGL interface=%d validate=%d", label, !!shipped, shipped && shipped->validate());

    GrContextOptions options;
    options.fSharpenMipmappedTextures = false;
    auto context = GrDirectContexts::MakeGL(interface, options);
    if (!context) {
        report("%s: Ganesh does not start (GL error 0x%x)", label, glGetError());
        return nullptr;
    }
    report("%s: Ganesh started, texture=%d render-target=%d RGBA=%d BGRA=%d GL-error=0x%x", label,
        context->maxTextureSize(), context->maxRenderTargetSize(),
        context->colorTypeSupportedAsSurface(kRGBA_8888_SkColorType),
        context->colorTypeSupportedAsSurface(kBGRA_8888_SkColorType), glGetError());
    return context;
}

static void drawOffscreen(GrDirectContext* context, const char* label, SkColorType colorType, const char* colorName)
{
    if (!context->colorTypeSupportedAsSurface(colorType)) {
        report("%s: %s is not a surface colour type", label, colorName);
        return;
    }
    auto info = SkImageInfo::Make(64, 64, colorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::RenderTarget(context, skgpu::Budgeted::kYes, info, 0, kTopLeft_GrSurfaceOrigin, nullptr);
    if (!surface) {
        report("%s: no %s render target (GL error 0x%x)", label, colorName, glGetError());
        return;
    }
    auto* canvas = surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    canvas->drawRect(SkRect::MakeXYWH(8, 8, 16, 16), paint);
    paint.setColor(SK_ColorBLUE);
    paint.setAntiAlias(true);
    canvas->drawCircle(40, 40, 8, paint);
    context->flushAndSubmit(surface.get(), GrSyncCpu::kYes);
    static uint32_t pixels[64 * 64];
    auto readInfo = SkImageInfo::Make(64, 64, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    bool read = surface->readPixels(readInfo, pixels, 64 * sizeof(uint32_t), 0, 0);
    report("%s: %s render target read=%d clear=%08x rect=%08x circle=%08x (expect ffffffff ffff0000 ff0000ff) GL-error=0x%x",
        label, colorName, read, pixels[0], pixels[12 * 64 + 12], pixels[40 * 64 + 40], glGetError());
}

// Skia drawing into the window's own framebuffer, as the single context compositor does.
static void drawWindow(GrDirectContext* context, const char* label, EGLSurface surface, int width, int height)
{
    context->resetContext();
    GrGLFramebufferInfo framebuffer;
    framebuffer.fFBOID = 0;
    bool useBGRA = context->colorTypeSupportedAsSurface(kBGRA_8888_SkColorType);
    framebuffer.fFormat = useBGRA ? 0x93A1 : 0x8058;
    auto target = GrBackendRenderTargets::MakeGL(width, height, 0, 8, framebuffer);
    auto window = SkSurfaces::WrapBackendRenderTarget(context, target, kBottomLeft_GrSurfaceOrigin,
        useBGRA ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
    if (!window) {
        report("%s: Skia does not wrap the window (BGRA=%d)", label, useBGRA);
        return;
    }
    for (unsigned frame = 0; frame < 30; ++frame) {
        auto* canvas = window->getCanvas();
        canvas->clear(SK_ColorWHITE);
        SkPaint paint;
        paint.setColor(SK_ColorRED);
        canvas->drawRect(SkRect::MakeXYWH(20 + frame * 4, 40, 120, 120), paint);
        paint.setColor(SK_ColorBLUE);
        paint.setAntiAlias(true);
        canvas->drawCircle(width / 2, height / 2, 60, paint);
        context->flushAndSubmit(window.get(), GrSyncCpu::kNo);
        uint8_t pixel[4] {};
        if (!frame) {
            glReadPixels(30, height - 60, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            report("%s: window pixel in the rectangle rgba=%02x%02x%02x%02x (red is ff0000ff) GL-error=0x%x",
                label, pixel[0], pixel[1], pixel[2], pixel[3], glGetError());
        }
        if (!eglSwapBuffers(display, surface)) {
            report("%s: eglSwapBuffers fails on frame %u (EGL error 0x%x)", label, frame, eglGetError());
            return;
        }
        probePumpWindow(40);
    }
    report("%s: 30 frames drawn by Skia into the window; a red square moves over a blue circle on white", label);
}

static EGLSurface createWindowSurface(EGLConfig config, void* nativeWindow, const char* label)
{
    const EGLint premultiplied[] { EGL_VG_ALPHA_FORMAT, EGL_VG_ALPHA_FORMAT_PRE, EGL_NONE };
    auto surface = eglCreateWindowSurface(display, config, static_cast<EGLNativeWindowType>(nativeWindow), premultiplied);
    if (surface == EGL_NO_SURFACE) {
        report("%s: no premultiplied window surface (EGL error 0x%x)", label, eglGetError());
        surface = eglCreateWindowSurface(display, config, static_cast<EGLNativeWindowType>(nativeWindow), nullptr);
    }
    if (surface == EGL_NO_SURFACE)
        report("%s: no window surface (EGL error 0x%x)", label, eglGetError());
    return surface;
}

// An offscreen context as WebKit makes it, then the window given to the same context.
static void runOffscreen(const char* label, EGLConfig config, EGLConfig windowConfig, void* nativeWindow, int width, int height, bool describe)
{
    if (!config) {
        report("%s: skipped, no configuration", label);
        return;
    }
    report("--- %s: configuration %d, surface types 0x%x", label, attribute(config, EGL_CONFIG_ID), attribute(config, EGL_SURFACE_TYPE));
    const EGLint contextAttributes[] { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    const EGLint pbufferAttributes[] { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    auto eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (eglContext == EGL_NO_CONTEXT) {
        report("%s: no context (EGL error 0x%x)", label, eglGetError());
        return;
    }
    auto pbuffer = eglCreatePbufferSurface(display, config, pbufferAttributes);
    if (pbuffer == EGL_NO_SURFACE)
        report("%s: no pbuffer (EGL error 0x%x)", label, eglGetError());
    else if (!eglMakeCurrent(display, pbuffer, pbuffer, eglContext))
        report("%s: the context is not current on its pbuffer (EGL error 0x%x)", label, eglGetError());
    else {
        if (describe) {
            reportDriver();
            reportLookups();
        }
        auto context = startGanesh(label);
        if (context) {
            drawOffscreen(context.get(), label, kRGBA_8888_SkColorType, "RGBA");
            drawOffscreen(context.get(), label, kBGRA_8888_SkColorType, "BGRA");
        }
        if (nativeWindow && windowConfig) {
            report("%s: window surface in configuration %d", label, attribute(windowConfig, EGL_CONFIG_ID));
            auto window = createWindowSurface(windowConfig, nativeWindow, label);
            if (window != EGL_NO_SURFACE) {
                if (!eglMakeCurrent(display, window, window, eglContext))
                    report("%s: the context does not take the window surface (EGL error 0x%x)", label, eglGetError());
                else {
                    report("%s: the context is current on the window surface", label);
                    if (context)
                        drawWindow(context.get(), label, window, width, height);
                    else {
                        glViewport(0, 0, width, height);
                        glClearColor(0, 1, 0, 1);
                        glClear(GL_COLOR_BUFFER_BIT);
                        bool swapped = eglSwapBuffers(display, window);
                        report("%s: plain GL clear to green, swap=%d (EGL error 0x%x, GL error 0x%x)", label, swapped, eglGetError(), glGetError());
                        probePumpWindow(1000);
                    }
                    if (!eglMakeCurrent(display, pbuffer, pbuffer, eglContext))
                        report("%s: the context does not go back to its pbuffer (EGL error 0x%x)", label, eglGetError());
                    else if (context)
                        drawOffscreen(context.get(), label, kBGRA_8888_SkColorType, "BGRA after the window");
                }
            }
            if (context) {
                context->abandonContext();
                context = nullptr;
            }
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (window != EGL_NO_SURFACE)
                eglDestroySurface(display, window);
        }
        if (context)
            context->abandonContext();
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (pbuffer != EGL_NO_SURFACE)
        eglDestroySurface(display, pbuffer);
    eglDestroyContext(display, eglContext);
}

// A context made for the window, as the compositor's own.
static void runWindow(EGLConfig config, void* nativeWindow, int width, int height)
{
    const char* label = "window context";
    if (!config || !nativeWindow) {
        report("%s: skipped", label);
        return;
    }
    report("--- %s: configuration %d, surface types 0x%x", label, attribute(config, EGL_CONFIG_ID), attribute(config, EGL_SURFACE_TYPE));
    const EGLint contextAttributes[] { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    auto eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (eglContext == EGL_NO_CONTEXT) {
        report("%s: no context (EGL error 0x%x)", label, eglGetError());
        return;
    }
    auto window = createWindowSurface(config, nativeWindow, label);
    if (window != EGL_NO_SURFACE) {
        if (!eglMakeCurrent(display, window, window, eglContext))
            report("%s: the context is not current on the window (EGL error 0x%x)", label, eglGetError());
        else {
            EGLint swapBehavior = 0;
            eglQuerySurface(display, window, EGL_SWAP_BEHAVIOR, &swapBehavior);
            report("%s: swap behavior 0x%x", label, swapBehavior);
            auto context = startGanesh(label);
            if (context) {
                drawOffscreen(context.get(), label, kBGRA_8888_SkColorType, "BGRA");
                drawWindow(context.get(), label, window, width, height);
                context->abandonContext();
            }
        }
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display, window);
    }
    eglDestroyContext(display, eglContext);
}

int main()
{
    mkdir("C:/Data", 0777);
    mkdir("C:/Data/WebKit", 0777);
    output = std::fopen("C:/Data/WebKit/glprobe.log", "w");
    report("glprobe started");
    int width = 0;
    int height = 0;
    void* nativeWindow = probeOpenWindow(&width, &height);
    report("native window: %p, %dx%d", nativeWindow, width, height);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0;
    EGLint minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor))
        report("no EGL display (EGL error 0x%x)", eglGetError());
    else {
        eglBindAPI(EGL_OPENGL_ES_API);
        report("EGL %d.%d, vendor %s, version %s, client APIs %s", major, minor, eglQueryString(display, EGL_VENDOR),
            eglQueryString(display, EGL_VERSION), eglQueryString(display, EGL_CLIENT_APIS));
        reportList("EGL_EXTENSIONS", eglQueryString(display, EGL_EXTENSIONS));
        listConfigurations();
        EGLConfig pbufferConfig = choose(EGL_PBUFFER_BIT, "pbuffer");
        EGLConfig windowConfig = choose(EGL_WINDOW_BIT, "window");
        EGLConfig bothConfig = choose(EGL_PBUFFER_BIT | EGL_WINDOW_BIT, "pbuffer and window");
        runOffscreen("pbuffer context", pbufferConfig, windowConfig, nativeWindow, width, height, true);
        runOffscreen("pbuffer and window context", bothConfig, bothConfig, nativeWindow, width, height, false);
        runWindow(windowConfig, nativeWindow, width, height);
        eglTerminate(display);
    }
    probeCloseWindow();
    report("glprobe finished");
    if (output)
        std::fclose(output);
    return 0;
}
