#include "Window.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "OBJLoader.h"
#include "MeshSampler.h"
#include <iostream>

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
int              Window::particleCount = 1000;
char             Window::objPathA[256] = {};
char             Window::objPathB[256] = {};
int              Window::sampleCount   = 2000;

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
    particleCount = 1000;

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

    GLfloat light0_pos[] = {10.0f, 10.0f, 10.0f, 1.0f};
    GLfloat light0_color[] = {1.0f, 0.8f, 0.8f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_color);

    GLfloat light1_pos[] = {-10.0f, 5.0f, -10.0f, 1.0f};
    GLfloat light1_color[] = {0.8f, 0.8f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_color);

    particleSystem->Draw(Cam->GetViewProjectMtx(), Window::height);

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
        // === Discrete mesh-to-mesh OT ===
        // Particles start exactly on mesh A surface, end exactly on mesh B surface.
        // The Gaussian OT map is not used — the greedy matching IS the transport.
        std::vector<Triangle> trisA, trisB;
        if (!OBJLoader::Load(objPathA, trisA)) return;
        if (!OBJLoader::Load(objPathB, trisB)) return;
        OBJLoader::Normalize(trisA);
        OBJLoader::Normalize(trisB);

        auto ptsA = MeshSampler::Sample(trisA, particleCount);
        auto ptsB = MeshSampler::Sample(trisB, particleCount);

        auto endPts = GreedyDiscreteOT(ptsA, ptsB);

        // Place particles directly at mesh A surface positions
        particleSystem->particles.clear();
        particleSystem->transportPositions = ptsA;
        for (const auto& pos : ptsA) {
            Particle p(particleSystem->particleRadius);
            p.position = pos;
            particleSystem->particles.push_back(p);
        }

        AssignHeightColors(particleSystem);

        animator->InitializeWithEndPoints(ptsA, endPts);
        particleSystem->useOTPositions = true;
        animator->StartTransport();

    } else {
        // === Gaussian OT (mesh → blob, blob → mesh, or blob → blob) ===
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
    ImGui::SliderInt("Particles",    &particleCount, 100, 2000);
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
        particleSystem->useOTPositions = false;
        particleSystem->particles.clear();
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
