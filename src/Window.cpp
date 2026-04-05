#include "Window.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "OBJLoader.h"
#include "MeshSampler.h"
<<<<<<< Updated upstream
#include <iostream>
=======

#include <filesystem>
#include <algorithm>
#include <random>
#include <glm/gtc/type_ptr.hpp>
>>>>>>> Stashed changes

//Window Properties
int Window::width;
int Window::height;
const char* Window::windowTitle = "Gaussian Cake";

//Camera Properties
Camera* Cam;

//Object particles
ParticleSystem* Window::particleSystem = nullptr;

//Gaussian OT pipeline
Gaussian*        Window::gaussianA     = nullptr;
Gaussian*        Window::gaussianB     = nullptr;
OTMap*           Window::otMap         = nullptr;
TransportAnimator* Window::animator    = nullptr;
bool             Window::otMapComputed = false;
<<<<<<< Updated upstream
int              Window::particleCount = 1000;
char             Window::objPathA[256] = {};
char             Window::objPathB[256] = {};
=======
int              Window::particleCount = 2000;
std::vector<std::string> Window::meshNames;
std::vector<std::string> Window::meshPaths;
int              Window::meshIndexA    = 0;
int              Window::meshIndexB    = 0;
>>>>>>> Stashed changes
int              Window::sampleCount   = 2000;
std::vector<glm::vec3> Window::morphSrcVerts;
std::vector<glm::vec3> Window::morphDstVerts;
bool             Window::useMeshMorph  = false;
GLuint           Window::morphVAO      = 0;
GLuint           Window::morphVBOPos   = 0;
GLuint           Window::morphVBONorm  = 0;

//Interaction Variables
bool LeftDown, RightDown;
int MouseX, MouseY;

//The shader program id
GLuint Window::shaderProgram;

// Default Gaussian: isotropic blob, σ=1.22 in all directions
static Eigen::Matrix3d DefaultCovariance() {
    return Eigen::Matrix3d::Identity() * 1.5;
}

// Forward declarations for static helper functions used before their definitions
static glm::vec4 MahalanobisColor(float sigma);
static void AssignGaussianColors(class ParticleSystem* ps, const class Gaussian& g);
static void AssignHeightColors(class ParticleSystem* ps);

// Constructors and desctructors
bool Window::initializeProgram() {
    shaderProgram = LoadShaders("shaders/shader.vert", "shaders/shader.frag");

    if (!shaderProgram) {
        std::cerr << "Failed to initialize shader program" << std::endl;
        return false;
    }

    std::cerr << "Window::initializeProgram: shaderProgram = " << shaderProgram << std::endl;

    return true;
}

bool Window::initializeObjects() {
    particleSystem = new ParticleSystem();

    gaussianA     = new Gaussian();
    gaussianB     = new Gaussian();
    otMap         = nullptr;
    animator      = new TransportAnimator();
    otMapComputed = false;
<<<<<<< Updated upstream
    particleCount = 1000;
=======
    particleCount = 2000;
    meshIndexA    = 0;
    meshIndexB    = 0;
    useMeshMorph  = false;

    // Set up shared VAO/VBOs used for both mesh morph and blob point cloud
    glGenVertexArrays(1, &morphVAO);
    glGenBuffers(1, &morphVBOPos);
    glGenBuffers(1, &morphVBONorm);
    glBindVertexArray(morphVAO);
    glBindBuffer(GL_ARRAY_BUFFER, morphVBOPos);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, morphVBONorm);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    ScanMeshes();
>>>>>>> Stashed changes

    Cam->SetDistance(8.0f);
    Cam->SetIncline(-20.0f);

    particleSystem->InitGL();

    // Spawn default isotropic Gaussian blob on startup
    gaussianA->mean       = Eigen::Vector3d::Zero();
    gaussianA->covariance = DefaultCovariance();
    particleSystem->SpawnFromGaussian(*gaussianA, particleCount);
    // Uniform soft white — no coloring on the default blob
    for (auto& p : particleSystem->particles)
        p.color = glm::vec4(0.95f, 0.95f, 0.95f, 0.70f);

    // Ensure particle radius is large enough to be visible by default while debugging
    particleSystem->particleRadius = 0.5f;
    std::cerr << "Window::initializeObjects: set particleRadius=" << particleSystem->particleRadius << std::endl;

    return true;
}

