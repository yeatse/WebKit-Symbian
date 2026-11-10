#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <e32debug.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

static FILE* output;
static unsigned checks;
static unsigned failures;

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

static bool checkContextState(EGLDisplay display, EGLConfig config, EGLSurface primarySurface, EGLContext primaryContext)
{
    GLint initialViewport[4] {};
    GLint initialScissor[4] {};
    glGetIntegerv(GL_VIEWPORT, initialViewport);
    glGetIntegerv(GL_SCISSOR_BOX, initialScissor);
    const GLint fullSurface[] { 0, 0, 128, 96 };
    expect(!std::memcmp(initialViewport, fullSurface, sizeof(fullSurface))
        && !std::memcmp(initialScissor, fullSurface, sizeof(fullSurface)),
        "first current viewport and scissor match surface");

    const GLint viewport[] { 3, 5, 73, 41 };
    const GLint scissor[] { 7, 9, 61, 29 };
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
    const EGLint contextAttributes[] { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    const EGLint surfaceAttributes[] { EGL_WIDTH, 32, EGL_HEIGHT, 24, EGL_NONE };
    auto secondary = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    auto surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    const bool switched = secondary != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE
        && eglMakeCurrent(display, surface, surface, secondary) == EGL_TRUE;
    if (switched) {
        glViewport(1, 2, 12, 13);
        glScissor(2, 3, 7, 5);
    }
    const bool restored = eglMakeCurrent(display, primarySurface, primarySurface, primaryContext) == EGL_TRUE;
    GLint actualViewport[4] {};
    GLint actualScissor[4] {};
    if (restored) {
        glGetIntegerv(GL_VIEWPORT, actualViewport);
        glGetIntegerv(GL_SCISSOR_BOX, actualScissor);
    }
    expect(switched && restored && !std::memcmp(actualViewport, viewport, sizeof(viewport))
        && !std::memcmp(actualScissor, scissor, sizeof(scissor)),
        "context switches retain viewport and scissor");
    if (restored) {
        glViewport(initialViewport[0], initialViewport[1], initialViewport[2], initialViewport[3]);
        glScissor(initialScissor[0], initialScissor[1], initialScissor[2], initialScissor[3]);
    }
    if (secondary != EGL_NO_CONTEXT)
        eglDestroyContext(display, secondary);
    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
    return restored;
}

static void drawAndCheck(EGLDisplay display, EGLConfig config, EGLSurface pbuffer, EGLContext eglContext)
{
    auto interface = GrGLInterfaces::MakeEGL();
    if (!expect(interface && interface->validate() && interface->fStandard == kGLES_GrGLStandard,
        "native GLES2 Ganesh interface"))
        return;

    GrContextOptions options;
    options.fGlyphCacheTextureMaximumBytes = 2 * 1024 * 1024;
    options.fSharpenMipmappedTextures = false;
    auto context = GrDirectContexts::MakeGL(interface, options);
    if (!expect(!!context, "Ganesh direct context"))
        return;
    char capabilities[256];
    std::snprintf(capabilities, sizeof(capabilities),
        "INFO: texture=%d render-target=%d RGBA=%d BGRA=%d GL-error=0x%x",
        context->maxTextureSize(), context->maxRenderTargetSize(),
        context->colorTypeSupportedAsSurface(kRGBA_8888_SkColorType),
        context->colorTypeSupportedAsSurface(kBGRA_8888_SkColorType), glGetError());
    report(capabilities);
    context->setResourceCacheLimit(8 * 1024 * 1024);
    const auto colorType = context->colorTypeSupportedAsSurface(kRGBA_8888_SkColorType)
        ? kRGBA_8888_SkColorType : kBGRA_8888_SkColorType;
    auto imageInfo = SkImageInfo::Make(128, 96, colorType,
        kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::RenderTarget(context.get(), skgpu::Budgeted::kYes,
        imageInfo, 0, kTopLeft_GrSurfaceOrigin, nullptr);
    if (!expect(surface && surface->recordingContext() == context.get(), "GPU render target"))
        return;

    auto* canvas = surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    canvas->drawRect(SkRect::MakeXYWH(8, 8, 16, 16), paint);
    canvas->save();
    canvas->clipRect(SkRect::MakeXYWH(32, 0, 16, 16));
    paint.setColor(SK_ColorGREEN);
    canvas->drawPaint(paint);
    canvas->restore();
    paint.setColor(0x800000ff);
    canvas->drawRect(SkRect::MakeXYWH(8, 8, 16, 16), paint);
    paint.setColor(SK_ColorBLUE);
    canvas->drawCircle(80, 24, 8, paint);

    auto fontManager = SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
    auto typeface = fontManager ? fontManager->matchFamilyStyle("sans-serif", SkFontStyle::Normal()) : nullptr;
    expect(!!typeface, "native font for GPU glyph atlas");
    if (typeface) {
        SkFont font(typeface, 18);
        paint.setColor(SK_ColorBLACK);
        canvas->drawSimpleText("Ganesh", 6, SkTextEncoding::kUTF8, 4, 80, font, paint);
    }
    context->flushAndSubmit(surface.get(), GrSyncCpu::kYes);
    static uint32_t pixels[128 * 96];
    auto readInfo = SkImageInfo::Make(128, 96, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    if (expect(surface->readPixels(readInfo, pixels, 128 * sizeof(uint32_t), 0, 0), "GPU pixel readback")) {
        char actual[128];
        std::snprintf(actual, sizeof(actual), "INFO: pixels clear=%08x clip=%08x blend=%08x path=%08x",
            pixels[0], pixels[5 * 128 + 35], pixels[10 * 128 + 10], pixels[24 * 128 + 80]);
        report(actual);
        expect(pixels[0] == 0xffffffff && pixels[5 * 128 + 35] == 0xff00ff00
            && pixels[5 * 128 + 50] == 0xffffffff, "clear and clipped rectangle pixels");
        const uint32_t blended = pixels[10 * 128 + 10];
        const unsigned red = (blended >> 16) & 255;
        const unsigned blue = blended & 255;
        expect((blended >> 24) == 255 && red >= 126 && red <= 128
            && blue >= 127 && blue <= 129, "GPU alpha blending");
        expect(pixels[24 * 128 + 80] == 0xff0000ff, "GPU path pixels");
        unsigned glyphPixels = 0;
        for (unsigned y = 60; y < 84; ++y) {
            for (unsigned x = 0; x < 100; ++x)
                glyphPixels += pixels[y * 128 + x] != 0xffffffff;
        }
        expect(glyphPixels > 20, "GPU text pixels");
    }

    auto texture = SkSurfaces::GetBackendTexture(surface.get(), SkSurfaces::BackendHandleAccess::kFlushRead);
    GrGLTextureInfo textureInfo;
    bool haveTexture = GrBackendTextures::GetGLTextureInfo(texture, &textureInfo);
    expect(haveTexture && textureInfo.fTarget == GL_TEXTURE_2D, "GPU texture for composition");

    GLuint tileTexture = 0;
    glGenTextures(1, &tileTexture);
    glBindTexture(GL_TEXTURE_2D, tileTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const GLenum tileFormat = colorType == kBGRA_8888_SkColorType ? 0x80e1 : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, tileFormat, 128, 96, 0, tileFormat, GL_UNSIGNED_BYTE, nullptr);
    context->resetContext();
    GrGLTextureInfo tileInfo { GL_TEXTURE_2D, tileTexture, textureInfo.fFormat };
    auto tileBackend = GrBackendTextures::MakeGL(128, 96, skgpu::Mipmapped::kNo, tileInfo);
    auto tileSurface = SkSurfaces::WrapBackendTexture(context.get(), tileBackend,
        kTopLeft_GrSurfaceOrigin, 0, colorType, SkColorSpace::MakeSRGB(), nullptr);
    if (expect(!!tileSurface, "wrap native compositor texture")) {
        tileSurface->getCanvas()->clear(SK_ColorWHITE);
        paint.setColor(SK_ColorRED);
        tileSurface->getCanvas()->drawRect(SkRect::MakeXYWH(0, 0, 128, 12), paint);
        paint.setColor(SK_ColorGREEN);
        tileSurface->getCanvas()->drawRect(SkRect::MakeXYWH(0, 84, 128, 12), paint);
        context->flushAndSubmit(tileSurface.get(), GrSyncCpu::kYes);
    }
    const EGLint contextAttributes[] { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    const EGLint sharedSurfaceAttributes[] { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    auto sharedContext = eglCreateContext(display, config, eglContext, contextAttributes);
    auto sharedPbuffer = eglCreatePbufferSurface(display, config, sharedSurfaceAttributes);
    if (expect(sharedContext != EGL_NO_CONTEXT && sharedPbuffer != EGL_NO_SURFACE,
        "shared GLES2 compositor context")) {
        if (expect(eglMakeCurrent(display, sharedPbuffer, sharedPbuffer, sharedContext) == EGL_TRUE,
            "make shared context current")) {
            GLuint framebuffer = 0;
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureInfo.fID, 0);
            bool complete = haveTexture && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            expect(complete, "shared Ganesh texture framebuffer");
            if (complete) {
                unsigned char blue[4] {};
                glReadPixels(80, 24, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, blue);
                expect(blue[0] == 0 && blue[1] == 0 && blue[2] == 255 && blue[3] == 255,
                    "shared GPU texture pixels");
            }
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tileTexture, 0);
            if (expect(tileSurface && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "shared wrapped texture framebuffer")) {
                unsigned char top[4] {};
                unsigned char bottom[4] {};
                glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, top);
                glReadPixels(4, 92, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, bottom);
                char rows[256];
                std::snprintf(rows, sizeof(rows), "INFO: texture rows 4=(%u,%u,%u,%u) 92=(%u,%u,%u,%u)",
                    top[0], top[1], top[2], top[3], bottom[0], bottom[1], bottom[2], bottom[3]);
                report(rows);
                expect(top[0] == 255 && top[1] == 0 && top[2] == 0 && top[3] == 255
                    && bottom[0] == 0 && bottom[1] == 255 && bottom[2] == 0 && bottom[3] == 255,
                    "shared compositor texture row orientation");
            }
            glDeleteFramebuffers(1, &framebuffer);
        }
        eglMakeCurrent(display, pbuffer, pbuffer, eglContext);
    }
    if (sharedContext != EGL_NO_CONTEXT)
        eglDestroyContext(display, sharedContext);
    if (sharedPbuffer != EGL_NO_SURFACE)
        eglDestroySurface(display, sharedPbuffer);
    tileSurface.reset();
    surface.reset();
    context->releaseResourcesAndAbandonContext();
    glDeleteTextures(1, &tileTexture);
    expect(glGetError() == GL_NO_ERROR, "native GLES2 completion");
}

static void checkSurvivingContext(EGLDisplay display, EGLConfig config, EGLContext& creator)
{
    const char* vertexSource = "attribute vec2 position; void main(){ gl_Position=vec4(position,0.0,1.0); }";
    const char* fragmentSource = "precision mediump float; uniform sampler2D image; void main(){ gl_FragColor=texture2D(image,vec2(0.5)); }";
    GLuint shaders[2] { glCreateShader(GL_VERTEX_SHADER), glCreateShader(GL_FRAGMENT_SHADER) };
    glShaderSource(shaders[0], 1, &vertexSource, nullptr);
    glShaderSource(shaders[1], 1, &fragmentSource, nullptr);
    glCompileShader(shaders[0]);
    glCompileShader(shaders[1]);
    GLuint program = glCreateProgram();
    glAttachShader(program, shaders[0]);
    glAttachShader(program, shaders[1]);
    glBindAttribLocation(program, 0, "position");
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    expect(linked == GL_TRUE, "creator shader program");

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const unsigned char sourcePixel[] { 255, 0, 0, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, sourcePixel);
    const GLfloat vertices[] { -1, -1, 1, -1, -1, 1, 1, 1 };
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
    GLuint renderbuffer = 0;
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 4, 4);
    glFinish();

    const EGLint contextAttributes[] { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    const EGLint surfaceAttributes[] { EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE };
    auto survivor = eglCreateContext(display, config, creator, contextAttributes);
    auto surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    if (expect(survivor != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE
        && eglMakeCurrent(display, surface, surface, survivor) == EGL_TRUE, "surviving shared context")) {
        expect(eglDestroyContext(display, creator) == EGL_TRUE, "close resource creator context");
        creator = EGL_NO_CONTEXT;
        expect(glIsShader(shaders[0]) && glIsShader(shaders[1]) && glIsProgram(program)
            && glIsTexture(texture) && glIsBuffer(buffer) && glIsRenderbuffer(renderbuffer),
            "shared resources survive creator close");

        glBindAttribLocation(program, 0, "position");
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        expect(linked == GL_TRUE, "relink surviving shader program");
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "image"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        const unsigned char updatedPixel[] { 0, 255, 0, 255 };
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, updatedPixel);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 4, 4);
        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
        if (expect(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
            "surviving renderbuffer framebuffer")) {
            glDisable(GL_BLEND);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_STENCIL_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glViewport(0, 0, 4, 4);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glFinish();
            unsigned char pixel[4] {};
            glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            expect(pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255,
                "draw updated shared resources after creator close");
        }
        glUseProgram(0);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteProgram(program);
        glDeleteShader(shaders[0]);
        glDeleteShader(shaders[1]);
        glDeleteTextures(1, &texture);
        glDeleteBuffers(1, &buffer);
        glDeleteRenderbuffers(1, &renderbuffer);
        glFinish();
        expect(glGetError() == GL_NO_ERROR, "surviving shared resource cleanup");
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (survivor != EGL_NO_CONTEXT)
        eglDestroyContext(display, survivor);
    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
}

int main()
{
    mkdir("C:/Data", 0777);
    mkdir("C:/Data/WebKit", 0777);
    output = std::fopen("C:/Data/WebKit/ganesh-probe.log", "w");
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0;
    EGLint minor = 0;
    if (expect(display != EGL_NO_DISPLAY && eglInitialize(display, &major, &minor) == EGL_TRUE,
        "native EGL display")) {
        const EGLint configAttributes[] { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
        EGLConfig config = 0;
        EGLint count = 0;
        if (expect(eglChooseConfig(display, configAttributes, &config, 1, &count) == EGL_TRUE && count,
            "RGBA pbuffer configuration")) {
            eglBindAPI(EGL_OPENGL_ES_API);
            const EGLint contextAttributes[] { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
            const EGLint surfaceAttributes[] { EGL_WIDTH, 128, EGL_HEIGHT, 96, EGL_NONE };
            auto context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
            auto pbuffer = eglCreatePbufferSurface(display, config, surfaceAttributes);
            if (expect(context != EGL_NO_CONTEXT && pbuffer != EGL_NO_SURFACE
                && eglMakeCurrent(display, pbuffer, pbuffer, context) == EGL_TRUE, "native GLES2 pbuffer")) {
                char line[256];
                std::snprintf(line, sizeof(line), "INFO: %s; %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));
                report(line);
                if (checkContextState(display, config, pbuffer, context)) {
                    drawAndCheck(display, config, pbuffer, context);
                    checkSurvivingContext(display, config, context);
                }
            }
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (pbuffer != EGL_NO_SURFACE)
                eglDestroySurface(display, pbuffer);
            if (context != EGL_NO_CONTEXT)
                eglDestroyContext(display, context);
        }
        eglTerminate(display);
    }
    char result[256];
    std::snprintf(result, sizeof(result), "RESULT {\"probe\":\"ganesh-probe\",\"passed\":%u,\"failed\":%u}",
        checks - failures, failures);
    report(result);
    report(failures ? "FAIL: ganesh-probe" : "PASS: ganesh-probe");
    std::snprintf(result, sizeof(result), "ganesh-probe exiting %u", failures ? 1 : 0);
    report(result);
    if (output)
        std::fclose(output);
    return failures ? 1 : 0;
}
