// window.h
#ifndef WINDOW_H
#define WINDOW_H

// includes
#include <raylib.h>

// definitions
#define CAMERA_SPEED 300.0f

// structs
typedef struct CameraState {
  bool isPanning;
  Vector2 lastMousePosition;
} CameraState;

typedef struct WindowState {
  int width;
  int height;
} WindowState;

// functions
void UpdateCameraOffset(Camera2D *camera, int newWidth, int newHeight);

void HandleWindowResize(WindowState *windowState, Camera2D *camera);

#endif // WINDOW_H
