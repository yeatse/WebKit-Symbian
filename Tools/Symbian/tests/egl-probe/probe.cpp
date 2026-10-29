// Standalone native EGL/GLES2 validation. No WebKit or test-only driver APIs.
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <e32base.h>
#include <e32debug.h>
#include <fbs.h>
#include <w32std.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static FILE* output;
static unsigned checks, failures, swaps, pixelChecks;

static void report(const char* format, ...)
{
    char line[4096];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    TUint16 units[512];
    unsigned length = 0;
    while (line[length] && length < 511) {
        units[length] = static_cast<unsigned char>(line[length]);
        ++length;
    }
    RDebug::RawPrint(TPtrC16(units, length));
    if (output) {
        fprintf(output, "%s\n", line);
        fflush(output);
    }
}

static bool expect(bool condition, const char* description)
{
    ++checks;
    if (!condition)
        ++failures;
    report("%s: %s", condition ? "PASS" : "FAIL", description);
    return condition;
}

static bool glOK(const char* stage)
{
    bool success = true;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            return expect(success, stage);
        report("GL ERROR: %s 0x%x", stage, error);
        success = false;
    }
    report("GL ERROR: error queue did not clear after 16 reads");
    return expect(false, stage);
}

static bool eglOK(EGLBoolean result, const char* stage)
{
    EGLint error = eglGetError();
    if (result != EGL_TRUE || error != EGL_SUCCESS)
        report("EGL ERROR: %s result=%d error=0x%x", stage, result, error);
    return expect(result == EGL_TRUE && error == EGL_SUCCESS, stage);
}

class Window {
public:
    bool open()
    {
        if (!expect(session.Connect() == KErrNone, "window-server connection")) return false;
        connected = true;
        if (!expect(RFbsSession::Connect() == KErrNone, "bitmap-server connection")) return false;
        fbsConnected = true;
        screen = new CWsScreenDevice(session);
        if (!expect(screen && screen->Construct() == KErrNone, "screen device")) return false;
        const TSize size = screen->SizeInPixels();
        if (!expect(size.iWidth >= 160 && size.iHeight >= 160, "usable screen dimensions")) return false;
        width = size.iWidth < 240 ? size.iWidth : 240;
        height = size.iHeight < 320 ? size.iHeight : 320;
        // RWindow constructors capture the connected session's client handle.
        group = new RWindowGroup(session);
        if (!expect(group && group->Construct(1, ETrue) == KErrNone, "window group")) return false;
        groupOpen = true;
        native = new RWindow(session);
        if (!expect(native && native->Construct(*group, 2) == KErrNone, "native window")) return false;
        windowOpen = true;
        group->SetOrdinalPosition(0);
        native->SetRequiredDisplayMode(EColor16MA);
        native->SetExtent(TPoint(0, 0), TSize(width, height));
        native->SetVisible(ETrue);
        native->Activate();
        session.EventReady(&event);
        session.RedrawReady(&redraw);
        requestsOpen = true;
        session.Flush();
        report("INFO: native window %dx%d at %p", width, height, native);
        return true;
    }

    bool pump()
    {
        if (event != KRequestPending) {
            if (!expect(event.Int() == KErrNone, "window event completion")) return false;
            TWsEvent value;
            session.GetEvent(value);
            session.EventReady(&event);
        }
        if (redraw != KRequestPending) {
            if (!expect(redraw.Int() == KErrNone, "window redraw completion")) return false;
            TWsRedrawEvent value;
            session.GetRedraw(value);
            if (value.Handle() == 2) {
                native->BeginRedraw(value.Rect());
                native->EndRedraw();
            }
            session.RedrawReady(&redraw);
        }
        session.Flush();
        return true;
    }

    bool wait(unsigned milliseconds)
    {
        for (unsigned elapsed = 0; elapsed < milliseconds; elapsed += 20) {
            if (!pump()) return false;
            User::After(20000);
        }
        return true;
    }

    void resize()
    {
        width -= 32;
        height -= 48;
        native->SetExtent(TPoint(0, 0), TSize(width, height));
        session.Flush();
        session.Finish();
        report("INFO: native window resized to %dx%d", width, height);
    }

    ~Window()
    {
        if (requestsOpen) {
            session.EventReadyCancel();
            session.RedrawReadyCancel();
            if (event == KRequestPending) User::WaitForRequest(event);
            if (redraw == KRequestPending) User::WaitForRequest(redraw);
        }
        if (windowOpen) native->Close();
        if (groupOpen) group->Close();
        delete native; delete group; delete screen;
        if (connected) session.Close();
        if (fbsConnected) RFbsSession::Disconnect();
    }

