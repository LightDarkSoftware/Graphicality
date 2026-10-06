// Copyright © 2026 Graphicality, LightDark Software SP. All rights reserved.
// Graphicality Engine is propriatary software, All contents of Graphicality must not be reproduced.
// Source may be used for educational purposes.

#include <Windows.h>
#include <iostream>
#include <cmath>
#include <vector>
#include "Camera.h"

double radians(double degrees) {
    return degrees * (3.1415926535 / 180.0);
}

int currentWidth = 800;
int currentHeight = 600;

bool keys[256] = { false };

struct Pixel { unsigned char r, g, b, a; } ;
struct Vector3 { float x, y, z; } ;
struct Vector2 { float x, y; } ;
struct Colour3 { float r, g, b; } ;

struct StructPart {
    Vector3 position;
    Vector3 size;
    Colour3 colour;
    Vector3 linearVelocity;
    bool anchored;
};

Vector2 lastMouse;

Pixel* frameBuffer = NULL;
float* zBuffer = NULL;

std::vector<StructPart> cubes;

int Humanoid;
float cameraDistance = 5.0f;

StructPart CreateCube(Vector3 position, Vector3 size, Colour3 colour, bool anchored) {
    return StructPart{position, size, colour, {0.0f, 0.0f, 0.0f}, anchored};
}

float vertices[8][3] = {
    { 0.5f,  0.5f, -0.5f},
    {-0.5f,  0.5f, -0.5f},
    { 0.5f, -0.5f, -0.5f},
    {-0.5f, -0.5f, -0.5f},
    { 0.5f,  0.5f,  0.5f},
    {-0.5f,  0.5f,  0.5f},
    { 0.5f, -0.5f,  0.5f},
    {-0.5f, -0.5f,  0.5f}
};

int edges[12][2] = {
    {0, 1},
    {2, 3},
    {0, 2},
    {1, 3},
    {4, 5},
    {6, 7},
    {4, 6},
    {5, 7},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7}
};

int faces[12][3] = {
    {4, 6, 5}, {5, 6, 7},
    {1, 3, 0}, {0, 3, 2},
    {5, 7, 1}, {1, 7, 3},
    {0, 2, 4}, {4, 2, 6},
    {5, 1, 4}, {4, 1, 0},
    {2, 3, 6}, {6, 3, 7}
};

void clearScreen() {
    memset(frameBuffer, 0, currentWidth * currentHeight * sizeof(Pixel));
    if (zBuffer != NULL) {
        for (int i = 0; i < currentWidth * currentHeight; i++) {
            zBuffer[i] = 10000.0f;
        }
    }
    for (int i = 0; i < (currentWidth * currentHeight); i++) {
        frameBuffer[i].b = 70;
        frameBuffer[i].g = 100;
        frameBuffer[i].r = 200;
        frameBuffer[i].a = 255;
    }
}

float crossProduct(float ax, float ay, float bx, float by, float cx, float cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

void rasterizeTriangle(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, Colour3 colour1, Colour3 colour2, Colour3 colour3) {
    float area = crossProduct(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;

    int minX = (int)floorf(fmaxf(0.0f, fminf(x0, fminf(x1, x2))));
    int maxX = (int)ceilf(fminf((float)(currentWidth - 1), fmaxf(x0, fmaxf(x1, x2))));
    int minY = (int)floorf(fmaxf(0.0f, fminf(y0, fminf(y1, y2))));
    int maxY = (int)ceilf(fminf((float)(currentHeight - 1), fmaxf(y0, fmaxf(y1, y2))));

    colour1.r /= z0; colour1.g /= z0; colour1.b /= z0;
    colour2.r /= z1; colour2.g /= z1; colour2.b /= z1;
    colour3.r /= z2; colour3.g /= z2; colour3.b /= z2;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            float w0 = crossProduct(x1, y1, x2, y2, (float)x, (float)y) / area;
            float w1 = crossProduct(x2, y2, x0, y0, (float)x, (float)y) / area;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float invertedZ = (w0 / z0) + (w1 / z1) + (w2 / z2);
                float z = 1.0f / invertedZ;

                int index = y * currentWidth + x;
                if (z <= zBuffer[index]) {
                    zBuffer[index] = z;
                    float correction = z * 255.0f;

                    frameBuffer[index].r = (unsigned char)((w0 * colour1.r + w1 * colour2.r + w2 * colour3.r) * correction);
                    frameBuffer[index].g = (unsigned char)((w0 * colour1.g + w1 * colour2.g + w2 * colour3.g) * correction);
                    frameBuffer[index].b = (unsigned char)((w0 * colour1.b + w1 * colour2.b + w2 * colour3.b) * correction);
                    frameBuffer[index].a = 255.0f;
                }
            }
        }
    }
}