void Window::cleanUp() {
    delete particleSystem;
    delete gaussianA;
    delete gaussianB;
    delete otMap;
    delete animator;
    glDeleteVertexArrays(1, &morphVAO);
    glDeleteBuffers(1, &morphVBOPos);
    glDeleteBuffers(1, &morphVBONorm);
    glDeleteProgram(shaderProgram);
}

//window settings
GLFWwindow* Window::createWindow(int width, int height) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return NULL;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(width, height, windowTitle, NULL, NULL);

    if (!window) {
        std::cerr << "Failed to open GLFW window." << std::endl;
        glfwTerminate();
        return NULL;
    }

    glfwMakeContextCurrent(window);
    glewInit();
    glfwSwapInterval(0);

    Cam = new Camera();
    Cam->SetAspect(float(width) / float(height));

    LeftDown = RightDown = false;
    MouseX = MouseY = 0;

    Window::resizeCallback(window, width, height);

    return window;
}

void Window::resizeCallback(GLFWwindow* window, int width, int height) {
    Window::width = width;
    Window::height = height;
    glViewport(0, 0, width, height);
    Cam->SetAspect(float(width) / float(height));
}

//update and draw functions
void Window::idleCallback() {
    Cam->Update();

    static double lastTime = 0.0;
    double currentTime = glfwGetTime();
    float deltaTime = (lastTime == 0.0) ? 0.016f : (float)(currentTime - lastTime);
    lastTime = currentTime;

    deltaTime = glm::min(deltaTime, 0.05f);
    particleSystem->Update(deltaTime);

    if (otMapComputed) {
        animator->Update(deltaTime, particleSystem->transportPositions);
        particleSystem->useOTPositions = (animator->state != AnimationState::GAUSSIAN_A);
    }
}

void Window::displayCallback(GLFWwindow* window) {
    glClearColor(83/255.0f, 203/255.0f, 243/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 vp       = Cam->GetViewProjectMtx();
    glm::mat4 identity = glm::mat4(1.0f);

    if (useMeshMorph && !particleSystem->transportPositions.empty()) {
        // === Mesh morph: render lerped triangles with computed face normals ===
        auto& verts = particleSystem->transportPositions;
        int numVerts = (int)verts.size();

<<<<<<< Updated upstream
    particleSystem->Draw(Cam->GetViewProjectMtx(), Window::height);
=======
        std::vector<glm::vec3> normals(numVerts, glm::vec3(0, 1, 0));
        for (int i = 0; i + 2 < numVerts; i += 3) {
            glm::vec3 e1 = verts[i+1] - verts[i];
            glm::vec3 e2 = verts[i+2] - verts[i];
            glm::vec3 n  = glm::cross(e1, e2);
            float len = glm::length(n);
            if (len > 1e-6f) n /= len;
            normals[i] = normals[i+1] = normals[i+2] = n;
        }

        glBindBuffer(GL_ARRAY_BUFFER, morphVBOPos);
        glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof(glm::vec3), verts.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, morphVBONorm);
        glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof(glm::vec3), normals.data(), GL_DYNAMIC_DRAW);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(vp));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),    1, GL_FALSE, glm::value_ptr(identity));
        glUniform3f(glGetUniformLocation(shaderProgram, "DiffuseColor"), 0.85f, 0.85f, 0.92f);

        glBindVertexArray(morphVAO);
        glDrawArrays(GL_TRIANGLES, 0, numVerts);
        glBindVertexArray(0);
        glUseProgram(0);

    } else if (!particleSystem->particles.empty()) {
        // === Gaussian blob: render particle cloud as lit points ===
        int numPts = (int)particleSystem->particles.size();
        std::vector<glm::vec3> positions(numPts);
        std::vector<glm::vec3> normals(numPts, glm::vec3(0.577f, 0.577f, 0.577f));
        for (int i = 0; i < numPts; i++)
            positions[i] = particleSystem->particles[i].position;

        glBindBuffer(GL_ARRAY_BUFFER, morphVBOPos);
        glBufferData(GL_ARRAY_BUFFER, numPts * sizeof(glm::vec3), positions.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, morphVBONorm);
        glBufferData(GL_ARRAY_BUFFER, numPts * sizeof(glm::vec3), normals.data(), GL_DYNAMIC_DRAW);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(vp));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),    1, GL_FALSE, glm::value_ptr(identity));
        glUniform3f(glGetUniformLocation(shaderProgram, "DiffuseColor"), 0.9f, 0.9f, 0.9f);

        glPointSize(4.0f);
        glBindVertexArray(morphVAO);
        glDrawArrays(GL_POINTS, 0, numPts);
        glBindVertexArray(0);
        glUseProgram(0);
    }
