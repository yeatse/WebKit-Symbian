// EGLImages the phone can make without copying pixels through GL: RSgImages
// (EGL_NOK_pixmap_type_rsgimage), GL textures shared with an unshared context
// (EGL_KHR_gl_texture_2D_image), and surface manager buffers acquired through
// an EGL_NOK_image_endpoint2 consumer endpoint.
#include "images.h"
#include <e32base.h>
#include <hal.h>
#include <graphics/surface.h>
#include <graphics/surfacemanager.h>
#include <graphics/surfaceupdateclient.h>
#include <graphics/suerror.h>
#include <pixelformats.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

ProbeReport report;
EGLDisplay display;

using EGLImageTargetTexture2D = void (*)(GLenum, EGLImageKHR);
EGLImageTargetTexture2D imageTargetRenderbuffer;
using CreateImage = EGLImageKHR (*)(EGLDisplay, EGLContext, EGLenum, void*, const EGLint*);
using DestroyImage = EGLBoolean (*)(EGLDisplay, EGLImageKHR);
EGLImageTargetTexture2D imageTargetTexture;
CreateImage createImage;
DestroyImage destroyImage;

const EGLint KImagePreserved[] { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
const TUint32 KQuadrants[4] { 0xffff0000, 0xff0000ff, 0xff00ff00, 0xffffffff }; // top-left, top-right, bottom-left, bottom-right

unsigned microsecondsSince(TUint32 start)
{
    TInt frequency = 0;
    if (HAL::Get(HALData::EFastCounterFrequency, frequency) != KErrNone || frequency <= 0)
        return 0;
    return static_cast<unsigned>((static_cast<TUint64>(User::FastCounter() - start) * 1000000) / frequency);
}

void fillQuadrants(TUint32* pixels, int width, int height, int strideWords, TUint32 tint)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            pixels[y * strideWords + x] = KQuadrants[(y >= height / 2) * 2 + (x >= width / 2)] ^ tint;
    }
}

GLuint program;
GLint positionLocation;

GLuint compile(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
        report("images: shader 0x%x does not compile", type);
    return shader;
}

bool buildProgram()
{
    program = glCreateProgram();
    glAttachShader(program, compile(GL_VERTEX_SHADER,
        "attribute vec2 p; varying vec2 t; void main() { t = p * 0.5 + 0.5; gl_Position = vec4(p, 0.0, 1.0); }"));
    glAttachShader(program, compile(GL_FRAGMENT_SHADER,
        "precision mediump float; varying vec2 t; uniform sampler2D s; void main() { gl_FragColor = texture2D(s, t); }"));
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    positionLocation = glGetAttribLocation(program, "p");
    return linked && positionLocation >= 0;
}

// Draws texture over the 64x64 pbuffer and reports the quadrant colours read
// back in GL's bottom-up order: bottom-left, bottom-right, top-left, top-right.
void drawAndRead(GLuint texture, const char* label)
{
    static const GLfloat quad[] { -1, -1, 1, -1, -1, 1, 1, 1 };
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 64, 64);
    glClearColor(0.5f, 0.5f, 0.5f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, quad);
    glEnableVertexAttribArray(positionLocation);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (!label)
        return;
    TUint8 pixels[4][4] { };
    const int points[4][2] { { 16, 16 }, { 48, 16 }, { 16, 48 }, { 48, 48 } };
    for (int i = 0; i < 4; ++i)
        glReadPixels(points[i][0], points[i][1], 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels[i]);
    report("%s: GL error 0x%x, rgba bottom-left %02x%02x%02x%02x bottom-right %02x%02x%02x%02x top-left %02x%02x%02x%02x top-right %02x%02x%02x%02x",
        label, glGetError(), pixels[0][0], pixels[0][1], pixels[0][2], pixels[0][3], pixels[1][0], pixels[1][1], pixels[1][2], pixels[1][3],
        pixels[2][0], pixels[2][1], pixels[2][2], pixels[2][3], pixels[3][0], pixels[3][1], pixels[3][2], pixels[3][3]);
}