void renderCube(Vector3 position, Vector3 size, Colour3 baseColour) {
    float scaleFactor = currentHeight / (2.0f * tanf(radians(Camera::FieldOfView) / 2.0f));
    float nearPlaneZ = 0.5f;
    Vector3 lightPosition = {10.0f, 20.0f, -10.0f};
    float ambientIntensity = 0.5f;

    Vector3 globalVertices[8];
    Colour3 vertexColours[8];

    for (int i = 0; i < sizeof(vertices) / sizeof(vertices[0]); i++) {
        Vector3 localVertex = {
            vertices[i][0] * size.x,
            vertices[i][1] * size.y,
            vertices[i][2] * size.z
        };

        // Get position relative to world
        Vector3 worldPosition = {
            position.x + localVertex.x,
            position.y + localVertex.y,
            position.z + localVertex.z
        };

        // Normal
        Vector3 localNormal = {
            vertices[i][0],
            vertices[i][1],
            vertices[i][2]
        };

        float normalMagnitude = sqrtf(localNormal.x*localNormal.x + localNormal.y*localNormal.y + localNormal.z*localNormal.z);

        Vector3 worldNormal = {
            localNormal.x / normalMagnitude,
            localNormal.y / normalMagnitude,
            localNormal.z / normalMagnitude
        };

        // Lighting
        Vector3 lightDirection = {
            lightPosition.x - worldPosition.x,
            lightPosition.y - worldPosition.y,
            lightPosition.z - worldPosition.z
        };

        float lightMagnitude = sqrtf(lightDirection.x*lightDirection.x + lightDirection.y*lightDirection.y + lightDirection.z*lightDirection.z);

        if (lightMagnitude > 0.0f) {
            lightDirection.x /= lightMagnitude; lightDirection.y /= lightMagnitude; lightDirection.z /= lightMagnitude;
        }

        float dot = (worldNormal.x * lightDirection.x) + (worldNormal.y * lightDirection.y) + (worldNormal.z * lightDirection.z);
        float intensity = dot > 0.0f ? dot : 0.0f;
        float final = ambientIntensity + (1.0f - ambientIntensity) * intensity;

        vertexColours[i].r = baseColour.r * final;
        vertexColours[i].g = baseColour.g * final;
        vertexColours[i].b = baseColour.b * final;

        // Get position relative to camera
        Vector3 offset = {
            worldPosition.x - Camera::CFrame.Position.x,
            worldPosition.y - Camera::CFrame.Position.y,
            worldPosition.z - Camera::CFrame.Position.z
        };

        globalVertices[i].x = (offset.x * Camera::CFrame.RightVector.x) + (offset.y * Camera::CFrame.RightVector.y) + (offset.z * Camera::CFrame.RightVector.z);
        globalVertices[i].y = (offset.x * Camera::CFrame.UpVector.x) + (offset.y * Camera::CFrame.UpVector.y) + (offset.z * Camera::CFrame.UpVector.z);
        globalVertices[i].z = (offset.x * Camera::CFrame.LookVector.x) + (offset.y * Camera::CFrame.LookVector.y) + (offset.z * Camera::CFrame.LookVector.z);
    }

    for (int i = 0; i < 12; i++) {
        int index0 = faces[i][0];
        int index1 = faces[i][1];
        int index2 = faces[i][2];

        Vector3 vertex[3] = {
            globalVertices[index0],
            globalVertices[index1],
            globalVertices[index2]
        };

        Colour3 colour[3] = {
            vertexColours[index0],
            vertexColours[index1],
            vertexColours[index2]
        };

        Vector3 clipped[4];
        Colour3 clippedColour[4];
        int clippedCount = 0;

        for (int j = 0; j < 3; j++) {
            int next = (j+1) % 3;

            Vector3 currentVertex = vertex[j];
            Vector3 nextVertex = vertex[next];
            Colour3 currentColour = colour[j];
            Colour3 nextColour = colour[next];

            bool currentInside = currentVertex.z >= nearPlaneZ;
            bool nextInside = nextVertex.z >= nearPlaneZ;

            if (currentInside) {
                clipped[clippedCount] = currentVertex;
                clippedColour[clippedCount] = currentColour;
                clippedCount++;
            }

            if (currentInside != nextInside) {
                float t = (nearPlaneZ - currentVertex.z) / (nextVertex.z - currentVertex.z);

                Vector3 intersection = {
                    currentVertex.x + (nextVertex.x - currentVertex.x) * t,
                    currentVertex.y + (nextVertex.y - currentVertex.y) * t,
                    nearPlaneZ
                };

                Colour3 intersectionColour = {
                    currentColour.r + (nextColour.r - currentColour.r) * t,
                    currentColour.g + (nextColour.g - currentColour.g) * t,
                    currentColour.b + (nextColour.b - currentColour.b) * t
                };

                clipped[clippedCount] = intersection;
                clippedColour[clippedCount] = intersectionColour;
                clippedCount++;
            }
        }

        if (clippedCount < 3) continue;
        Vector3 screen[4];

        for (int j = 0; j < clippedCount; j++) {
            screen[j].x = (clipped[j].x * scaleFactor) / clipped[j].z + (currentWidth / 2.0f);
            screen[j].y = -(clipped[j].y * scaleFactor) / clipped[j].z + (currentHeight / 2.0f);
            screen[j].z = clipped[j].z;
        }

        if (clippedCount == 3) {
            float area = crossProduct(
                screen[0].x, screen[0].y,
                screen[1].x, screen[1].y,
                screen[2].x, screen[2].y
            );

            if (area > 0.0f) {
                rasterizeTriangle(
                    screen[0].x, screen[0].y, screen[0].z,
                    screen[1].x, screen[1].y, screen[1].z,
                    screen[2].x, screen[2].y, screen[2].z,
                    clippedColour[0], clippedColour[1], clippedColour[2]
                );
            }
        }

        else if (clippedCount == 4) {
            float area1 = crossProduct(
                screen[0].x, screen[0].y,
                screen[1].x, screen[1].y,
                screen[2].x, screen[2].y
            );

            if (area1 > 0.0f) {
                rasterizeTriangle(
                    screen[0].x, screen[0].y, screen[0].z,
                    screen[1].x, screen[1].y, screen[1].z,
                    screen[2].x, screen[2].y, screen[2].z,
                    clippedColour[0], clippedColour[1], clippedColour[2]
                );
            }

            float area2 = crossProduct(
                screen[0].x, screen[0].y,
                screen[2].x, screen[2].y,
                screen[3].x, screen[3].y
            );

            if (area2 > 0.0f) {
                rasterizeTriangle(
                    screen[0].x, screen[0].y, screen[0].z,
                    screen[2].x, screen[2].y, screen[2].z,
                    screen[3].x, screen[3].y, screen[3].z,
                    clippedColour[0], clippedColour[2], clippedColour[3]
                );
            }
        }
    }
}