    RWindow* native { nullptr };
    int width { 0 }, height { 0 };
private:
    RWsSession session;
    RWindowGroup* group { nullptr };
    CWsScreenDevice* screen { nullptr };
    TRequestStatus event, redraw;
    bool connected { false }, fbsConnected { false }, groupOpen { false };
    bool windowOpen { false }, requestsOpen { false };
};

class Probe {
public:
    bool run(unsigned holdMilliseconds, bool requireES3Rejection)
    {
        if (!window.open()) return false;
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (!expect(display != EGL_NO_DISPLAY, "EGL display handle")) return false;
        EGLint major = 0, minor = 0;
        if (!eglOK(eglInitialize(display, &major, &minor), "EGL initialization")) return false;
        initialized = true;
        report("INFO: EGL version %d.%d vendor=%s version=%s", major, minor,
            eglQueryString(display, EGL_VENDOR), eglQueryString(display, EGL_VERSION));
        report("INFO: EGL extensions %s", eglQueryString(display, EGL_EXTENSIONS));
        if (!eglOK(eglBindAPI(EGL_OPENGL_ES_API), "bind GLES API")) return false;
        // Matches the minimum window config requested by WebCore's TextureMapper GLContext.
        const EGLint attributes[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_NONE };
        EGLint count = 0;
        if (!eglOK(eglChooseConfig(display, attributes, &config, 1, &count), "choose EGL RGBA8 stencil8 window config")) return false;
        if (!expect(count == 1, "matching compositor window config")) return false;
        const EGLint configAttributes[] = {EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE, EGL_STENCIL_SIZE, EGL_DEPTH_SIZE};
        for (EGLint attribute : configAttributes) {
            EGLint value = 0;
            if (!eglOK(eglGetConfigAttrib(display, config, attribute, &value), "query EGL config")) return false;
            report("INFO: EGL config attribute 0x%x=%d", attribute, value);
        }
        const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        if (!eglOK(context != EGL_NO_CONTEXT, "create GLES2 context")) return false;
        if (requireES3Rejection) {
            const EGLint unsupportedAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
            EGLContext unsupported = eglCreateContext(display, config, EGL_NO_CONTEXT, unsupportedAttributes);
            EGLint error = eglGetError();
            report("INFO: requested EKA ES3 rejection context=%d error=0x%x", unsupported, error);
            if (unsupported != EGL_NO_CONTEXT) eglDestroyContext(display, unsupported);
            if (!expect(unsupported == EGL_NO_CONTEXT && error == EGL_BAD_MATCH, "EKA rejects unsupported ES3 context")) return false;
        }
        const EGLint pbufferConfigAttributes[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
        EGLConfig pbufferConfig = 0;
        if (!eglOK(eglChooseConfig(display, pbufferConfigAttributes, &pbufferConfig, 1, &count), "choose pbuffer config")
            || !expect(count == 1, "pbuffer config available")) return false;
        const EGLint pbufferAttributes[] = { EGL_WIDTH, 8, EGL_HEIGHT, 8, EGL_NONE };
        EGLSurface pbuffer = eglCreatePbufferSurface(display, pbufferConfig, pbufferAttributes);
        if (!eglOK(pbuffer != EGL_NO_SURFACE, "pbuffer attributes terminated by EGL_NONE")) return false;
        if (!eglOK(eglDestroySurface(display, pbuffer), "destroy probe pbuffer")) return false;
        if (!createSurface()) return false;
        report("INFO: GL vendor=%s renderer=%s version=%s shading-language=%s", glGetString(GL_VENDOR),
            glGetString(GL_RENDERER), glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
        report("INFO: GL extensions %s", glGetString(GL_EXTENSIONS));
        if (!glOK("query GL driver information")) return false;
        if (!createProgram()) return false;
        glDisable(GL_DITHER);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_SCISSOR_TEST);

        glGenTextures(1, &source);
        glBindTexture(GL_TEXTURE_2D, source);
        textureParameters();
        uploadQuadrants(true);
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        textureParameters();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        report("INFO: FBO status=0x%x", status);
        if (!expect(status == GL_FRAMEBUFFER_COMPLETE, "RGBA texture FBO complete") || !glOK("texture and FBO setup")) return false;
        glViewport(0, 0, 32, 32);
        draw(source);
        if (!glOK("shader texture draw to FBO")) return false;
        if (!verifyQueries()) return false;
        if (!pixel(8, 8, 255, 0, 0, 255, "FBO bottom-left red")
            || !pixel(24, 8, 0, 255, 0, 255, "FBO bottom-right green")
            || !pixel(8, 24, 0, 0, 255, 255, "FBO top-left blue")
            || !pixel(24, 24, 255, 255, 0, 128, "FBO top-right alpha")) return false;

        // Premultiplied Cairo-compatible source over opaque blue must produce purple.
        const unsigned char translucent[] = {128, 0, 0, 128, 128, 0, 0, 128, 128, 0, 0, 128, 128, 0, 0, 128};
        glBindTexture(GL_TEXTURE_2D, source);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, translucent);
        glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        draw(source);
        glDisable(GL_BLEND);
        if (!glOK("premultiplied alpha blend") || !pixel(16, 16, 128, 0, 127, 255, "FBO premultiplied alpha result")) return false;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, window.width, window.height);
        glBindTexture(GL_TEXTURE_2D, source); uploadQuadrants(false);
        draw(source);
        if (!pixel(window.width / 4, window.height / 4, 255, 0, 0, 255, "window frame 1 texture red")
            || !pixel(3 * window.width / 4, 3 * window.height / 4, 255, 255, 0, 255, "window frame 1 texture yellow")
            || !swap("window frame 1 swap") || !window.wait(500)) return false;
        draw(color);
        if (!pixel(window.width / 2, window.height / 2, 128, 0, 127, 255, "window frame 2 FBO texture")
            || !swap("window frame 2 swap") || !window.wait(500)) return false;

