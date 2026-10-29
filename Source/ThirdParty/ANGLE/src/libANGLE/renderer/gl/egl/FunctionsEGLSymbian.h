// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBANGLE_RENDERER_GL_EGL_FUNCTIONSEGLSYMBIAN_H_
#define LIBANGLE_RENDERER_GL_EGL_FUNCTIONSEGLSYMBIAN_H_

#include "libANGLE/renderer/gl/egl/FunctionsEGL.h"

#include <memory>

namespace rx
{
// Resolve firmware exports directly, without the public names exported by ANGLE.
class FunctionsEGLSymbian final : public FunctionsEGL
{
  public:
    FunctionsEGLSymbian();
    ~FunctionsEGLSymbian() override;

    egl::Error initialize(EGLAttrib platformType, EGLNativeDisplayType nativeDisplay);
    void *getProcAddress(const char *name) const override;

  private:
    struct Libraries;
    std::unique_ptr<Libraries> mLibraries;
};
}  // namespace rx

#endif
