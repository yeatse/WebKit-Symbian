// Small real ANGLE validation + GLES renderer probe; native GL is never linked.
#include "libGLESv2/entry_points_egl_autogen.h"
#include "libGLESv2/entry_points_egl_ext_autogen.h"
#include "libGLESv2/entry_points_gles_2_0_autogen.h"
#include <e32debug.h>
#include <atomic>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

static FILE* output;
static unsigned checks, failures;
static void report(const char* format, ...)
{
    char line[2048]; va_list args; va_start(args, format);
    vsnprintf(line, sizeof(line), format, args); va_end(args);
    TUint16 units[512]; unsigned n = 0;
    while (line[n] && n < 511) { units[n] = static_cast<unsigned char>(line[n]); ++n; }
    RDebug::RawPrint(TPtrC16(units, n));
    if (output) { fprintf(output, "%s\n", line); fflush(output); }
}
static bool expect(bool value, const char* what)
{
    ++checks; failures += !value; report("%s: %s", value ? "PASS" : "FAIL", what); return value;
}
static bool noError(const char* what)
{
    GLenum error = GL_GetError();
    if (error) report("GL ERROR 0x%x at %s", error, what);
    return expect(error == GL_NO_ERROR, what);
}
static GLuint shader(GLenum type, const char* source)
{
    GLuint object = GL_CreateShader(type); GL_ShaderSource(object, 1, &source, nullptr); GL_CompileShader(object);
    GLint status = 0; GL_GetShaderiv(object, GL_COMPILE_STATUS, &status);
    if (!status) { char log[1024]; GL_GetShaderInfoLog(object, sizeof(log), nullptr, log); report("SHADER: %s", log); }
    expect(status == GL_TRUE, type == GL_VERTEX_SHADER ? "vertex shader" : "fragment shader"); return object;
}
struct ThreadCheck {
    std::atomic<unsigned> phase { 0 };
    bool noContext { false };
    EGLint error { EGL_SUCCESS };
};
static void* threadCheck(void* pointer)
{
    auto& check = *static_cast<ThreadCheck*>(pointer);
    check.noContext = EGL_GetCurrentContext() == EGL_NO_CONTEXT;
    EGL_BindAPI(0xdead);
    check.phase.store(1, std::memory_order_release);
    for (unsigned wait = 0; check.phase.load(std::memory_order_acquire) != 2 && wait < 300; ++wait)
        User::After(10000);
    check.error = EGL_GetError();
    EGL_ReleaseThread();
    return nullptr;
}
static void verifyThreadIsolation(EGLContext context)
{
    ThreadCheck check; pthread_t thread;
    if (!expect(pthread_create(&thread,nullptr,threadCheck,&check) == 0,"ANGLE worker thread creation")) return;
    for (unsigned wait = 0; check.phase.load(std::memory_order_acquire) != 1 && wait < 300; ++wait)
        User::After(10000);
    expect(check.phase.load(std::memory_order_acquire) == 1,"worker TLS test reached barrier");
    expect(EGL_GetError() == EGL_SUCCESS,"worker EGL error isolated from main thread");
    expect(EGL_GetCurrentContext() == context,"main EGL current context preserved across worker");
    check.phase.store(2, std::memory_order_release);
    expect(pthread_join(thread,nullptr) == 0,"ANGLE worker thread joined");
    expect(check.noContext && check.error == EGL_BAD_PARAMETER,"worker has independent context and error state");
}
static bool run()
{
    report("INFO ANGLE client extensions=%s", EGL_QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    const EGLint displayAttributes[] = { EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 2, EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 0, EGL_NONE };
    EGLDisplay display = EGL_GetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayAttributes);
    report("INFO ANGLE display=%p EGLerror=0x%x type=OpenGLES device=EGL max-version=2.0", display, EGL_GetError());
    if (!expect(display != EGL_NO_DISPLAY, "ANGLE display")) return false;
    EGLint major = 0, minor = 0;
    bool initialized = EGL_Initialize(display, &major, &minor);
    report("INFO initialize=%d EGLerror=0x%x version=%d.%d", initialized, EGL_GetError(), major, minor);
    if (!expect(initialized, "ANGLE initialize over native EGL")) return false;
    report("INFO EGL vendor=%s extensions=%s", EGL_QueryString(display, EGL_VENDOR), EGL_QueryString(display, EGL_EXTENSIONS));
    if (!expect(EGL_BindAPI(EGL_OPENGL_ES_API), "bind GLES API")) return false;
    const EGLint configAttributes[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
    EGLConfig config; EGLint count = 0;
    if (!expect(EGL_ChooseConfig(display, configAttributes, &config, 1, &count) && count, "RGBA8 pbuffer config")) return false;
    const EGLint surfaceAttributes[] = { EGL_WIDTH, 8, EGL_HEIGHT, 8, EGL_NONE };
    EGLSurface surface = EGL_CreatePbufferSurface(display, config, surfaceAttributes);
    if (!expect(surface != EGL_NO_SURFACE, "ANGLE pbuffer")) return false;
    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE, EGL_TRUE,
        EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE, EGL_FALSE, EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE, EGL_TRUE,
        EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE, EGL_FALSE, EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM, EGL_FALSE, EGL_NONE };
    EGLContext context = EGL_CreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (!expect(context != EGL_NO_CONTEXT, "WebGL-compatible ANGLE context")) { report("EGLerror=0x%x", EGL_GetError()); return false; }
    if (!expect(EGL_MakeCurrent(display, surface, surface, context), "ANGLE make current")) return false;
    report("INFO GL vendor=%s renderer=%s version=%s", GL_GetString(GL_VENDOR), GL_GetString(GL_RENDERER), GL_GetString(GL_VERSION));
    auto* renderer = reinterpret_cast<const char*>(GL_GetString(GL_RENDERER));
    expect(renderer && strstr(renderer, "ANGLE"), "renderer identifies ANGLE");
    GLuint vs = shader(GL_VERTEX_SHADER, "attribute vec2 p; void main(){gl_Position=vec4(p,0.,1.);}");
    GLuint fs = shader(GL_FRAGMENT_SHADER, "precision mediump float; void main(){gl_FragColor=vec4(1.,0.,0.,1.);}");
    GLuint program = GL_CreateProgram(); GL_AttachShader(program, vs); GL_AttachShader(program, fs); GL_BindAttribLocation(program, 0, "p"); GL_LinkProgram(program);
    GLint linked = 0; GL_GetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!expect(linked == GL_TRUE, "shader program link")) { char log[1024]; GL_GetProgramInfoLog(program,sizeof(log),nullptr,log); report("PROGRAM: %s",log); return false; }
    GL_UseProgram(program);
    const GLfloat vertices[] = {-1,-1, 1,-1, -1,1, 1,1};
    GLuint buffer; GL_GenBuffers(1, &buffer); GL_BindBuffer(GL_ARRAY_BUFFER, buffer); GL_BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL_VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr); GL_EnableVertexAttribArray(0);
    GLuint texture; GL_GenTextures(1,&texture); GL_BindTexture(GL_TEXTURE_2D,texture);
    GL_TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    GLuint fbo; GL_GenFramebuffers(1,&fbo); GL_BindFramebuffer(GL_FRAMEBUFFER,fbo); GL_FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    if (!expect(GL_CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"RGBA8 framebuffer complete")) return false;
    GL_Viewport(0,0,8,8); GL_ClearColor(0,0,0,0); GL_Clear(GL_COLOR_BUFFER_BIT); GL_DrawArrays(GL_TRIANGLE_STRIP,0,4);
    unsigned char pixel[4] = {0}; GL_ReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    report("PIXEL %u %u %u %u",pixel[0],pixel[1],pixel[2],pixel[3]);
    expect(pixel[0]==255 && pixel[1]==0 && pixel[2]==0 && pixel[3]==255,"shader draw exact red pixel"); noError("draw and readback");
    EGLContext second = EGL_CreateContext(display,config,EGL_NO_CONTEXT,contextAttributes);
    if (!expect(second != EGL_NO_CONTEXT,"second isolated ANGLE context")) return false;
    if (!expect(EGL_MakeCurrent(display,surface,surface,second),"switch to second context")) return false;
    GL_ClearColor(0,0,1,1); GL_Clear(GL_COLOR_BUFFER_BIT); noError("second context blue clear");
    if (!expect(EGL_MakeCurrent(display,surface,surface,context),"restore first context")) return false;
    GL_ReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    expect(pixel[0]==255 && pixel[1]==0 && pixel[2]==0 && pixel[3]==255,"virtual context restores FBO and red pixel");
    expect(EGL_DestroyContext(display,second),"destroy inactive second context");
    verifyThreadIsolation(context);
    GL_BindTexture(GL_TEXTURE_2D, 0x00ffffff); expect(GL_GetError()==GL_INVALID_OPERATION,"WebGL rejects ungenerated texture name");
    GL_BindBuffer(GL_ARRAY_BUFFER,0); GL_VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,vertices);
    expect(GL_GetError()==GL_INVALID_OPERATION,"WebGL rejects client vertex arrays");
    GL_DeleteFramebuffers(1,&fbo); GL_DeleteTextures(1,&texture); GL_DeleteBuffers(1,&buffer); GL_DeleteProgram(program); GL_DeleteShader(vs); GL_DeleteShader(fs);
    noError("resource deletion");
    expect(EGL_MakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT),"unmake ANGLE context");
    expect(EGL_DestroyContext(display,context),"destroy ANGLE context"); expect(EGL_DestroySurface(display,surface),"destroy ANGLE surface");
    expect(EGL_Terminate(display),"terminate ANGLE display"); EGL_ReleaseThread();
    return true;
}
int main()
{
    mkdir("C:/Data",0777); mkdir("C:/Data/WebKit",0777);
    output=fopen("C:/Data/WebKit/angle-probe.log","w"); if(!output) return 2;
    report("BEGIN: angle-probe version=1"); bool completed=run(); expect(completed,"all stages completed");
    report("RESULT {\"probe\":\"angle-probe\",\"passed\":%u,\"failed\":%u}",checks-failures,failures);
    report("%s: angle-probe %u/%u",failures?"FAIL":"PASS",checks-failures,checks);
    report("angle-probe exiting %d",failures?1:0); fclose(output); return failures?1:0;
}