        // Recreate only the surface after a real native window resize. Keep the context,
        // shader, uploaded texture and FBO alive to exercise their lifetime boundary.
        if (!eglOK(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "unbind for surface resize")) return false;
        current = false;
        if (!eglOK(eglDestroySurface(display, surface), "destroy old window surface")) return false;
        surface = EGL_NO_SURFACE;
        window.resize();
        if (!createSurface()) return false;
        glViewport(0, 0, window.width, window.height);
        draw(color);
        if (!pixel(window.width - 2, window.height - 2, 128, 0, 127, 255, "resized window preserved FBO texture")
            || !swap("resized window frame 3 swap")) return false;
        report("READY: egl-window-probe presented 3 frames; resized %dx%d; holding %u ms", window.width, window.height, holdMilliseconds);
        return window.wait(holdMilliseconds);
    }

    ~Probe()
    {
        if (current) {
            if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
            if (source) glDeleteTextures(1, &source);
            if (color) glDeleteTextures(1, &color);
            if (program) glDeleteProgram(program);
            glOK("delete GL resources");
            eglOK(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "release current EGL context");
        }
        if (surface != EGL_NO_SURFACE) eglOK(eglDestroySurface(display, surface), "destroy EGL surface");
        if (context != EGL_NO_CONTEXT) eglOK(eglDestroyContext(display, context), "destroy EGL context");
        if (initialized) eglOK(eglTerminate(display), "terminate EGL display");
    }

