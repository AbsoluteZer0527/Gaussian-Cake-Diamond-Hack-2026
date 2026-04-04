#pragma once
#include "core.h"

class Particle {
public:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 force;
    float mass;
    float life;
    float maxLife;
    float radius;

    Particle(float maxLife = 3.0f, float radius = 0.1f);

    void ApplyForce(const glm::vec3& f);
    void Integrate(float deltaTime);
    void CheckGroundCollision(float groundHeight, float elasticity, float friction);
    bool HaveLife() const;
};
