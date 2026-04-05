#include "ParticleSystem.h"
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"

ParticleSystem::ParticleSystem() {
    particleRadius = 0.03f;
    useOTPositions = false;
}

void ParticleSystem::SpawnFromGaussian(const Gaussian& g, int N, unsigned int seed) {
    particles.clear();
    transportPositions.clear();

    auto pts = g.Sample(N, seed);
    for (const auto& pos : pts) {
        Particle p(particleRadius);
        p.position = pos;
        particles.push_back(p);
        transportPositions.push_back(pos);
    }
    useOTPositions = false;
}

void ParticleSystem::Update(float deltaTime) {
    if (!useOTPositions) return;
    for (size_t i = 0; i < particles.size() && i < transportPositions.size(); i++) {
        particles[i].position = transportPositions[i];
    }
}

// DrawSphere has been completely removed.

void ParticleSystem::Draw(glm::mat4 viewProjection) {
    // 1. Disable lighting to save calculation time
    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    // 2. Load the matrix ONCE, not per-particle
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(viewProjection)); 

    // Set color once
    glColor3f(0.0f, 0.0f, 0.0f); // warm golden-orange

    // 3. Map your world-space radius to pixel-space point size. 
    // You may need to adjust the multiplier (e.g., 100.0f) depending on your window size.
    glPointSize(particleRadius * 100.0f); 

    // 4. Draw all particles in a single batch
    glBegin(GL_POINTS);
    for (const Particle& p : particles) {
        glVertex3f(p.position.x, p.position.y, p.position.z);
    }
    glEnd();
}

void ParticleSystem::DrawGUI() {
    ImGui::Begin("Particle System");
    ImGui::Text("Active Particles: %d", (int)particles.size());
    // Adjusted the slider max slightly so the points don't get too massive in pixel space
    ImGui::SliderFloat("Particle Radius", &particleRadius, 0.01f, 0.5f); 
    ImGui::End();
}