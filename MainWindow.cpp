// Copyright © 2026 Graphicality, LightDark Software SP. All rights reserved.
// Graphicality Engine is propriatary software, All contents of Graphicality must not be reproduced.
// Source may be used for educational purposes.

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <array>
#include "Camera.h"

double radians(double degrees) {
    return degrees * (3.1415926535 / 180.0);
}

int currentWidth = 800;
int currentHeight = 600;

bool keys[256] = { false };

struct Pixel { unsigned char r, g, b, a; };
struct Vector3 { float x, y, z; };
struct Vector2 { float x, y; };
struct Colour3 { float r, g, b; };

using Vertex3D = std::array<float, 3>;
using Face3D = std::vector<int>;

struct MeshData {
    std::vector<Vertex3D> vertices;
    std::vector<Face3D> faces;
};

enum class PartShape {
    Cube,
    Sphere,
    Plane
};

struct StructPart {
    PartShape shape;
    Vector3 position;
    Vector3 size;
    Colour3 colour;
    Vector3 linearVelocity;
    bool anchored;
};

Vector2 lastMouse;

Pixel* frameBuffer = NULL;
float* zBuffer = NULL;

std::vector<StructPart> worldParts;

float cameraDistance = 5.0f;

int Humanoid;
bool grounded = false;

StructPart CreatePart(PartShape shape, Vector3 position, Vector3 size, Colour3 colour, bool anchored) {
    return StructPart{shape, position, size, colour, {0.0f, 0.0f, 0.0f}, anchored};
}

MeshData CubeMesh = {
    {
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        {-0.5f, -0.5f,  0.5f}
    },
    {
        {4, 6, 5}, {5, 6, 7},
        {1, 3, 0}, {0, 3, 2},
        {5, 7, 1}, {1, 7, 3},
        {0, 2, 4}, {4, 2, 6},
        {5, 1, 4}, {4, 1, 0},
        {2, 3, 6}, {6, 3, 7}
    }
};

MeshData PlaneMesh = {
    {
        { 0.5f,  0.0f, -0.5f},
        {-0.5f,  0.0f, -0.5f},
        { 0.5f,  0.0f,  0.5f},
        {-0.5f,  0.0f,  0.5f}
    },
    {
        {3, 1, 2}, {2, 1, 0}
    }
};

MeshData SphereMesh;

