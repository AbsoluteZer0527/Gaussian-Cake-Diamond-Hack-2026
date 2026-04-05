#include "ParticleSystem.h"
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include <iostream>

ParticleSystem::ParticleSystem() {
    particleRadius  = 0.08f;
    useOTPositions  = false;
    particleShader  = 0;
    vao = vboPos = vboColor = 0;
}

void ParticleSystem::InitGL() {
    particleShader = LoadShaders("shaders/particle.vert", "shaders/particle.frag");

    if (particleShader == 0) {
        std::cerr << "ParticleSystem::InitGL: failed to load particle shader (id=0)" << std::endl;
    } else {
        std::cerr << "ParticleSystem::InitGL: particle shader id = " << particleShader << std::endl;
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboPos);
    glGenBuffers(1, &vboColor);

    glBindVertexArray(vao);

    // Allow the vertex shader to set gl_PointSize
    glEnable(GL_PROGRAM_POINT_SIZE);


    // Position buffer — attribute location 0
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    // Color buffer — attribute location 1
    glBindBuffer(GL_ARRAY_BUFFER, vboColor);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
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
    std::cerr << "ParticleSystem::SpawnFromGaussian: spawned " << particles.size() << " particles\n";
}

void ParticleSystem::Update(float deltaTime) {
    if (!useOTPositions) return;
    for (size_t i = 0; i < particles.size() && i < transportPositions.size(); i++) {
        particles[i].position = transportPositions[i];
    }
}

void ParticleSystem::Draw(glm::mat4 viewProjection, int viewportHeight) {
    if (particles.empty()) {
        std::cerr << "ParticleSystem::Draw: no particles to draw (particles.empty())" << std::endl;
        return;
    }
    if (particleShader == 0) {
        std::cerr << "ParticleSystem::Draw: particleShader == 0, cannot draw" << std::endl;
        return;
    }

    // Quick debug: ensure GL objects and data look sane
    std::cerr << "ParticleSystem::Draw: vao=" << vao << " vboPos=" << vboPos << " vboColor=" << vboColor
              << " shader=" << particleShader << " count=" << particles.size() << std::endl;
    if (!particles.empty()) {
        const auto &p0 = particles[0].position;
        glm::vec4 clip = viewProjection * glm::vec4(p0, 1.0f);
        std::cerr << " first particle pos=" << p0.x << "," << p0.y << "," << p0.z
                  << " clip=" << clip.x << "," << clip.y << "," << clip.z << "," << clip.w << std::endl;
    }

    GLenum errBefore = glGetError();
    if (errBefore != GL_NO_ERROR) std::cerr << "GL error before upload: " << errBefore << std::endl;

    // Build flat CPU arrays for upload
    std::vector<glm::vec3> positions(particles.size());
    std::vector<glm::vec4> colors(particles.size());
    for (size_t i = 0; i < particles.size(); i++) {
        positions[i] = particles[i].position;
        colors[i]    = particles[i].color;
    }

    // Upload to GPU
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                 positions.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vboColor);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4),
                 colors.data(), GL_DYNAMIC_DRAW);

    // Disable fixed-function lighting so it doesn't interfere with the shader
    glDisable(GL_LIGHTING);

    // Render state: alpha blend, no depth writes (transparent splats stack correctly)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUseProgram(particleShader);
    // Ensure shader point size is enabled, and set a reasonable fallback glPointSize
    glEnable(GL_PROGRAM_POINT_SIZE);
    float fallbackSize = glm::max(1.0f, (float)viewportHeight * particleRadius * 0.5f);
    glPointSize(fallbackSize);

    glUniformMatrix4fv(glGetUniformLocation(particleShader, "uViewProj"),
                       1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform1f(glGetUniformLocation(particleShader, "uPointRadius"),    particleRadius);
    glUniform1f(glGetUniformLocation(particleShader, "uViewportHeight"), (float)viewportHeight);

    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, (GLsizei)particles.size());
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "GL error after draw: " << err << std::endl;
    glBindVertexArray(0);

    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUseProgram(0);
    glEnable(GL_LIGHTING);
}

void ParticleSystem::DrawGUI() {
    ImGui::Begin("Particle System");
    ImGui::Text("Active Particles: %d", (int)particles.size());
    ImGui::SliderFloat("Particle Radius", &particleRadius, 1.0f, 1.0f);
    ImGui::End();
}
