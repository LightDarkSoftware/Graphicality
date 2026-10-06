#ifndef CAMERA_H
#define CAMERA_H

typedef struct {
    struct { float x, y, z; } Position;
    float yaw;
    float pitch;
    struct { float x, y, z; } RightVector;
    struct { float x, y, z; } UpVector;
    struct { float x, y, z; } LookVector;
} CFrame;

extern float FieldOfView;
extern CFrame CameraCFrame;

void InitCamera(void);

#endif