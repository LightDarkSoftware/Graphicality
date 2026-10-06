#include "Camera.h"

float FieldOfView = 70.0f;

CFrame CameraCFrame = {
    .Position = {0.0f, 9.0f, 4.0f},
    .yaw = 0.0f,
    .pitch = 0.0f,
    .RightVector = {1.0f, 0.0f, 0.0f},
    .UpVector = {0.0f, 0.7f, -0.7f},
    .LookVector = {0.0f, -0.7f, -0.7f}
};

void InitCamera(void) {
    FieldOfView = 70.0f;
}