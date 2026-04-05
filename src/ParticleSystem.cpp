#include "ParticleSystem.h"
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"

ParticleSystem::ParticleSystem() {
    particleRadius = 0.1f;
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

void ParticleSystem::DrawSphere(float radius, int slices, int stacks) {
    for (int i = 0; i < stacks; i++) {
        float phi0 = glm::pi<float>() * ((float)i / stacks);
        float phi1 = glm::pi<float>() * ((float)(i + 1) / stacks);

        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * glm::pi<float>() * ((float)j / slices);

            float x0 = radius * sin(phi0) * cos(theta);
            float y0 = radius * cos(phi0);
            float z0 = radius * sin(phi0) * sin(theta);

            float x1 = radius * sin(phi1) * cos(theta);
            float y1 = radius * cos(phi1);
            float z1 = radius * sin(phi1) * sin(theta);

            glNormal3f(x0/radius, y0/radius, z0/radius);
            glVertex3f(x0, y0, z0);
            glNormal3f(x1/radius, y1/radius, z1/radius);
            glVertex3f(x1, y1, z1);
        }
        glEnd();
    }
}

void ParticleSystem::Draw(glm::mat4 viewProjection) {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glColor3f(1.0f, 0.75f, 0.3f); // warm golden-orange
    for (Particle& p : particles) {
        glPushMatrix();
        glLoadMatrixf(glm::value_ptr(viewProjection));
        glTranslatef(p.position.x, p.position.y, p.position.z);
        DrawSphere(p.radius, 12, 12);
        glPopMatrix();
    }
    glDisable(GL_LIGHTING);

    // ground plane
    glPushMatrix();
    glLoadMatrixf(glm::value_ptr(viewProjection));
    glColor3f(84/255.0f, 120/255.0f, 255/255.0f);
    glBegin(GL_QUADS);
    glVertex3f(-20, 0, -20);
    glVertex3f( 20, 0, -20);
    glVertex3f( 20, 0,  20);
    glVertex3f(-20, 0,  20);
    glEnd();
    glPopMatrix();

    glEnable(GL_LIGHTING);
}

void ParticleSystem::DrawGUI() {
    ImGui::Begin("Particle System");
    ImGui::Text("Active Particles: %d", (int)particles.size());
    ImGui::SliderFloat("Particle Radius", &particleRadius, 0.01f, 1.0f);
    ImGui::End();
}