private:
    bool createSurface()
    {
        surface = eglCreateWindowSurface(display, config, static_cast<EGLNativeWindowType>(window.native), nullptr);
        if (!eglOK(surface != EGL_NO_SURFACE, "create native EGL window surface")) return false;
        if (!eglOK(eglMakeCurrent(display, surface, surface, context), "make GLES2 window current")) return false;
        current = true;
        EGLint width = 0, height = 0;
        if (!eglOK(eglQuerySurface(display, surface, EGL_WIDTH, &width), "query surface width")
            || !eglOK(eglQuerySurface(display, surface, EGL_HEIGHT, &height), "query surface height")) return false;
        report("INFO: EGL surface %dx%d; native window %dx%d", width, height, window.width, window.height);
        return expect(width == window.width && height == window.height, "EGL surface matches native window extent");
    }

    GLuint compile(GLenum type, const char* sourceText)
    {
        GLuint shader = glCreateShader(type);
        if (!expect(shader != 0, "create shader")) return 0;
        glShaderSource(shader, 1, &sourceText, nullptr); glCompileShader(shader);
        GLint success = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        char log[2048] = {0}; glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        report("INFO: shader type=0x%x status=%d log=%s", type, success, log);
        if (!expect(success == GL_TRUE, "compile shader")) { glDeleteShader(shader); return 0; }
        return shader;
    }

    bool createProgram()
    {
        const char* vertexText = "attribute vec2 position; attribute vec2 texCoord; varying vec2 uv; void main(){ gl_Position=vec4(position,0.0,1.0); uv=texCoord; }";
        const char* fragmentText = "precision mediump float; varying vec2 uv; uniform sampler2D image; void main(){ gl_FragColor=texture2D(image,uv); }";
        GLuint vertex = compile(GL_VERTEX_SHADER, vertexText), fragment = compile(GL_FRAGMENT_SHADER, fragmentText);
        if (!vertex || !fragment) {
            if (vertex) glDeleteShader(vertex);
            if (fragment) glDeleteShader(fragment);
            return false;
        }
        program = glCreateProgram();
        glAttachShader(program, vertex); glAttachShader(program, fragment);
        glBindAttribLocation(program, 0, "position"); glBindAttribLocation(program, 1, "texCoord");
        glLinkProgram(program);
        glDeleteShader(vertex); glDeleteShader(fragment);
        GLint success = 0; glGetProgramiv(program, GL_LINK_STATUS, &success);
        char log[2048] = {0}; glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        report("INFO: program link status=%d log=%s", success, log);
        if (!expect(success == GL_TRUE, "link textured shader program")) return false;
        GLint validated = -1;
        glGetProgramiv(program, GL_VALIDATE_STATUS, &validated);
        if (!expect(validated == GL_FALSE, "program starts unvalidated")) return false;
        glUseProgram(program);
        GLint sampler = glGetUniformLocation(program, "image");
        if (!expect(sampler >= 0, "active texture sampler uniform")) return false;
        glUniform1i(sampler, 0);
        return glOK("shader program setup");
    }

    bool verifyQueries()
    {
        GLint viewport[2] = {0, 0};
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport);
        report("INFO: max viewport=%d,%d", viewport[0], viewport[1]);
        if (!expect(viewport[0] > 0 && viewport[1] > 0, "both max viewport dimensions")) return false;
        const GLenum limits[] = { GL_MAX_CUBE_MAP_TEXTURE_SIZE, GL_MAX_RENDERBUFFER_SIZE,
            GL_MAX_VERTEX_UNIFORM_VECTORS, GL_MAX_FRAGMENT_UNIFORM_VECTORS, GL_MAX_VARYING_VECTORS };
        for (GLenum limit : limits) {
            GLint value = 0;
            glGetIntegerv(limit, &value);
            report("INFO: GL limit 0x%x=%d", limit, value);
            if (!expect(value > 0, "positive GLES2 capability limit")) return false;
        }
        const GLenum ranges[] = { GL_ALIASED_POINT_SIZE_RANGE, GL_ALIASED_LINE_WIDTH_RANGE };
        for (GLenum query : ranges) {
            GLfloat range[2] = {0, 0};
            glGetFloatv(query, range);
            report("INFO: GL range 0x%x=%f,%f", query, range[0], range[1]);
            if (!expect(range[0] > 0 && range[1] >= range[0], "positive GLES2 capability range")) return false;
        }
        if (!glOK("ANGLE-required capability queries")) return false;
        const GLenum shaderTypes[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
        const GLenum precisions[] = { GL_LOW_FLOAT, GL_MEDIUM_FLOAT, GL_HIGH_FLOAT, GL_LOW_INT, GL_MEDIUM_INT, GL_HIGH_INT };
        for (GLenum shader : shaderTypes) {
            for (GLenum precisionType : precisions) {
                GLint range[2] = {-1, -1}, precision = -1;
                glGetShaderPrecisionFormat(shader, precisionType, range, &precision);
                report("INFO: precision shader=0x%x type=0x%x range=%d,%d precision=%d", shader, precisionType, range[0], range[1], precision);
                bool optionalHigh = shader == GL_FRAGMENT_SHADER && (precisionType == GL_HIGH_FLOAT || precisionType == GL_HIGH_INT);
                bool absent = !range[0] && !range[1] && !precision;
                bool floating = precisionType == GL_LOW_FLOAT || precisionType == GL_MEDIUM_FLOAT || precisionType == GL_HIGH_FLOAT;
                if (!expect((optionalHigh && absent) || (range[0] > 0 && range[1] > 0 && (floating ? precision > 0 : precision == 0)), "shader precision query")) return false;
            }
        }
        const GLenum attachmentQueries[] = { GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL };
        const GLint attachmentExpected[] = { GL_TEXTURE, static_cast<GLint>(color), 0 };
        for (unsigned i = 0; i < 3; ++i) {
            GLint result = -1;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, attachmentQueries[i], &result);
            if (!expect(result == attachmentExpected[i], "FBO attachment query")) return false;
        }
        GLuint renderbuffer = 0;
        glGenRenderbuffers(1, &renderbuffer); glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 17, 19);
        const GLenum renderbufferQueries[] = {GL_RENDERBUFFER_WIDTH, GL_RENDERBUFFER_HEIGHT, GL_RENDERBUFFER_INTERNAL_FORMAT,
            GL_RENDERBUFFER_RED_SIZE, GL_RENDERBUFFER_GREEN_SIZE, GL_RENDERBUFFER_BLUE_SIZE, GL_RENDERBUFFER_ALPHA_SIZE};
        const GLint renderbufferExpected[] = {17, 19, GL_RGBA4, 4, 4, 4, 4};
        bool renderbufferOK = true;
        for (unsigned i = 0; i < 7; ++i) {
            GLint result = -1;
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, renderbufferQueries[i], &result);
            renderbufferOK &= expect(i < 3 ? result == renderbufferExpected[i] : result >= renderbufferExpected[i], "renderbuffer storage query");
        }
        glBindRenderbuffer(GL_RENDERBUFFER, 0); glDeleteRenderbuffers(1, &renderbuffer);
        if (!renderbufferOK) return false;

        GLfloat floatValues[4] = {-1, -1, -1, -1};
        GLint intValues[4] = {-1, -1, -1, -1};
        glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, floatValues);
        if (!expect(floatValues[0] == 0 && floatValues[1] == 0 && floatValues[2] == 0 && floatValues[3] == 1, "default vertex attribute value")) return false;
        glVertexAttrib4f(2, 2, 4, 6, 1);
        glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, floatValues);
        glGetVertexAttribiv(2, GL_CURRENT_VERTEX_ATTRIB, intValues);
        if (!expect(floatValues[0] == 2 && floatValues[1] == 4 && floatValues[2] == 6 && floatValues[3] == 1
            && intValues[0] == 2 && intValues[1] == 4 && intValues[2] == 6 && intValues[3] == 1, "float and integer vertex attribute values")) return false;
        GLuint buffer = 0;
        glGenBuffers(1, &buffer); glBindBuffer(GL_ARRAY_BUFFER, buffer);
        const GLfloat data[16] = {0};
        glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void*>(4));
        glEnableVertexAttribArray(2);
        const GLenum attributeQueries[] = {GL_VERTEX_ATTRIB_ARRAY_SIZE, GL_VERTEX_ATTRIB_ARRAY_TYPE, GL_VERTEX_ATTRIB_ARRAY_STRIDE,
            GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING};
        const GLint attributeExpected[] = {3, GL_FLOAT, 16, GL_FALSE, GL_TRUE, static_cast<GLint>(buffer)};
        bool attributeOK = true;
        for (unsigned i = 0; i < 6; ++i) {
            GLint result = -1; glGetVertexAttribiv(2, attributeQueries[i], &result);
            attributeOK &= expect(result == attributeExpected[i], "vertex attribute array state");
        }
        void* pointer = nullptr;
        glGetVertexAttribPointerv(2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
        attributeOK &= expect(pointer == reinterpret_cast<void*>(4), "vertex attribute buffer offset");
        glDisableVertexAttribArray(2); glBindBuffer(GL_ARRAY_BUFFER, 0); glDeleteBuffers(1, &buffer);
        if (!attributeOK) return false;
        glValidateProgram(program);
        GLint validated = -1; glGetProgramiv(program, GL_VALIDATE_STATUS, &validated);
        if (!expect(validated == GL_TRUE, "validate linked textured program")) return false;
        GLuint unlinked = glCreateProgram();
        glValidateProgram(unlinked);
        glGetProgramiv(unlinked, GL_VALIDATE_STATUS, &validated);
        glDeleteProgram(unlinked);
        if (!expect(validated == GL_FALSE, "unlinked program fails validation")) return false;
        return glOK("native GLES2 query and validation operations");
    }

    static void textureParameters()
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    static void uploadQuadrants(bool alpha)
    {
        const unsigned char pixels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, static_cast<unsigned char>(alpha ? 128 : 255)};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    void draw(GLuint texture)
    {
        static const GLfloat vertices[] = {-1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1};
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices + 2);
        glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    bool pixel(int x, int y, int r, int g, int b, int a, const char* description)
    {
        unsigned char actual[4] = {0};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual);
        if (!glOK("read framebuffer pixel")) return false;
        ++pixelChecks;
        report("PIXEL: %s xy=%d,%d got=%u,%u,%u,%u expected=%d,%d,%d,%d tolerance=3", description, x, y,
            actual[0], actual[1], actual[2], actual[3], r, g, b, a);
        return expect(abs(static_cast<int>(actual[0]) - r) <= 3 && abs(static_cast<int>(actual[1]) - g) <= 3
            && abs(static_cast<int>(actual[2]) - b) <= 3 && abs(static_cast<int>(actual[3]) - a) <= 3, description);
    }

    bool swap(const char* description)
    {
        if (!glOK("draw before swap") || !eglOK(eglSwapBuffers(display, surface), description)) return false;
        ++swaps;
        return true;
    }

    Window window;
    EGLDisplay display { EGL_NO_DISPLAY };
    EGLConfig config { 0 };
    EGLContext context { EGL_NO_CONTEXT };
    EGLSurface surface { EGL_NO_SURFACE };
    GLuint program { 0 }, source { 0 }, color { 0 }, framebuffer { 0 };
    bool initialized { false }, current { false };
};

