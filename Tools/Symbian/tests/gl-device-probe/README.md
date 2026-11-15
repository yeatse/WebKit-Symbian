# GL device probe

A windowed executable that reports what a phone's EGL and GLES2 driver gives
Skia, in one run: the EGL configurations and the ones WebKit chooses, the GL
functions Skia finds, the requirement of `GrGLInterface::validate()` that fails
if one does, Ganesh on a pbuffer context and on a window context, and one
context drawing into both its pbuffer and the window. It then makes EGLImages
from RSgImages (pixels in, a GL render target shared with a second context, a
pixmap surface) and from the buffers of a surface manager surface through an
`EGL_NOK_image_endpoint2` consumer endpoint, reads each back, and times a
512x512 upload by `glTexSubImage2D` against an RSgImage and a 640x360 endpoint
frame.

The CI packages it as `glprobe.sis` (artifact `probe`). Start "glprobe" from
the applications menu; it draws for a few seconds, exits by itself and leaves
`C:\Data\WebKit\glprobe.log`.

`failing-line` in the log is a line of the generated `validate.inc`; the same
requirement is 76 lines further down in Skia's `GrGLInterfaceAutogen.cpp`.