>>>>>>> Stashed changes

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    Window::DrawMainGUI();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwPollEvents();
    glfwSwapBuffers(window);
}

// Color helpers

// Map Mahalanobis distance (in sigma units) to a layered "Gaussian cake" color.
static glm::vec4 MahalanobisColor(float sigma) {
    if (sigma < 1.0f) return glm::vec4(1.00f, 0.50f, 0.71f, 0.85f); // pink   — 1σ core
    if (sigma < 2.0f) return glm::vec4(0.75f, 0.52f, 0.99f, 0.70f); // lavender — 2σ shell
    if (sigma < 3.0f) return glm::vec4(0.38f, 0.65f, 0.98f, 0.55f); // sky blue — 3σ shell
    return             glm::vec4(0.13f, 0.25f, 0.67f, 0.40f);        // deep blue — outer tail
}

// Color each particle by its Mahalanobis distance from the Gaussian mean.
// This shows the shell structure of the Gaussian density directly on the cloud.
static void AssignGaussianColors(ParticleSystem* ps, const Gaussian& g) {
    Eigen::Matrix3d covInv = g.covariance.inverse();
    for (auto& p : ps->particles) {
        Eigen::Vector3d d(p.position.x - g.mean(0),
                          p.position.y - g.mean(1),
                          p.position.z - g.mean(2));
        float mahal = (float)std::sqrt((d.transpose() * covInv * d)(0, 0));
        p.color = MahalanobisColor(mahal);
    }
}

// Color each particle by normalized Y height: blue at bottom, pink at top.
// Used for discrete mesh OT where no Gaussian is available.
static void AssignHeightColors(ParticleSystem* ps) {
    if (ps->particles.empty()) return;
    float minY = ps->particles[0].position.y;
    float maxY = minY;
    for (const auto& p : ps->particles) {
        minY = std::min(minY, p.position.y);
        maxY = std::max(maxY, p.position.y);
    }
    float range = (maxY - minY) < 1e-6f ? 1.0f : (maxY - minY);
    for (auto& p : ps->particles) {
        float t = (p.position.y - minY) / range;
        p.color = glm::vec4(
            glm::mix(0.38f, 1.00f, t),  // R: blue→pink
            glm::mix(0.65f, 0.50f, t),  // G
            glm::mix(0.98f, 0.71f, t),  // B
            0.75f
        );
    }
}

// OT pipeline

// Load an OBJ and fit a Gaussian to its surface. Returns false on failure.
static bool LoadMeshGaussian(const char* path, int N, Gaussian* g) {
    std::vector<Triangle> tris;
    if (!OBJLoader::Load(path, tris)) return false;
    OBJLoader::Normalize(tris);
    auto pts = MeshSampler::Sample(tris, N);
    g->FitFromPoints(pts);
    return true;
}