// RSgImage through either Graphics Resource 1.1 (sgresource.dll, RSgDriver)
// or 1.0 (graphicsresource.dll, SgDriver), looked up by ordinal so the probe
// starts on firmware with neither.
struct SgImage {
    TUid handleType { 0x10285A73 };
    void* impl { nullptr };
    TUint32 spare[2] { };
};

class GraphicsResource {
public:
    bool open()
    {
        if (iLibrary.Load(_L("sgresource.dll")) == KErrNone) {
            iVersion = 11;
            iDriverOpen = iLibrary.Lookup(1);
            iDriverClose = iLibrary.Lookup(2);
            iImageCreate = iLibrary.Lookup(9);
            iDrawableClose = iLibrary.Lookup(5);
            iPixelFormats = iLibrary.Lookup(13);
        } else if (iLibrary.Load(_L("graphicsresource.dll")) == KErrNone) {
            iVersion = 10;
            iDriverOpen = iLibrary.Lookup(21);
            iDriverClose = iLibrary.Lookup(22);
            iImageCreate = iLibrary.Lookup(16);
            iDrawableClose = iLibrary.Lookup(2);
            iInfoConstruct = iLibrary.Lookup(5);
            iImageConstruct = iLibrary.Lookup(18);
        } else {
            report("images: neither sgresource.dll nor graphicsresource.dll loads");
            return false;
        }
        if (!iDriverOpen || !iDriverClose || !iImageCreate || !iDrawableClose
            || (iVersion == 10 && (!iInfoConstruct || !iImageConstruct))) {
            report("images: Graphics Resource %d.%d lacks an ordinal", iVersion / 10, iVersion % 10);
            return false;
        }
        TInt error = iVersion == 11 ? reinterpret_cast<TInt (*)(void*)>(iDriverOpen)(&iDriver)
                                    : reinterpret_cast<TInt (*)()>(iDriverOpen)();
        report("images: Graphics Resource %d.%d, driver open %d", iVersion / 10, iVersion % 10, error);
        iOpen = error == KErrNone;
        return iOpen;
    }

    void close()
    {
        if (iOpen) {
            if (iVersion == 11)
                reinterpret_cast<void (*)(void*)>(iDriverClose)(&iDriver);
            else
                reinterpret_cast<void (*)()>(iDriverClose)();
        }
        iOpen = false;
        iLibrary.Close();
    }

    bool isVersion11() const { return iVersion == 11; }

    void reportPixelFormats(TUint32 usage)
    {
        if (iVersion != 11 || !iPixelFormats)
            return;
        RArray<TInt> formats;
        TInt error = reinterpret_cast<TInt (*)(TUint32, RArray<TInt>&, const void*)>(iPixelFormats)(usage, formats, nullptr);
        char line[200];
        int used = std::snprintf(line, sizeof line, "images: pixel formats for usage 0x%x: %d,", static_cast<unsigned>(usage), error);
        for (TInt i = 0; i < formats.Count() && used < static_cast<int>(sizeof line) - 12; ++i)
            used += std::snprintf(line + used, sizeof line - used, " 0x%x", static_cast<unsigned>(formats[i]));
        report("%s", line);
        formats.Close();
    }

    // usage is a 1.1 TSgUsageBits value; 1.0 gets the matching TSgUsageFlags.
    TInt create(SgImage& image, int width, int height, TInt format, TUint32 usage, const void* data, int stride)
    {
        if (iVersion == 11) {
            struct { TSize size; TInt format; TUint32 usage; } info { TSize(width, height), format, usage };
            return reinterpret_cast<TInt (*)(void*, const void*, const void*, TInt, const void*)>(iImageCreate)(&image, &info, data, stride, nullptr);
        }
        TUint32 usage10 = (usage & 0x1 ? 0x0080 : 0) | (usage & 0x2 ? 0x0020 : 0) | (usage & 0x4 ? 0x0400 : 0)
            | (usage & 0x10 ? 0x0100 : 0) | (usage & 0x20 ? 0x0040 : 0) | (usage & 0x40 ? 0x0800 : 0);
        TUint32 info[16] { };
        reinterpret_cast<void (*)(void*)>(iInfoConstruct)(info);
        info[0] = width;
        info[1] = height;
        info[2] = format;
        info[3] = usage10;
        reinterpret_cast<void (*)(void*)>(iImageConstruct)(&image);
        return reinterpret_cast<TInt (*)(void*, const void*, const void*, TInt)>(iImageCreate)(&image, info, data, stride);
    }