static bool nextArgument(TLex& commandLine, TPtrC& token)
{
    commandLine.SkipSpace();
    if (commandLine.Eos())
        return false;
    if (commandLine.Peek() != '"') {
        token.Set(commandLine.NextToken());
        return true;
    }
    commandLine.Inc();
    commandLine.Mark();
    while (!commandLine.Eos() && commandLine.Peek() != '"')
        commandLine.Inc();
    token.Set(commandLine.MarkedToken());
    if (commandLine.Eos())
        return false;
    commandLine.Inc();
    return commandLine.Eos() || commandLine.Peek().IsSpace();
}

static bool readOptions(unsigned& holdMilliseconds, bool& requireES3Rejection)
{
    // SDK startup currently calls main with only a synthetic argv[0]. Read
    // the actual UTF-16 process command line, as the JSC/browser shells do.
    TBuf<256> line;
    if (User::CommandLineLength() > line.MaxLength())
        return false;
    User::CommandLine(line);
    TLex commandLine(line);
    commandLine.SkipSpace();
    while (!commandLine.Eos()) {
        TPtrC option;
        if (!nextArgument(commandLine, option))
            return false;
        if (option == _L("--expect-es3-rejection"))
            requireES3Rejection = true;
        else if (option == _L("--hold-ms")) {
            TPtrC argument;
            if (!nextArgument(commandLine, argument) || argument.Length() == 0)
                return false;
            TLex number(argument);
            TInt parsed = -1;
            if (number.Val(parsed) != KErrNone || !number.Eos() || parsed < 0 || parsed > 60000)
                return false;
            holdMilliseconds = static_cast<unsigned>(parsed);
        } else
            return false;
        commandLine.SkipSpace();
    }
    return true;
}

