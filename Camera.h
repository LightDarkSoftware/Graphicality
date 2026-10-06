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

    enum Cameratype {
        FREE_CAMERA = 0,
        ORBIT_CAMERA = 1,
        FP_CAMERA = 3
    };

    static Cameratype cameratype;
};

#endif
