#pragma once
#include "core.h"

class Particle {
public:
    glm::vec3 position;
    glm::vec3 velocity;

    Particle();
    void Update(float deltaTime, float damping);
};