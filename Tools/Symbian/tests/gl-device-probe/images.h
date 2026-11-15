#pragma once

#include <EGL/egl.h>

using ProbeReport = void (*)(const char* format, ...);

// RSgImage and GL texture EGLImages, and EGL_NOK_image_endpoint2 on surface manager surfaces,
// in a pbuffer context of config.
void probeImages(EGLDisplay, EGLConfig, ProbeReport);