int main(int, char**)
{
    unsigned holdMilliseconds = 10000;
    bool requireES3Rejection = false;
    mkdir("C:/Data", 0777); mkdir("C:/Data/WebKit", 0777);
    output = fopen("C:/Data/WebKit/egl-probe.log", "w");
    if (!output) { report("FAIL: cannot create fresh probe log"); return 1; }
    if (!readOptions(holdMilliseconds, requireES3Rejection)) {
        report("FAIL: invalid process command line (expected --expect-es3-rejection and/or --hold-ms 0..60000)");
        report("egl-window-probe exiting 2");
        fclose(output);
        return 2;
    }
    report("BEGIN: egl-window-probe version=2 hold-ms=%u expect-es3-rejection=%u", holdMilliseconds, requireES3Rejection ? 1u : 0u);
    bool completed;
    {
        Probe probe;
        completed = probe.run(holdMilliseconds, requireES3Rejection);
    }
    expect(completed && swaps == 3 && pixelChecks == 9, "all native rendering stages completed");
    report("RESULT {\"probe\":\"egl-window-probe\",\"version\":2,\"passed\":%u,\"failed\":%u,\"swaps\":%u,\"pixelChecks\":%u,\"holdMilliseconds\":%u,\"expectES3Rejection\":%s}", checks - failures, failures, swaps, pixelChecks, holdMilliseconds, requireES3Rejection ? "true" : "false");
    report("%s: egl-window-probe %u/%u", failures ? "FAIL" : "PASS", checks - failures, checks);
    report("egl-window-probe exiting %d", failures ? 1 : 0);
    fclose(output);
    return failures ? 1 : 0;
}
