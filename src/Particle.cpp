#include "Particle.h"

Particle::Particle(float radius) {
    position = glm::vec3(0.0f);
    color    = glm::vec4(1.0f, 0.5f, 0.71f, 0.8f); // default: bubblegum pink
    this->radius = radius;
}
