#ifndef CAMERA_H
#define CAMERA_H

class Camera {
public:
    static float FieldOfView;

    static struct {
        struct { float x, y, z; } Position;
        float yaw;
        float pitch;
        struct { float x, y, z; } RightVector;
        struct { float x, y, z; } UpVector;
        struct { float x, y, z; } LookVector;
    } CFrame;
};

#endif