int addCube(Vector3 position, Vector3 size, Colour3 colour, bool anchored) {
    cubes.push_back(CreateCube(position, size, colour, anchored));
    return (int)cubes.size() - 1;
}

void renderScene() {
    for (int i = 0; i < cubes.size(); i++) {
        const StructPart& cube = cubes[i];
        renderCube(cube.position, cube.size, cube.colour);
    }
}

void processPhysics(float deltaTime) {
    for (int i = 0; i < cubes.size(); i++) {
        StructPart* cube = &cubes[i];
        if (cube->anchored) {
            continue;
        }

        cube->linearVelocity.x *= expf(-10.0f * deltaTime);
        cube->linearVelocity.z *= expf(-10.0f * deltaTime);

        float gravity = -50.0f;
        //cube->linearVelocity.y += gravity * deltaTime;

        cube->position.x += cube->linearVelocity.x * deltaTime;
        cube->position.y += cube->linearVelocity.y * deltaTime;
        cube->position.z += cube->linearVelocity.z * deltaTime;
    }
}

void rotateCamera(float yaw, float pitch) {
    float lx = sinf(yaw) * cosf(pitch);
    float ly = sinf(pitch);
    float lz = -cosf(yaw) * cosf(pitch);
    Vector3 look = {lx, ly, lz};

    float rx = cosf(yaw);
    float ry = 0.0f;
    float rz = sinf(yaw);
    Vector3 right = {rx, ry, rz};

    float ux = (ry * lz) - (rz * ly);
    float uy = (rz * lx) - (rx * lz);
    float uz = (rx * ly) - (ry * lx);
    Vector3 up = {ux, uy, uz};

    Camera::CFrame.RightVector.x = right.x;
    Camera::CFrame.RightVector.y = right.y;
    Camera::CFrame.RightVector.z = right.z;

    Camera::CFrame.UpVector.x = up.x;
    Camera::CFrame.UpVector.y = up.y;
    Camera::CFrame.UpVector.z = up.z;

    Camera::CFrame.LookVector.x = look.x;
    Camera::CFrame.LookVector.y = look.y;
    Camera::CFrame.LookVector.z = look.z;
}

