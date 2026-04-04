#include "Particle.h"

Particle::Particle(float maxLife,float radius){
    position = glm::vec3(0.0f);
    velocity = glm::vec3(0.0f);
    force = glm::vec3(0.0f);
    mass = 1.0f;
    life = maxLife;
    this->maxLife = maxLife;
    this->radius = radius;

}

//add all the forces, called by other classes
void Particle::ApplyForce(const glm::vec3& f){
    force += f;
}

//Core physics
void Particle::Integrate(float deltaTime){
    //Newton 2nd law
    glm::vec3 acceleration = force / mass;

    //Euler
    velocity += acceleration * deltaTime;
    position += velocity * deltaTime;

    force = glm::vec3(0.0f);
}

void Particle::CheckGroundCollision(float groundHeight, float elasticity, float friction){
    //push position on ground
    if (position.y - radius < groundHeight){
        position.y = groundHeight+radius;

         //velocity: break it to downward(normal) + other(tangent)
        glm::vec3 groundNormal = glm::vec3(0,1,0);
        float normalComponent = glm::dot(velocity,groundNormal); 
        glm::vec3 tangentComponent = velocity - normalComponent* groundNormal;
        normalComponent *=-elasticity;
        tangentComponent *= 1 - friction;
        velocity = tangentComponent + normalComponent*groundNormal;
    }
}