void Window::ComputeOTAndSpawnParticles() {
<<<<<<< Updated upstream
    // Gaussian A — OBJ if path given, else preset
    if (objPathA[0] != '\0') {
        if (!LoadMeshGaussian(objPathA, sampleCount, gaussianA)) return;
    } else {
        gaussianA->mean       = Eigen::Vector3d::Zero();
        gaussianA->covariance = presets[presetIndexA].cov;
=======
    bool hasMeshA = (objPathA[0] != '\0');
    bool hasMeshB = (objPathB[0] != '\0');

    if (hasMeshA && hasMeshB) {
        // === Mesh morph: render actual triangles lerped from A to B ===
        std::vector<Triangle> trisA, trisB;
<<<<<<< Updated upstream
        if (!OBJLoader::Load(objPathA, trisA)) return;
        if (!OBJLoader::Load(objPathB, trisB)) return;
=======
        if (!OBJLoader::Load(pathA, trisA)) return;
        if (!OBJLoader::Load(pathB, trisB)) return;
>>>>>>> Stashed changes
        OBJLoader::Normalize(trisA);
        OBJLoader::Normalize(trisB);

        // Subsample trisA to keep per-frame CPU work reasonable
        const int triLimit = 3000;
        if ((int)trisA.size() > triLimit) {
            std::mt19937 rng(42);
            std::shuffle(trisA.begin(), trisA.end(), rng);
            trisA.resize(triLimit);
        }

        // Collect trisB vertices for nearest-neighbour matching
        std::vector<glm::vec3> bVerts;
        bVerts.reserve(trisB.size() * 3);
        for (auto& tri : trisB) {
            bVerts.push_back(tri.v0);
            bVerts.push_back(tri.v1);
            bVerts.push_back(tri.v2);
        }
        // Subsample bVerts so matching stays fast
        const int bVertLimit = 9000;
        if ((int)bVerts.size() > bVertLimit) {
            float step = (float)bVerts.size() / bVertLimit;
            std::vector<glm::vec3> sampled;
            sampled.reserve(bVertLimit);
            for (int i = 0; i < bVertLimit; i++)
                sampled.push_back(bVerts[(int)(i * step)]);
            bVerts = std::move(sampled);
        }

        // For each triangle vertex in trisA, find the nearest vertex on trisB
        morphSrcVerts.clear();
        morphDstVerts.clear();
        morphSrcVerts.reserve(trisA.size() * 3);
        morphDstVerts.reserve(trisA.size() * 3);
        for (auto& tri : trisA) {
            glm::vec3 v[3] = { tri.v0, tri.v1, tri.v2 };
            for (int k = 0; k < 3; k++) {
                morphSrcVerts.push_back(v[k]);
                float best = 1e30f;
                glm::vec3 bestVert = bVerts[0];
                for (auto& bv : bVerts) {
                    glm::vec3 d = v[k] - bv;
                    float dist = glm::dot(d, d);
                    if (dist < best) { best = dist; bestVert = bv; }
                }
                morphDstVerts.push_back(bestVert);
            }
        }

        // Feed morph verts into the animator so it drives transportProgress
        particleSystem->particles.clear();
        particleSystem->transportPositions = morphSrcVerts;
        for (auto& pos : morphSrcVerts) {
            Particle p; p.position = pos;
            particleSystem->particles.push_back(p);
        }

<<<<<<< Updated upstream
        AssignHeightColors(particleSystem);

        animator->InitializeWithEndPoints(ptsA, endPts);
=======
        useMeshMorph  = true;
        otMapComputed = true;
        animator->InitializeWithEndPoints(morphSrcVerts, morphDstVerts);
>>>>>>> Stashed changes
        particleSystem->useOTPositions = true;
        animator->StartTransport();

    } else {
<<<<<<< Updated upstream
        // === Gaussian OT (mesh → blob, blob → mesh, or blob → blob) ===
=======
        // === Gaussian OT (mesh → Gaussian or blob → blob) ===
        useMeshMorph = false;
>>>>>>> Stashed changes
        if (hasMeshA) {
            if (!LoadMeshGaussian(objPathA, sampleCount, gaussianA)) return;
        } else {
            gaussianA->mean       = Eigen::Vector3d::Zero();
            gaussianA->covariance = DefaultCovariance();
        }

        if (hasMeshB) {
            if (!LoadMeshGaussian(objPathB, sampleCount, gaussianB)) return;
        } else {
            gaussianB->mean       = Eigen::Vector3d::Zero();
            gaussianB->covariance = DefaultCovariance();
        }

        delete otMap;
        otMap         = new OTMap(OptimalTransport::Compute(*gaussianA, *gaussianB));
        otMapComputed = true;

        particleSystem->SpawnFromGaussian(*gaussianA, particleCount);
        AssignGaussianColors(particleSystem, *gaussianA);
        animator->Initialize(particleSystem->transportPositions, *otMap);
        particleSystem->useOTPositions = true;
        animator->StartTransport();
>>>>>>> Stashed changes
    }

    // Gaussian B — OBJ if path given, else preset
    if (objPathB[0] != '\0') {
        if (!LoadMeshGaussian(objPathB, sampleCount, gaussianB)) return;
    } else {
        gaussianB->mean       = Eigen::Vector3d::Zero();
        gaussianB->covariance = presets[presetIndexB].cov;
    }

    delete otMap;
    otMap         = new OTMap(OptimalTransport::Compute(*gaussianA, *gaussianB));
    otMapComputed = true;

    particleSystem->SpawnFromGaussian(*gaussianA, particleCount);

    animator->Initialize(particleSystem->transportPositions, *otMap);
    particleSystem->useOTPositions = true;
    animator->StartTransport();
}

// Draw a Gaussian source selector: OBJ path input or default blob.
static void DrawGaussianSelector(const char* label, char* pathBuf) {
    ImGui::Text("%s:", label);
    ImGui::PushID(label);

    bool usingMesh = (pathBuf[0] != '\0');

    ImGui::SetNextItemWidth(-60);
    ImGui::InputText("##obj", pathBuf, 256);
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        pathBuf[0] = '\0';

    if (usingMesh)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "  mesh");
    else
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "  gaussian blob (default)");

    ImGui::PopID();
}