void CameraOrbit(float yaw, float pitch, int player) {
    float lx = sinf(yaw) * cosf(pitch);
    float ly = sinf(pitch);
    float lz = -cosf(yaw) * cosf(pitch);
    Vector3 look = {lx, ly, lz};

    float rx = cosf(yaw);
    float ry = 0.0f;
    float rz = sinf(yaw);
    Vector3 right = {rx, ry, rz};

    float ux = (ry * lz) - (rz * ly);
    float uy = (rz * lx) - (rx * lz);
    float uz = (rx * ly) - (ry * lx);
    Vector3 up = {ux, uy, uz};

    Camera::CFrame.RightVector.x = right.x;
    Camera::CFrame.RightVector.y = right.y;
    Camera::CFrame.RightVector.z = right.z;

    Camera::CFrame.UpVector.x = up.x;
    Camera::CFrame.UpVector.y = up.y;
    Camera::CFrame.UpVector.z = up.z;

    Camera::CFrame.LookVector.x = look.x;
    Camera::CFrame.LookVector.y = look.y;
    Camera::CFrame.LookVector.z = look.z;

    Camera::CFrame.Position.x = cubes[player].position.x - (Camera::CFrame.LookVector.x * cameraDistance);
    Camera::CFrame.Position.y = cubes[player].position.y - (Camera::CFrame.LookVector.y * cameraDistance);
    Camera::CFrame.Position.z = cubes[player].position.z - (Camera::CFrame.LookVector.z * cameraDistance);
}

void mouseHandler(float mouseX, float mouseY) {
    float dx = (mouseX - lastMouse.x) * 0.005f;
    float dy = (mouseY - lastMouse.y) * 0.005f;
    Camera::CFrame.yaw += dx;
    Camera::CFrame.pitch = fmax(radians(-80), fmin(radians(80), Camera::CFrame.pitch - dy));
    lastMouse.x = mouseX;
    lastMouse.y = mouseY;
    rotateCamera(Camera::CFrame.yaw, Camera::CFrame.pitch);
    //CameraOrbit(Camera::CFrame.yaw, Camera::CFrame.pitch, Humanoid);
}

