// window.c
#include "window.h"

// Grid update functions
void UpdateCameraOffset(Camera2D *camera, int newWidth, int newHeight) {
  camera->offset = (Vector2){newWidth / 2.0f, newHeight / 2.0f};
}

void HandleWindowResize(WindowState *windowState, Camera2D *camera) {
  // Get current window dimensions
  int newWidth = GetScreenWidth();
  int newHeight = GetScreenHeight();

  // Only update if dimensions actually changed
  if (newWidth != windowState->width || newHeight != windowState->height) {
    windowState->width = newWidth;
    windowState->height = newHeight;
    UpdateCameraOffset(camera, newWidth, newHeight);
  }
}
