#pragma once

#include <GL/glew.h>

// Initialize post-process subsystem (compile shaders, create screen quad)
void ppInit();
// Resize or (re)create textures/FBOs for window size
void ppResize(int width, int height);
// Begin rendering the scene into the scene FBO
void ppBeginScene();
// End rendering scene (unbind FBO)
void ppEndScene();
// Run bloom passes and present final image to default framebuffer
void ppApplyAndPresent(float bloomThreshold, float bloomIntensity, int blurPasses);

// Optional cleanup (not required for this task)
void ppShutdown();