void playerMovement(float deltaTime, int player) {
	Vector3 LookVector = {Camera::CFrame.LookVector.x, Camera::CFrame.LookVector.y, Camera::CFrame.LookVector.z};
    float FLength = sqrtf((LookVector.x * LookVector.x) + (LookVector.z * LookVector.z));
	Vector3 forward = {LookVector.x, 0.0f, LookVector.z};
    if (FLength > 0.0f) {
        forward.x /= FLength;
        forward.z /= FLength;
    }

	Vector3 RightVector = {Camera::CFrame.RightVector.x, Camera::CFrame.RightVector.y, Camera::CFrame.RightVector.z};
    float rLength = sqrtf((RightVector.x * RightVector.x) + (RightVector.y * RightVector.y) + (RightVector.z * RightVector.z));
	Vector3 right = {RightVector.x, 0.0f, RightVector.z};

	float X = cubes[player].linearVelocity.x;
	float Z = cubes[player].linearVelocity.z;

	if (keys['W']) {
		X = X + (forward.x * 200.0f * deltaTime);
		Z = Z + (forward.z * 200.0f * deltaTime);
    }
	if (keys['S']) {
		X = X - (forward.x * 200.0f * deltaTime);
		Z = Z - (forward.z * 200.0f * deltaTime);
    }
	if (keys['A']) {
		X = X - (right.x * 200.0f * deltaTime);
		Z = Z - (right.z * 200.0f * deltaTime);
    }
	if (keys['D']) {
		X = X + (right.x * 200.0f * deltaTime);
		Z = Z + (right.z * 200.0f * deltaTime);
    }
	
    cubes[player].linearVelocity = {X, cubes[player].linearVelocity.y, Z};
    CameraOrbit(Camera::CFrame.yaw, Camera::CFrame.pitch, player);
}