void Window::DrawMainGUI() {
    ImGui::Begin("Gaussian Cake");

    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "Gaussian Cake");
    ImGui::Separator();

    DrawGaussianSelector("Source (A)", objPathA);
    ImGui::Spacing();
    DrawGaussianSelector("Target (B)", objPathB);

    ImGui::Separator();
    ImGui::SliderInt("Particles",    &particleCount, 100, 5000);
    ImGui::SliderInt("Mesh Samples", &sampleCount,   500, 10000);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 0.4f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.6f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.8f, 0.2f, 0.5f, 1.0f));
    if (ImGui::Button("Bake It!", ImVec2(-1, 0)))
        ComputeOTAndSpawnParticles();
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    animator->DrawGUI();

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Return to A"))
        animator->ReverseTransport();
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        animator->Reset();
        useMeshMorph = false;
        particleSystem->useOTPositions = false;
        particleSystem->particles.clear();
        otMapComputed = false;
    }

    ImGui::Separator();
    ImGui::SliderFloat("Particle Radius", &particleSystem->particleRadius, 0.01f, 1.0f);

    ImGui::End();
}

// helper to reset the camera
void Window::resetCamera() {
    Cam->Reset();
    Cam->SetAspect(float(Window::width) / float(Window::height));
}

//key callbacks - for Interaction
void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT ) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GL_TRUE);
                break;
            case GLFW_KEY_R:
                resetCamera();
                break;
            default:
                break;
        }
    }
}

void Window::mouse_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if(!ImGui::GetIO().WantCaptureMouse){
            LeftDown = (action == GLFW_PRESS);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if(!ImGui::GetIO().WantCaptureMouse){
            RightDown = (action == GLFW_PRESS);
        }
    }
}

void Window::cursor_callback(GLFWwindow* window, double currX, double currY) {
    if(ImGui::GetIO().WantCaptureMouse){
        MouseX = (int)currX;
        MouseY = (int)currY;
        return;
    }

    int maxDelta = 100;
    int dx = glm::clamp((int)currX - MouseX, -maxDelta, maxDelta);
    int dy = glm::clamp(-((int)currY - MouseY), -maxDelta, maxDelta);

    MouseX = (int)currX;
    MouseY = (int)currY;

    if (LeftDown) {
        const float rate = 1.0f;
        Cam->SetAzimuth(Cam->GetAzimuth() + dx * rate);
        Cam->SetIncline(glm::clamp(Cam->GetIncline() - dy * rate, -90.0f, 90.0f));
    }
    if (RightDown) {
        const float rate = 0.005f;
        float dist = glm::clamp(Cam->GetDistance() * (1.0f - dx * rate), 0.01f, 1000.0f);
        Cam->SetDistance(dist);
    }
}
