#pragma once
#include "core.h"

class Particle {
public:
    glm::vec3 position;
    glm::vec4 color;   // rgba — assigned at spawn, read by Draw()
    float radius;

    Particle(float radius = 0.03f);
};