void processMovement(float deltaTime) {
    float cameraSpeed = 30.0f * deltaTime;

    Vector3 look = {Camera::CFrame.LookVector.x, Camera::CFrame.LookVector.y, Camera::CFrame.LookVector.z};
    Vector3 right = {Camera::CFrame.RightVector.x, Camera::CFrame.RightVector.y, Camera::CFrame.RightVector.z};
    Vector3 up = {Camera::CFrame.UpVector.x, Camera::CFrame.UpVector.y, Camera::CFrame.UpVector.z};

    if (keys['W']) {
        Camera::CFrame.Position.x += look.x * cameraSpeed;
        Camera::CFrame.Position.y += look.y * cameraSpeed;
        Camera::CFrame.Position.z += look.z * cameraSpeed;
    }
    if (keys['S']) {
        Camera::CFrame.Position.x -= look.x * cameraSpeed;
        Camera::CFrame.Position.y -= look.y * cameraSpeed;
        Camera::CFrame.Position.z -= look.z * cameraSpeed;
    }
    if (keys['A']) {
        Camera::CFrame.Position.x -= right.x * cameraSpeed;
        Camera::CFrame.Position.y -= right.y * cameraSpeed;
        Camera::CFrame.Position.z -= right.z * cameraSpeed;
    }
    if (keys['D']) {
        Camera::CFrame.Position.x += right.x * cameraSpeed;
        Camera::CFrame.Position.y += right.y * cameraSpeed;
        Camera::CFrame.Position.z += right.z * cameraSpeed;
    }
    if (keys['E']) {
        Camera::CFrame.Position.x += up.x * cameraSpeed;
        Camera::CFrame.Position.y += up.y * cameraSpeed;
        Camera::CFrame.Position.z += up.z * cameraSpeed;
    }
    if (keys['Q']) {
        Camera::CFrame.Position.x -= up.x * cameraSpeed;
        Camera::CFrame.Position.y -= up.y * cameraSpeed;
        Camera::CFrame.Position.z -= up.z * cameraSpeed;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE: {
            currentWidth = LOWORD(lParam);
            currentHeight = HIWORD(lParam);
            if (currentWidth == 0 || currentHeight == 0) break;
            Pixel* newBuffer = (Pixel*)realloc(frameBuffer, currentWidth * currentHeight * sizeof(Pixel));
            if (newBuffer != NULL) {
                frameBuffer = newBuffer;
            }
            float* newzBuffer = (float*)realloc(zBuffer, currentWidth * currentHeight * sizeof(float));
            if (newzBuffer != NULL) {
                zBuffer = newzBuffer;
            }
            return 0;
        }

        case WM_KEYDOWN:
            keys[wParam] = true;
            return 0;
        case WM_KEYUP:
            keys[wParam] = false;
            return 0;

        case WM_RBUTTONDOWN: {
            lastMouse.x = (float)LOWORD(lParam);
            lastMouse.y = (float)HIWORD(lParam);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (wParam & MK_RBUTTON) {
                mouseHandler(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            Vector3 look = {Camera::CFrame.LookVector.x, Camera::CFrame.LookVector.y, Camera::CFrame.LookVector.z};
            if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
                //Camera::CFrame.Position.x += look.x * 10.0;
                //Camera::CFrame.Position.y += look.y * 10.0;
                //Camera::CFrame.Position.z += look.z * 10.0;
                cameraDistance = fminf(80.0f, fmaxf(0.0f, floor(cameraDistance) / 1.5f));
            } else if (GET_WHEEL_DELTA_WPARAM(wParam) < 0) {
                //Camera::CFrame.Position.x -= look.x * 10.0;
                //Camera::CFrame.Position.y -= look.y * 10.0;
                //Camera::CFrame.Position.z -= look.z * 10.0;
                cameraDistance = fminf(80, fmaxf(0.0f, cameraDistance * 1.5f));
                if (cameraDistance == 0) {
                    cameraDistance = 1.5f;
                }
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    frameBuffer = (Pixel*)malloc(currentWidth * currentHeight * sizeof(Pixel));
    zBuffer = (float*)malloc(currentWidth * currentHeight * sizeof(float));

    const wchar_t CLASS_NAME[] = L"GCWindowClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Graphicality Engine",
        WS_OVERLAPPEDWINDOW,

        CW_USEDEFAULT, CW_USEDEFAULT, // POSITION
        currentWidth, currentHeight, // SIZE

        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    //addCube({0.0f, 0.5f, 0.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.0f, 0.0f}, true);
    //addCube({2.0f, 1.0f, 1.0f}, {4.0f, 1.0f, 2.0f}, {0.0f, 0.0f, 0.75f}, true);
    addCube({0.0f, 0.0f, 0.0f}, {100.0f, 1.0f, 100.0f}, {0.0f, 0.75f, 0.0f}, true);
    Humanoid = addCube({0.0f, 3.0f, 0.0f}, {2.0f, 5.0f, 1.0f}, {0.75f, 0.75f, 0.75f}, false);
    Camera::CFrame.yaw = 0.0f;
    Camera::CFrame.pitch = radians(-45.0f);

    rotateCamera(Camera::CFrame.yaw, Camera::CFrame.pitch);

    LARGE_INTEGER frequency;
    LARGE_INTEGER lastTime;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);

    const double TARGET_FPS = 60.0;
    const double TARGET_FRAME_TIME = 1.0 / TARGET_FPS;

    double deltaTime = 0.0;
    double fpsTimer = 0.0;
    unsigned int frameCount = 0;

    HDC hdc = GetDC(hwnd);

    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            LARGE_INTEGER frameStartTime;
            QueryPerformanceCounter(&frameStartTime);
            deltaTime = (double)(frameStartTime.QuadPart - lastTime.QuadPart) / (double)frequency.QuadPart;
            lastTime = frameStartTime;

            frameCount++;
            fpsTimer += deltaTime;
            if (fpsTimer >= 1) {
                char titleBuffer[64];
                snprintf(titleBuffer, sizeof(titleBuffer), "Graphicality Engine | FPS: %u | Parts: %u", frameCount, cubes.size());
                //addCube({2.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}, false); // Just a test for later
                SetWindowTextA(hwnd, titleBuffer);
                frameCount = 0;
                fpsTimer -= 1;
            }
            clearScreen();
            //processMovement(deltaTime); // Freecam, not for player orbit
            processPhysics(deltaTime);
            playerMovement(deltaTime, Humanoid); // Enable processMovement, and disable playerMovement for freecam
            renderScene();

            BITMAPINFO bmi = {0};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = currentWidth;
            bmi.bmiHeader.biHeight = -currentHeight;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            StretchDIBits(hdc, 0, 0, currentWidth, currentHeight, 0, 0, currentWidth, currentHeight,
                          frameBuffer, &bmi, DIB_RGB_COLORS, SRCCOPY);

            LARGE_INTEGER frameEndTime;
            double frameElapsed = 0.0;
            do {
                QueryPerformanceCounter(&frameEndTime);
                frameElapsed = (double)(frameEndTime.QuadPart - frameStartTime.QuadPart) / (double)frequency.QuadPart;
                if (frameElapsed < TARGET_FRAME_TIME) {
                    Sleep(0);
                }
            } while (frameElapsed < TARGET_FRAME_TIME);
        }
    }
    ReleaseDC(hwnd, hdc);
    delete[] frameBuffer;
    delete[] zBuffer;
    cubes.clear();
    return 0;
}
