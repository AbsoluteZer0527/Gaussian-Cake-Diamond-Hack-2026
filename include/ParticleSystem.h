#pragma once
#include "core.h"
#include <vector>
#include "Particle.h"
#include "Gaussian.h"

class ParticleSystem
{
public:
    ParticleSystem();

    std::vector<Particle> particles;
    float particleRadius;

    //OT transport mode — positions driven by TransportAnimator
    bool useOTPositions;
    std::vector<glm::vec3> transportPositions;

    //Spawn N particles sampled from a Gaussian (clears existing particles)
    void SpawnFromGaussian(const Gaussian& g, int N, unsigned int seed = 42);

    void Update(float deltaTime);
    void DrawGUI();
    void DrawSphere(float radius, int slices, int stacks);
    void Draw(glm::mat4 viewProjection);
};
