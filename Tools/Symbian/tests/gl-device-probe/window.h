#pragma once

// Returns the RWindow to hand to eglCreateWindowSurface, or null.
void* probeOpenWindow(int* width, int* height);
void probePumpWindow(unsigned milliseconds);
void probeCloseWindow();