    void destroy(SgImage& image)
    {
        if (image.impl)
            reinterpret_cast<void (*)(void*)>(iDrawableClose)(&image);
        image.impl = nullptr;
    }

private:
    RLibrary iLibrary;
    int iVersion { 0 };
    bool iOpen { false };
    void* iDriver[2] { };
    TLibraryFunction iDriverOpen { }, iDriverClose { }, iImageCreate { }, iDrawableClose { }, iInfoConstruct { }, iImageConstruct { }, iPixelFormats { };
};

enum : TUint32 {
    KUsageVgImage = 0x0001, // ESgUsageBitOpenVgImage
    KUsageGles1Texture = 0x0002, // ESgUsageBitOpenGlesTexture2D
    KUsageGles2Texture = 0x0004, // ESgUsageBitOpenGles2Texture2D
    KUsageGles2Surface = 0x0040, // ESgUsageBitOpenGles2Surface
};

void clearErrors()
{
    for (int i = 0; i < 8 && glGetError() != GL_NO_ERROR; ++i) { }
}

GLuint textureFromImage(EGLImageKHR image, GLenum* error = nullptr)
{
    clearErrors();
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    imageTargetTexture(GL_TEXTURE_2D, image);
    GLenum result = glGetError();
    if (error)
        *error = result;
    return texture;
}

struct Pixels {
    TInt format;
    const char* name;
    int bytes;
};

const Pixels KFormats[] {
    { EUidPixelFormatARGB_8888_PRE, "ARGB_8888_PRE", 4 },
    { EUidPixelFormatARGB_8888, "ARGB_8888", 4 },
    { EUidPixelFormatXRGB_8888, "XRGB_8888", 4 },
    { EUidPixelFormatRGB_565, "RGB_565", 2 },
};

void fillQuadrants565(TUint16* pixels, int width, int height)
{
    const TUint16 colours[4] { 0xf800, 0x001f, 0x07e0, 0xffff };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            pixels[y * width + x] = colours[(y >= height / 2) * 2 + (x >= width / 2)];
    }
}

// Each pixel format and usage, with and without EGL_IMAGE_PRESERVED: does the
// RSgImage become an EGLImage, and does GLES2 take that as a texture?
bool probeSgImageMatrix(GraphicsResource& resource, const Pixels*& working, TUint32& workingUsage, const EGLint*& workingAttributes)
{
    static TUint32 data[64 * 64];
    const TUint32 usages[] { KUsageGles2Texture, KUsageGles2Texture | KUsageVgImage, KUsageGles2Texture | KUsageGles1Texture,
        KUsageGles2Texture | KUsageGles2Surface };
    const EGLint* attributeLists[] { KImagePreserved, nullptr };
    bool found = false;
    for (const Pixels& pixels : KFormats) {
        if (pixels.bytes == 4)
            fillQuadrants(data, 64, 64, 64, 0);
        else
            fillQuadrants565(reinterpret_cast<TUint16*>(data), 64, 64);
        for (TUint32 usage : usages) {
            SgImage source;
            TInt created = resource.create(source, 64, 64, pixels.format, usage, data, 64 * pixels.bytes);
            if (created != KErrNone) {
                report("images: %s usage 0x%x: create %d", pixels.name, static_cast<unsigned>(usage), created);
                continue;
            }
            for (const EGLint* attributes : attributeLists) {
                EGLImageKHR image = createImage(display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, &source, attributes);
                EGLint eglError = eglGetError();
                GLenum glError = GL_NO_ERROR;
                GLuint texture = image != EGL_NO_IMAGE_KHR ? textureFromImage(image, &glError) : 0;
                GLenum renderbufferError = GL_INVALID_ENUM;
                if (image != EGL_NO_IMAGE_KHR && imageTargetRenderbuffer) {
                    GLuint renderbuffer = 0;
                    glGenRenderbuffers(1, &renderbuffer);
                    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
                    imageTargetRenderbuffer(GL_RENDERBUFFER, image);
                    renderbufferError = glGetError();
                    glDeleteRenderbuffers(1, &renderbuffer);
                }
                report("images: %s usage 0x%x %s: image 0x%x (EGL 0x%x), texture GL 0x%x, renderbuffer GL 0x%x", pixels.name,
                    static_cast<unsigned>(usage), attributes ? "preserved" : "no attributes", image, eglError, glError, renderbufferError);
                if (image != EGL_NO_IMAGE_KHR && glError == GL_NO_ERROR) {
                    drawAndRead(texture, "images:   read back (expect red, blue top; green, white bottom)");
                    if (!found) {
                        found = true;
                        working = &pixels;
                        workingUsage = usage;
                        workingAttributes = attributes;
                    }
                }
                if (texture)
                    glDeleteTextures(1, &texture);
                if (image != EGL_NO_IMAGE_KHR)
                    destroyImage(display, image);
            }
            resource.destroy(source);
        }
    }
    return found;
}

