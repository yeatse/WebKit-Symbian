// Loader selection regression; the values below are test fixtures, not driver capabilities.
#include <glad/glad.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *version;
static int compatibility;
static int missingPrecision;
static unsigned queries;
static const GLubyte *APIENTRY strings(GLenum name) {
    return (const GLubyte *)(name == GL_VERSION ? version : "GL_dummy_extension");
}
static const GLubyte *APIENTRY string_index(GLenum name, GLuint index) {
    (void)name; (void)index;
    return (const GLubyte *)(compatibility ? "GL_ARB_ES2_compatibility" : "GL_dummy_extension");
}
static void APIENTRY integers(GLenum name, GLint *out) { assert(name == GL_NUM_EXTENSIONS); *out = 1; }
static GLenum APIENTRY error(void) { return GL_NO_ERROR; }
static void APIENTRY precision(GLenum shader, GLenum type, GLint *range, GLint *bits) {
    assert(shader == GL_VERTEX_SHADER && type == GL_HIGH_FLOAT);
    range[0] = 13; range[1] = 17; *bits = 19; ++queries;
}
static void APIENTRY unused(void) {}
static void *load(const char *name) {
    if (!strcmp(name,"glGetString")) return (void *)strings;
    if (!strcmp(name,"glGetStringi")) return (void *)string_index;
    if (!strcmp(name,"glGetIntegerv")) return (void *)integers;
    if (!strcmp(name,"glGetError")) return (void *)error;
    if (!strcmp(name,"glGetShaderPrecisionFormat")) return missingPrecision ? NULL : (void *)precision;
    return (void *)unused;
}
static void check(const char *ver, int extension, int available) {
    version=ver;compatibility=extension;
    assert(gladLoadGLLoader(load));
    assert(GLAD_GL_ARB_ES2_compatibility == extension);
    assert(!!glad_glGetShaderPrecisionFormat == available);
    assert(glad_debug_glGetShaderPrecisionFormat); /* A wrapper alone is not API availability. */
    if (available) {
        GLint range[2]={0},bits=0;
        glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_HIGH_FLOAT,range,&bits);
        assert(range[0]==13 && range[1]==17 && bits==19);
    }
    printf("PASS: desktop %s extension=%d available=%d\n",ver,extension,available);
}
int main(void) {
    check("3.3",0,0);check("4.0",0,0);check("3.3",1,1);check("4.1",0,1);check("4.5",1,1);
    check("3.3",0,0); /* Switching to an unsupported context clears stale pointers. */
    missingPrecision=1;check("4.5",1,0);
    assert(queries==3);
    puts("PASS: precision loader 7/7, exact forwarded values");
}