void createQuadSphere(float rad, int subdivisions) {
    struct Direction {
        Vector3 normal;
        Vector3 right;
        Vector3 up;
    };

    std::vector<Direction> directions = {
        {{1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{-1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, -1, 0}, {-1, 0, 0}, {0, 0, 1}},
        {{0, 0, 1}, {-1, 0, 0}, {0, 1, 0}},
        {{0, 0, -1}, {1, 0, 0}, {0, 1, 0}},
    };

    for (const auto& dir : directions) {
        int offset = static_cast<int>(SphereMesh.vertices.size());

        for (int i = 0; i <= subdivisions; ++i) {
            for (int j = 0; j <= subdivisions; ++j) {
                float u = (static_cast<float>(i) / subdivisions) * 2.0f - 1.0f;
                float v = (static_cast<float>(j) / subdivisions) * 2.0f - 1.0f;

                float px = dir.normal.x + u * dir.right.x + v * dir.up.x;
                float py = dir.normal.y + u * dir.right.y + v * dir.up.y;
                float pz = dir.normal.z + u * dir.right.z + v * dir.up.z;

                float mag = std::sqrt(px * px + py * py + pz * pz);

                SphereMesh.vertices.push_back({(px / mag) * rad, (py / mag) * rad, (pz / mag) * rad});

                if (i < subdivisions && j < subdivisions) {
                    int p1 = offset + i*(subdivisions+1)+j;
                    int p2 = offset + i*(subdivisions+1)+j+1;
                    int p3 = offset + (i+1)*(subdivisions+1)+j+1;
                    int p4 = offset + (i+1)*(subdivisions+1)+j;
                    SphereMesh.faces.push_back({p1, p3, p2});
                    SphereMesh.faces.push_back({p1, p4, p3});
                }
            }
        }
    }
}

void clearScreen() {
    if (zBuffer != nullptr) {
        std::fill_n(zBuffer, currentWidth * currentHeight, 10000.0f);
    }
    Pixel skyColour;
    skyColour.b = 70;
    skyColour.g = 100;
    skyColour.r = 200;
    skyColour.a = 255;
    std::fill_n(frameBuffer, currentWidth * currentHeight, skyColour);
}

float crossProduct(float ax, float ay, float bx, float by, float cx, float cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

Vector3 crossProduct3D(Vector3 a, Vector3 b) {
    float x = (a.y * b.z) - (a.z * b.y);
    float y = (a.z * b.x) - (a.x * b.z);
    float z = (a.x * b.y) - (a.y * b.x);
    return Vector3{x, y, z};
}

Vector3 Normalize(Vector3 vector) {
    float magnitude = sqrtf(vector.x*vector.x + vector.y*vector.y + vector.z*vector.z);
    if (magnitude == 0.0f) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }
    Vector3 normal = {
        vector.x / magnitude,
        vector.y / magnitude,
        vector.z / magnitude
    };
    return normal;
}

Vector3 rotateVector3(Vector3 vertex, Vector3 rotation) {
    float x = vertex.x;
    float y = vertex.y;
    float z = vertex.z;

    float radX = rotation.x;
    float y1 = y * cosf(radX) - z * sinf(radX);
    float z1 = z * cosf(radX) + y * sinf(radX);

    float radY = rotation.y;
    float x2 = x * cosf(radY) - z1 * sinf(radY);
    float z2 = z1 * cosf(radY) + x * sinf(radY);

    float radZ = rotation.z;
    float x3 = x2 * cosf(radZ) - y1 * sinf(radZ);
    float y3 = y1 * cosf(radZ) + x2 * sinf(radZ);

    return Vector3{x3, y3, z2};
}

Vector3 LookAtCamera(Vector3 position) {
        float tx = position.x - Camera::CFrame.Position.x;
        float ty = position.y - Camera::CFrame.Position.y;
        float tz = position.z - Camera::CFrame.Position.z;
        Vector3 rawLV = {tx, ty, tz};

        Vector3 lV = Normalize(rawLV);
        Vector3 uV = {0, 1, 0};
        Vector3 rV = Normalize(crossProduct3D(uV, lV));
        Vector3 nuV = Normalize(crossProduct3D(lV, rV));

        float rx = std::asin(-lV.y); // Pitch
        float ry = -std::atan2(lV.x, lV.z); // Yaw
        float rz = std::atan2(rV.y, uV.y); // Roll

        return Vector3{rx, ry, rz};
}

void rasterizeTriangle(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, Colour3 colour1, Colour3 colour2, Colour3 colour3) {
    float area = crossProduct(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;

    int minX = (int)floorf(fmaxf(0.0f, fminf(x0, fminf(x1, x2))));
    int maxX = (int)ceilf(fminf((float)(currentWidth - 1), fmaxf(x0, fmaxf(x1, x2))));
    int minY = (int)floorf(fmaxf(0.0f, fminf(y0, fminf(y1, y2))));
    int maxY = (int)ceilf(fminf((float)(currentHeight - 1), fmaxf(y0, fmaxf(y1, y2))));

    if (minX > maxX || minY > maxY) return;
    float invertedArea = 1.0f / area;

    colour1.r /= z0; colour1.g /= z0; colour1.b /= z0;
    colour2.r /= z1; colour2.g /= z1; colour2.b /= z1;
    colour3.r /= z2; colour3.g /= z2; colour3.b /= z2;

    float w0_dx = -(y2 - y1) * invertedArea;
    float w0_dy = (x2 - x1) * invertedArea;

    float w1_dx = -(y0 - y2) * invertedArea;
    float w1_dy = (x0 - x2) * invertedArea;

    float w2_dx = -(y1 - y0) * invertedArea;
    float w2_dy = (x1 - x0) * invertedArea;

    float w0Y = crossProduct(x1, y1, x2, y2, (float)minX, (float)minY) * invertedArea;
    float w1Y = crossProduct(x2, y2, x0, y0, (float)minX, (float)minY) * invertedArea;
    float w2Y = crossProduct(x0, y0, x1, y1, (float)minX, (float)minY) * invertedArea;

    for (int y = minY; y <= maxY; y++) {
        float w0 = w0Y;
        float w1 = w1Y;
        float w2 = w2Y;

        for (int x = minX; x <= maxX; x++) {
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
            w0 += w0_dx;
            w1 += w1_dx;
            w2 += w2_dx;
        }
        w0Y += w0_dy;
        w1Y += w1_dy;
        w2Y += w2_dy;
    }
}

void RenderPart(PartShape shape, Vector3 position, Vector3 size, Colour3 baseColour) {
    float scaleFactor = currentHeight / (2.0f * tanf(radians(Camera::FieldOfView) * 0.5f));
    float nearPlaneZ = 0.5f;
    Vector3 lightPosition = {10.0f, 20.0f, -10.0f};
    float ambientIntensity = 0.6f;

    const MeshData& partMesh = (shape == PartShape::Cube) ? CubeMesh :
                               (shape == PartShape::Sphere) ? SphereMesh : PlaneMesh;

    size_t vertexCount = partMesh.vertices.size();
    std::vector<Vector3> globalVertices(vertexCount);
    std::vector<Colour3> vertexColours(vertexCount);
    size_t faceCount = partMesh.faces.size();

    Vector3 rotation= {0.0f, 0.0f, 0.0f};

    if (shape == PartShape::Plane) {
        rotation = LookAtCamera(position);
        rotation.x += radians(-90.0f); // Offset LookAtCamera by 90 degrees (Radians)
    }
    
    for (size_t i = 0; i < vertexCount; i++) {
        Vector3 localVertex = {
            partMesh.vertices[i][0] * size.x,
            partMesh.vertices[i][1] * size.y,
            partMesh.vertices[i][2] * size.z
        };

        Vector3 rotatedVertex = rotateVector3(localVertex, rotation);

        // Get position relative to world
        Vector3 worldPosition = {
            position.x + rotatedVertex.x,
            position.y + rotatedVertex.y,
            position.z + rotatedVertex.z
        };

        // Normal
        Vector3 localNormal = {
            partMesh.vertices[i][0],
            partMesh.vertices[i][1],
            partMesh.vertices[i][2]
        };

        Vector3 worldNormal = Normalize(rotateVector3(localNormal, rotation));

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

    for (size_t i = 0; i < faceCount; i++) {
        int index0 = partMesh.faces[i][0];
        int index1 = partMesh.faces[i][1];
        int index2 = partMesh.faces[i][2];

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
            screen[j].x = (clipped[j].x * scaleFactor) / clipped[j].z + (currentWidth * 0.5f);
            screen[j].y = -(clipped[j].y * scaleFactor) / clipped[j].z + (currentHeight * 0.5f);
            screen[j].z = clipped[j].z;
        }

        if (clippedCount == 3) {
            float area = crossProduct(
                screen[0].x, screen[0].y,
                screen[1].x, screen[1].y,
                screen[2].x, screen[2].y
            );

            if (area > 0.0f) { // Backface culling
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

int addPart(PartShape shape, Vector3 position, Vector3 size, Colour3 colour, bool anchored) {
    worldParts.push_back(CreatePart(shape, position, size, colour, anchored));
    return (int)worldParts.size() - 1;
}

void renderScene() {
    for (int i = 0; i < worldParts.size(); i++) {
        const StructPart& part = worldParts[i];
        RenderPart(part.shape, part.position, part.size, part.colour);
    }
}

bool intersectAABB(int a, int b) {
    return
        (worldParts[a].position.x - (worldParts[a].size.x * 0.5f) <= worldParts[b].position.x + (worldParts[b].size.x * 0.5f)) &&
        (worldParts[a].position.x + (worldParts[a].size.x * 0.5f) >= worldParts[b].position.x - (worldParts[b].size.x * 0.5f)) &&

        (worldParts[a].position.y - (worldParts[a].size.y * 0.5f) <= worldParts[b].position.y + (worldParts[b].size.y * 0.5f)) &&
        (worldParts[a].position.y + (worldParts[a].size.y * 0.5f) >= worldParts[b].position.y - (worldParts[b].size.y * 0.5f)) &&

        (worldParts[a].position.z - (worldParts[a].size.z * 0.5f) <= worldParts[b].position.z + (worldParts[b].size.z * 0.5f)) &&
        (worldParts[a].position.z + (worldParts[a].size.z * 0.5f) >= worldParts[b].position.z - (worldParts[b].size.z * 0.5f));
}

void resolveAABB(int a, int b) {
    float delta[3] = {
        worldParts[a].position.x - worldParts[b].position.x,
        worldParts[a].position.y - worldParts[b].position.y,
        worldParts[a].position.z - worldParts[b].position.z
    };

    float overlap[3] = {
        (worldParts[a].size.x + worldParts[b].size.x) * 0.5f - fabsf(delta[0]),
        (worldParts[a].size.y + worldParts[b].size.y) * 0.5f - fabsf(delta[1]),
        (worldParts[a].size.z + worldParts[b].size.z) * 0.5f - fabsf(delta[2])
    };

    int axis = 0;
    if (overlap[1] < overlap[axis]) axis = 1;
    if (overlap[2] < overlap[axis]) axis = 2;

    float pushAmount = (delta[axis] > 0.0f) ? overlap[axis] : -overlap[axis];
    float push[3] = {0.0f, 0.0f, 0.0f};
    push[axis] = pushAmount;

    float weightA = (worldParts[a].anchored) ? 0.0f : ((worldParts[b].anchored) ? 1.0f : 0.5f);
    float weightB = (worldParts[b].anchored) ? 0.0f : ((worldParts[a].anchored) ? 1.0f : 0.5f);

    worldParts[a].position.x += push[0] * weightA;
    worldParts[a].position.y += push[1] * weightA;
    worldParts[a].position.z += push[2] * weightA;

    worldParts[b].position.x -= push[0] * weightB;
    worldParts[b].position.y -= push[1] * weightB;
    worldParts[b].position.z -= push[2] * weightB;

    if (weightA > 0.0f) (&worldParts[a].linearVelocity.x)[axis] = 0.0f;
    if (weightB > 0.0f) (&worldParts[b].linearVelocity.x)[axis] = 0.0f;
}

void checkCollisions() {
    grounded = false;
    size_t partCount = worldParts.size();
    for (size_t i = 0; i < partCount; i++) {
        for (size_t j = i + 1; j < partCount; j++) {
            if (worldParts[i].anchored && worldParts[j].anchored) {
                continue;
            }
            if (worldParts[i].shape == PartShape::Plane || worldParts[j].shape == PartShape::Plane) {
                continue;
            }
            if (intersectAABB(i, j)) {
                float AY = worldParts[i].position.y;
                float BY = worldParts[j].position.y;
                resolveAABB(i, j);
                if (worldParts[i].position.y > AY && i == Humanoid) {
                    grounded = true;
                }
                if (worldParts[j].position.y > BY && j == Humanoid) {
                    grounded = true;
                }
            }
        }
    }
}

void processPhysics(float deltaTime) {
    for (int i = 0; i < worldParts.size(); i++) {
        StructPart* part = &worldParts[i];
        if (part->anchored) {
            continue;
        }

        // part holds the value of the current part, "i" holds the part in part count
        //if (i != Humanoid) {
        //    return;
        //}

        part->linearVelocity.x *= expf(-10.0f * deltaTime);
        part->linearVelocity.z *= expf(-10.0f * deltaTime);

        float gravity = -50.0f;
        part->linearVelocity.y += gravity * deltaTime;

        part->position.x += part->linearVelocity.x * deltaTime;
        part->position.y += part->linearVelocity.y * deltaTime;
        part->position.z += part->linearVelocity.z * deltaTime;
    }
    checkCollisions();
    std::cout << worldParts[Humanoid].linearVelocity.y << std::endl;
}

void rotateCamera(float yaw, float pitch, int player) {
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

    if (Camera::cameratype == Camera::ORBIT_CAMERA || Camera::cameratype == Camera::FP_CAMERA) {
        Camera::CFrame.Position.x = worldParts[player].position.x - (Camera::CFrame.LookVector.x * cameraDistance);
        Camera::CFrame.Position.y = worldParts[player].position.y - (Camera::CFrame.LookVector.y * cameraDistance);
        Camera::CFrame.Position.z = worldParts[player].position.z - (Camera::CFrame.LookVector.z * cameraDistance);
    }
}

void mouseHandler(float mouseX, float mouseY) {
    float dx = (mouseX - lastMouse.x) * 0.005f;
    float dy = (mouseY - lastMouse.y) * 0.005f;
    Camera::CFrame.yaw += dx;
    Camera::CFrame.pitch = fmax(radians(-80), fmin(radians(80), Camera::CFrame.pitch - dy));
    lastMouse.x = mouseX;
    lastMouse.y = mouseY;
    rotateCamera(Camera::CFrame.yaw, Camera::CFrame.pitch, Humanoid);
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

    float X = worldParts[player].linearVelocity.x;
    float Y = worldParts[player].linearVelocity.y;
    float Z = worldParts[player].linearVelocity.z;

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
    if (keys[' ']) {
        if (grounded) {
            Y = 20.0f;
            grounded = false;
        }
    }
        
    worldParts[player].linearVelocity = {X, Y, Z};
    rotateCamera(Camera::CFrame.yaw, Camera::CFrame.pitch, player);
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


// SAVE AND OPEN FILE
void saveToFile(const std::string& filename) {
    std::ofstream outFile(filename);

    if (!outFile) {
        std::cerr << "Failed to save file: " << filename << std::endl;
        return;
    }

    outFile << worldParts.size() << "\n";
    for (const auto& part : worldParts) {
        outFile << static_cast<int>(part.shape) << " "
                << part.position.x << " " << part.position.y << " " << part.position.z << " "
                << part.size.x << " " << part.size.y << " " << part.size.z << " "
                << part.colour.r << " " << part.colour.g << " " << part.colour.b << " "
                << part.anchored << "\n";
    }

    outFile.close();
    std::cout << "Sucessfully saved file to: " << filename << std::endl;
}

void openFile(const std::string& filename) {
    std::ifstream inFile(filename);

    if (!inFile) {
        std::cerr << "Failed to read file: " << filename << std::endl;
        return;
    }

    worldParts.clear();
    size_t partCount = 0;
    if (!(inFile >> partCount)) return;

    for (size_t i = 0; i < partCount; ++i) {
        PartShape shape;
        Vector3 position;
        Vector3 size;
        Colour3 colour;
        bool anchored;
        int shapeInt = 0;

        if (inFile >> shapeInt
                   >> position.x >> position.y >> position.z
                   >> size.x >> size.y >> size.z
                   >> colour.r >> colour.g >> colour.b
                   >> anchored) {

            shape = static_cast<PartShape>(shapeInt);
            worldParts.push_back(CreatePart(shape, position, size, colour, anchored));
        }
    }

    inFile.close();
    std::cout << "Sucessfully opened file: " << filename << std::endl;
}

// Create web protocol
void registerProtocol() {
    wchar_t clientPath[MAX_PATH];
    GetModuleFileNameW(NULL, clientPath, MAX_PATH);
    std::wstring clientString(clientPath);

    std::wstring cliArg = L"\"" + clientString + L"\" \"%1\"";
    HKEY hKey;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\graphicality", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)L"URL:graphicality Protocol", 20 * sizeof(wchar_t));
        RegSetValueExW(hKey, L"URL Protocol", 0, REG_SZ, (BYTE*)L"", 0);
        RegCloseKey(hKey);

        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\graphicality\\shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)cliArg.c_str(), (cliArg.length() + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
        }
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

            if (wParam == 'C') {
                if (Camera::cameratype != Camera::FREE_CAMERA) {
                    Camera::cameratype = Camera::FREE_CAMERA;
                } else {
                    Camera::cameratype = (cameraDistance == 0) ? Camera::FP_CAMERA : Camera::ORBIT_CAMERA;
                }
            }
            if (wParam == '1') {
                saveToFile("test.grfx");
            }
            if (wParam == '2') {
                openFile("test.grfx");
            }

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
            if ((wParam & MK_RBUTTON) && Camera::cameratype != Camera::FP_CAMERA) {
                mouseHandler(LOWORD(lParam), HIWORD(lParam));
            } else if (Camera::cameratype == Camera::FP_CAMERA) {
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
            if (Camera::cameratype != Camera::FREE_CAMERA) {
                Camera::cameratype = (cameraDistance == 0) ? Camera::FP_CAMERA : Camera::ORBIT_CAMERA;
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
    registerProtocol();
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

    createQuadSphere(0.5, 6);

    //addPart({0.0f, 0.5f, 0.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.0f, 0.0f}, true);
    //addPart({2.0f, 1.0f, 1.0f}, {4.0f, 1.0f, 2.0f}, {0.0f, 0.0f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 0.0f, 0.0f}, {100.0f, 1.0f, 100.0f}, {0.0f, 0.75f, 0.0f}, true);

    addPart(PartShape::Cube, {0.0f, 1.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 2.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 3.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 4.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 5.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 6.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 7.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 8.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 9.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 10.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 11.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 12.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 13.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 14.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);
    addPart(PartShape::Cube, {0.0f, 15.0f, -40.0f}, {4.0f, 1.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);

    addPart(PartShape::Sphere, {0.0f, 16.5f, -40.0f}, {2.0f, 2.0f, 2.0f}, {0.75f, 0.75f, 0.75f}, true);

    addPart(PartShape::Plane, {10.0f, 4.0f, -40.0f}, {5.0f, 0.0f, 3.5f}, {0.75f, 0.75f, 0.75f}, true);

    Humanoid = addPart(PartShape::Cube, {0.0f, 3.0f, 0.0f}, {2.0f, 5.0f, 1.0f}, {0.75f, 0.75f, 0.75f}, false);
    Camera::CFrame.yaw = 0.0f;
    Camera::CFrame.pitch = radians(-45.0f);

    Camera::cameratype = Camera::ORBIT_CAMERA;

    rotateCamera(Camera::CFrame.yaw, Camera::CFrame.pitch, Humanoid);

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
                snprintf(titleBuffer, sizeof(titleBuffer), "Graphicality Engine | FPS: %u | Parts: %u", frameCount, worldParts.size());
                //addPart({0.0f, 30.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}, false); // Just a test for later
                SetWindowTextA(hwnd, titleBuffer);
                frameCount = 0;
                fpsTimer -= 1;
            }
            clearScreen();
            processPhysics(deltaTime);

            if (Camera::cameratype == Camera::FREE_CAMERA) {
                processMovement(deltaTime);
            } else {
                playerMovement(deltaTime, Humanoid);
            }
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
    free(frameBuffer);
    free(zBuffer);
    worldParts.clear();
    return 0;
}