// Reads target, which context drew magenta into, from a second unshared context.
void readInOtherContext(EGLConfig config, EGLContext context, EGLSurface pbuffer, EGLenum target, void* buffer, const char* label)
{
    const EGLint contextAttributes[] { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext other = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (other == EGL_NO_CONTEXT || !eglMakeCurrent(display, pbuffer, pbuffer, other)) {
        report("%s: second context: EGL error 0x%x", label, eglGetError());
        if (other != EGL_NO_CONTEXT)
            eglDestroyContext(display, other);
        eglMakeCurrent(display, pbuffer, pbuffer, context);
        return;
    }
    GLuint ownProgram = program;
    GLint ownPosition = positionLocation;
    if (buildProgram()) {
        EGLImageKHR image = createImage(display, target == EGL_NATIVE_PIXMAP_KHR ? EGL_NO_CONTEXT : context, target, buffer, KImagePreserved);
        EGLint eglError = eglGetError();
        GLenum glError = GL_NO_ERROR;
        GLuint texture = image != EGL_NO_IMAGE_KHR ? textureFromImage(image, &glError) : 0;
        report("%s: image in the second context 0x%x (EGL 0x%x), texture GL 0x%x", label, image, eglError, glError);
        if (texture && glError == GL_NO_ERROR)
            drawAndRead(texture, label);
        if (texture)
            glDeleteTextures(1, &texture);
        if (image != EGL_NO_IMAGE_KHR)
            destroyImage(display, image);
    }
    program = ownProgram;
    positionLocation = ownPosition;
    eglMakeCurrent(display, pbuffer, pbuffer, context);
    eglDestroyContext(display, other);
}

// Clears texture to magenta through a framebuffer; returns the framebuffer status.
GLenum clearToMagenta(GLuint texture)
{
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) {
        glViewport(0, 0, 64, 64);
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glFinish();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    return status;
}

// EGL_KHR_gl_texture_2D_image: a texture of this context shown by another,
// unshared one, after this context draws into it.
void probeTextureImages(EGLConfig config, EGLContext context, EGLSurface pbuffer)
{
    static TUint32 data[64 * 64];
    fillQuadrants(data, 64, 64, 64, 0);
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glFinish();
    void* buffer = reinterpret_cast<void*>(static_cast<uintptr_t>(texture));
    readInOtherContext(config, context, pbuffer, EGL_GL_TEXTURE_2D_KHR, buffer,
        "images: GL texture in another context (expect blue, red top; green, white bottom as RGBA bytes)");
    report("images: framebuffer on that texture: status 0x%x", clearToMagenta(texture));
    readInOtherContext(config, context, pbuffer, EGL_GL_TEXTURE_2D_KHR, buffer, "images: GL texture after drawing (expect magenta)");
    glDeleteTextures(1, &texture);
}

void probeSgImages(EGLConfig config, EGLContext context, EGLSurface pbuffer)
{
    GraphicsResource resource;
    if (!resource.open()) {
        resource.close();
        return;
    }
    resource.reportPixelFormats(KUsageGles2Texture);
    resource.reportPixelFormats(KUsageGles2Surface);
    resource.reportPixelFormats(KUsageGles2Texture | KUsageGles2Surface);
    const Pixels* pixels = nullptr;
    TUint32 usage = 0;
    const EGLint* attributes = nullptr;
    if (!probeSgImageMatrix(resource, pixels, usage, attributes)) {
        report("images: no RSgImage became a GLES2 texture");
    } else {
        report("images: using %s usage 0x%x", pixels->name, static_cast<unsigned>(usage));
        SgImage target;
        TInt error = resource.create(target, 64, 64, pixels->format, usage | KUsageGles2Surface, nullptr, 0);
        report("images: RSgImage 64x64 texture and target: %d", error);
        if (error == KErrNone) {
            EGLImageKHR image = createImage(display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, &target, attributes);
            if (image != EGL_NO_IMAGE_KHR) {
                GLuint texture = textureFromImage(image);
                report("images: framebuffer on the RSgImage texture: status 0x%x", clearToMagenta(texture));
                glDeleteTextures(1, &texture);
                destroyImage(display, image);
                readInOtherContext(config, context, pbuffer, EGL_NATIVE_PIXMAP_KHR, &target,
                    "images: RSgImage target in another context (expect magenta)");
            }
            resource.destroy(target);
        }

        // Upload cost of 512x512: glTexSubImage2D against creating an RSgImage
        // with the pixels and binding its EGLImage, each drawn once.
        TUint32* data = new TUint32[512 * 512];
        fillQuadrants(data, 512, 512, 512, 0);
        const int rounds = 10;
        GLuint uploadTexture = 0;
        glGenTextures(1, &uploadTexture);
        glBindTexture(GL_TEXTURE_2D, uploadTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFinish();
        TUint32 start = User::FastCounter();
        for (int i = 0; i < rounds; ++i) {
            glBindTexture(GL_TEXTURE_2D, uploadTexture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, data);
            drawAndRead(uploadTexture, nullptr);
            glFinish();
        }
        report("images: glTexSubImage2D 512x512, draw and finish: %u us each", microsecondsSince(start) / rounds);
        glDeleteTextures(1, &uploadTexture);

        int made = 0;
        start = User::FastCounter();
        for (int i = 0; i < rounds; ++i) {
            SgImage upload;
            if (resource.create(upload, 512, 512, pixels->format, usage, data, 512 * 4) != KErrNone)
                break;
            EGLImageKHR image = createImage(display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, &upload, attributes);
            if (image != EGL_NO_IMAGE_KHR) {
                GLuint uploaded = textureFromImage(image);
                drawAndRead(uploaded, nullptr);
                glFinish();
                glDeleteTextures(1, &uploaded);
                destroyImage(display, image);
                ++made;
            }
            resource.destroy(upload);
        }
        if (made && pixels->bytes == 4)
            report("images: RSgImage 512x512 create, EGLImage, draw and finish: %u us each (%d of %d)", microsecondsSince(start) / made, made, rounds);
        delete[] data;
    }

    for (const Pixels& format : KFormats) {
        SgImage pixmap;
        TInt created = resource.create(pixmap, 64, 64, format.format, KUsageGles2Surface, nullptr, 0);
        if (created != KErrNone) {
            report("images: pixmap RSgImage %s: create %d", format.name, created);
            continue;
        }
        const EGLint attributes[] { EGL_MATCH_NATIVE_PIXMAP, reinterpret_cast<EGLint>(&pixmap), EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_PIXMAP_BIT, EGL_NONE };
        EGLConfig pixmapConfig;
        EGLint count = 0;
        eglChooseConfig(display, attributes, &pixmapConfig, 1, &count);
        EGLSurface surface = count ? eglCreatePixmapSurface(display, pixmapConfig, &pixmap, nullptr) : EGL_NO_SURFACE;
        report("images: pixmap surface on RSgImage %s: %d configurations, surface %p (EGL error 0x%x)", format.name, count, surface, eglGetError());
        if (surface != EGL_NO_SURFACE)
            eglDestroySurface(display, surface);
        resource.destroy(pixmap);
    }
    resource.close();
}

using CreateEndpoint = EGLEndpointNOK (*)(EGLDisplay, EGLenum, EGLenum, EGLEndpointSourceNOK, const EGLint*);
using EndpointCall = EGLBoolean (*)(EGLDisplay, EGLEndpointNOK);
using EndpointAttribute = EGLint (*)(EGLDisplay, EGLEndpointNOK, EGLint);
using AcquireImage = EGLImageKHR (*)(EGLDisplay, EGLEndpointNOK);
using ReleaseImage = EGLBoolean (*)(EGLDisplay, EGLEndpointNOK, EGLImageKHR, EGLenum);

void probeEndpoint()
{
    auto createEndpoint = reinterpret_cast<CreateEndpoint>(eglGetProcAddress("eglCreateEndpointNOK"));
    auto destroyEndpoint = reinterpret_cast<EndpointCall>(eglGetProcAddress("eglDestroyEndpointNOK"));
    auto beginStreaming = reinterpret_cast<EndpointCall>(eglGetProcAddress("eglEndpointBeginStreamingNOK"));
    auto endStreaming = reinterpret_cast<EndpointCall>(eglGetProcAddress("eglEndpointEndStreamingNOK"));
    auto endpointAttribute = reinterpret_cast<EndpointAttribute>(eglGetProcAddress("eglGetEndpointAttribNOK"));
    auto acquireImage = reinterpret_cast<AcquireImage>(eglGetProcAddress("eglAcquireImageNOK"));
    auto releaseImage = reinterpret_cast<ReleaseImage>(eglGetProcAddress("eglReleaseImageNOK"));
    report("endpoint: create %p destroy %p begin %p end %p attribute %p acquire %p release %p", createEndpoint, destroyEndpoint,
        beginStreaming, endStreaming, endpointAttribute, acquireImage, releaseImage);
    if (!createEndpoint || !destroyEndpoint || !beginStreaming || !endStreaming || !endpointAttribute || !acquireImage || !releaseImage)
        return;
    RSurfaceManager manager;
    TInt error = manager.Open();
    if (error != KErrNone) {
        report("endpoint: surface manager open %d", error);
        return;
    }
    const TSize size(640, 360);
    RSurfaceManager::TSurfaceCreationAttributesBuf request;
    request().iSize = size;
    request().iBuffers = 2;
    request().iPixelFormat = EUidPixelFormatARGB_8888_PRE;
    request().iStride = size.iWidth * 4;
    request().iAlignment = 4;
    request().iCacheAttrib = RSurfaceManager::ECached;
    request().iContiguous = EFalse;
    request().iMappable = ETrue;
    TSurfaceId surface = TSurfaceId::CreateNullId();
    RChunk chunk;
    TInt offsets[2] { };
    error = manager.CreateSurface(request, surface);
    if (error == KErrNone)
        error = manager.MapSurface(surface, chunk);
    for (int buffer = 0; error == KErrNone && buffer < 2; ++buffer)
        error = manager.GetBufferOffset(surface, buffer, offsets[buffer]);
    report("endpoint: surface 640x360x2: %d", error);
    if (error == KErrNone) {
        for (int buffer = 0; buffer < 2; ++buffer) {
            fillQuadrants(reinterpret_cast<TUint32*>(chunk.Base() + offsets[buffer]), size.iWidth, size.iHeight, size.iWidth,
                buffer ? 0x00ffffff : 0);
            manager.SynchronizeCache(surface, buffer, RSurfaceManager::ESyncBeforeNonCPURead);
        }
        EGLEndpointNOK endpoint = createEndpoint(display, EGL_ENDPOINT_TYPE_CONSUMER_NOK, EGL_TSURFACEID_NOK, &surface, nullptr);
        report("endpoint: consumer endpoint %p (EGL error 0x%x)", endpoint, eglGetError());
        RSurfaceUpdateSession updates;
        TInt connected = endpoint ? updates.Connect() : KErrNotReady;
        if (endpoint && connected != KErrNone)
            report("endpoint: surface update session %d", connected);
        if (endpoint && connected == KErrNone) {
            EGLBoolean streaming = beginStreaming(display, endpoint);
            report("endpoint: begin streaming %d (EGL error 0x%x)", streaming, eglGetError());
            const char* labels[2] { "endpoint: buffer 0 (expect red, blue top; green, white bottom)",
                "endpoint: buffer 1 (expect cyan, yellow top; magenta, black bottom)" };
            const int frames = 30;
            int acquired = 0;
            TUint32 start = User::FastCounter();
            for (int frame = 0; frame < frames + 2; ++frame) {
                if (frame == 2)
                    start = User::FastCounter();
                int buffer = frame & 1;
                error = updates.SubmitUpdate(KAllScreens, surface, buffer, nullptr);
                int waited = 0;
                while (endpointAttribute(display, endpoint, EGL_ENDPOINT_READY_NOK) != EGL_TRUE && waited < 100) {
                    User::After(2000);
                    ++waited;
                }
                EGLImageKHR image = acquireImage(display, endpoint);
                if (frame < 2)
                    report("endpoint: submit buffer %d: %d, waited %d ms, image 0x%x (EGL error 0x%x)", buffer, error, waited * 2, image, eglGetError());
                if (image == EGL_NO_IMAGE_KHR) {
                    if (frame < 2)
                        continue;
                    break;
                }
                GLuint texture = textureFromImage(image);
                drawAndRead(texture, frame < 2 ? labels[buffer] : nullptr);
                glDeleteTextures(1, &texture);
                releaseImage(display, endpoint, image, EGL_OPENGL_ES_API);
                if (frame >= 2)
                    ++acquired;
            }
            if (acquired)
                report("endpoint: submit, acquire, draw and release 640x360: %u us each (%d of %d)",
                    microsecondsSince(start) / acquired, acquired, frames);
            endStreaming(display, endpoint);
        }
        if (connected == KErrNone)
            updates.Close();
        if (endpoint)
            destroyEndpoint(display, endpoint);
    }
    chunk.Close();
    if (!surface.IsNull())
        manager.CloseSurface(surface);
    manager.Close();
}

} // namespace

void probeImages(EGLDisplay eglDisplay, EGLConfig config, ProbeReport reporter)
{
    report = reporter;
    display = eglDisplay;
    report("--- images");
    if (!config) {
        report("images: skipped, no configuration");
        return;
    }
    imageTargetTexture = reinterpret_cast<EGLImageTargetTexture2D>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    createImage = reinterpret_cast<CreateImage>(eglGetProcAddress("eglCreateImageKHR"));
    destroyImage = reinterpret_cast<DestroyImage>(eglGetProcAddress("eglDestroyImageKHR"));
    imageTargetRenderbuffer = reinterpret_cast<EGLImageTargetTexture2D>(eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES"));
    if (!imageTargetTexture || !createImage || !destroyImage) {
        report("images: EGLImage functions missing");
        return;
    }
    const EGLint contextAttributes[] { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    const EGLint pbufferAttributes[] { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    EGLSurface pbuffer = eglCreatePbufferSurface(display, config, pbufferAttributes);
    if (context == EGL_NO_CONTEXT || pbuffer == EGL_NO_SURFACE || !eglMakeCurrent(display, pbuffer, pbuffer, context))
        report("images: no current pbuffer context (EGL error 0x%x)", eglGetError());
    else if (!buildProgram())
        report("images: no program");
    else {
        probeSgImages(config, context, pbuffer);
        probeTextureImages(config, context, pbuffer);
        probeEndpoint();
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (pbuffer != EGL_NO_SURFACE)
        eglDestroySurface(display, pbuffer);
    if (context != EGL_NO_CONTEXT)
        eglDestroyContext(display, context);
}
