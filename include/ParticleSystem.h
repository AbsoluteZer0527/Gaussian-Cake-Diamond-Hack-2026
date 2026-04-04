#pragma once
#include "core.h"
#include <vector>
#include "Particle.h"

/* 
Spawn particles in a initial random position using Guassian
assign a random life span
for each particle, mannage gravity, aerodynamic drag, 
ground collision constrant
Integrated motion
*/

class ParticleSystem
{
public:
    ParticleSystem();
    
    std::vector<Particle> particles;

    //extra: keep particles in a shape
    enum EmitterShape { SPHERE, CUBE, NONE};
    EmitterShape emitterShape;
    float boundaryRadius;  

    glm::vec3 emitPosition;
    glm::vec3 emitPositionVariance;
    
    glm::vec3 initialVelocity;
    glm::vec3 initialVelocityVariance;

    float emitRate;
    float baseLife; //+lifeVariance = assign particle maxlife
    float lifeVariance;
    float spawnAccumulator; //leftover from last frame

    //physics, can modify in Imgui
    float gravity;
    float airDensity;
    float dragCoefficient;
    float particleRadius;
    float elasticity;
    float friction;
    float groundHeight;
    
    void Update(float deltaTime);
    void SpawnParticles();
    void ApplyGravity(Particle& p); //basic gravity
    void ApplyDrag(Particle& p); //compute drag and call apply force
    void DrawGUI();
    void DrawSphere(float radius, int slices, int stacks);
    void Draw(glm::mat4 viewProjection);//draw each particles

};
