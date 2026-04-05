#include "Window.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "OBJLoader.h"
#include "MeshSampler.h"

#include <filesystem>
#include <algorithm>

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
int              Window::particleCount = 5000;
std::vector<std::string> Window::meshNames;
std::vector<std::string> Window::meshPaths;
int              Window::meshIndexA    = 0;
int              Window::meshIndexB    = 0;
int              Window::sampleCount   = 5000;

//Interaction Variables
bool LeftDown, RightDown;
int MouseX, MouseY;

//The shader program id
GLuint Window::shaderProgram;

// Default blob: isotropic 3D Gaussian (no color, no named presets)
static const Eigen::Matrix3d blobCovariance = Eigen::Vector3d(1.0, 1.0, 1.0).asDiagonal();

// Scan the meshes/ directory and populate Window::meshNames / meshPaths.
// Index 0 is always the "blob" sentinel (no path).
static void ScanMeshes() {
    Window::meshNames.clear();
    Window::meshPaths.clear();

    namespace fs = std::filesystem;
    std::string meshDir = "meshes";

    if (fs::is_directory(meshDir)) {
        std::vector<std::string> found;
        for (const auto& entry : fs::directory_iterator(meshDir)) {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".obj") {
                found.push_back(entry.path().string());
            }
        }
        std::sort(found.begin(), found.end());
        for (const auto& p : found) {
            Window::meshPaths.push_back(p);
            Window::meshNames.push_back(fs::path(p).filename().string());
        }
    }
}

// Constructors and desctructors
bool Window::initializeProgram() {
    shaderProgram = LoadShaders("shaders/shader.vert", "shaders/shader.frag");

    if (!shaderProgram) {
        std::cerr << "Failed to initialize shader program" << std::endl;
        return false;
    }

    return true;
}

bool Window::initializeObjects() {
    particleSystem = new ParticleSystem();

    gaussianA = new Gaussian();
    gaussianB = new Gaussian();
    otMap = nullptr;
    animator = new TransportAnimator();
    otMapComputed = false;
    particleCount = 3000;
    meshIndexA = 0;
    meshIndexB = 0;

    ScanMeshes();

    Cam->SetDistance(8.0f);
    Cam->SetIncline(-20.0f);

    // Spawn initial blob so the scene isn't empty on startup
    gaussianA->mean       = Eigen::Vector3d::Zero();
    gaussianA->covariance = blobCovariance;
    particleSystem->SpawnFromGaussian(*gaussianA, particleCount);

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
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    // glClearColor(83/255.0f, 203/255.0f, 243/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLfloat light0_pos[] = {10.0f, 10.0f, 10.0f, 1.0f};
    GLfloat light0_color[] = {1.0f, 0.8f, 0.8f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_color);

    GLfloat light1_pos[] = {-10.0f, 5.0f, -10.0f, 1.0f};
    GLfloat light1_color[] = {0.8f, 0.8f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_color);

    particleSystem->Draw(Cam->GetViewProjectMtx());

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

// Greedy approximate discrete OT: match each point in ptsA to the nearest
// unmatched point in ptsB. O(N^2) — fine for N <= 2000 on a single Bake click.
static std::vector<glm::vec3> GreedyDiscreteOT(
    const std::vector<glm::vec3>& ptsA,
    const std::vector<glm::vec3>& ptsB)
{
    int N = (int)std::min(ptsA.size(), ptsB.size());
    std::vector<bool>    used(ptsB.size(), false);
    std::vector<glm::vec3> result(N);

    for (int i = 0; i < N; i++) {
        float bestDist = 1e30f;
        int   bestJ    = 0;
        for (int j = 0; j < (int)ptsB.size(); j++) {
            if (used[j]) continue;
            glm::vec3 d = ptsA[i] - ptsB[j];
            float dist = glm::dot(d, d); // squared distance — avoids sqrt
            if (dist < bestDist) { bestDist = dist; bestJ = j; }
        }
        result[i]   = ptsB[bestJ];
        used[bestJ] = true;
    }
    return result;
}

void Window::ComputeOTAndSpawnParticles() {
    // meshIndex 0 = blob; 1..N = meshPaths[index-1]
    bool hasMeshA = (meshIndexA > 0 && meshIndexA <= (int)meshPaths.size());
    bool hasMeshB = (meshIndexB > 0 && meshIndexB <= (int)meshPaths.size());
    const std::string& pathA = hasMeshA ? meshPaths[meshIndexA - 1] : "";
    const std::string& pathB = hasMeshB ? meshPaths[meshIndexB - 1] : "";

    if (hasMeshA && hasMeshB) {
        // === Discrete mesh-to-mesh OT ===
        // Particles start exactly on mesh A surface, end exactly on mesh B surface.
        // The Gaussian OT map is not used — the greedy matching IS the transport.
        std::vector<Triangle> trisA, trisB;
        if (!OBJLoader::Load(pathA.c_str(), trisA)) return;
        if (!OBJLoader::Load(pathB.c_str(), trisB)) return;
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

        animator->InitializeWithEndPoints(ptsA, endPts);
        particleSystem->useOTPositions = true;
        animator->StartTransport();

    } else {
        // === Gaussian OT (mesh → Gaussian or blob → blob) ===
        if (hasMeshA) {
            if (!LoadMeshGaussian(pathA.c_str(), sampleCount, gaussianA)) return;
        } else {
            gaussianA->mean       = Eigen::Vector3d::Zero();
            gaussianA->covariance = blobCovariance;
        }

        if (hasMeshB) {
            if (!LoadMeshGaussian(pathB.c_str(), sampleCount, gaussianB)) return;
        } else {
            gaussianB->mean       = Eigen::Vector3d::Zero();
            gaussianB->covariance = blobCovariance;
        }

        delete otMap;
        otMap         = new OTMap(OptimalTransport::Compute(*gaussianA, *gaussianB));
        otMapComputed = true;

        particleSystem->SpawnFromGaussian(*gaussianA, particleCount);
        animator->Initialize(particleSystem->transportPositions, *otMap);
        particleSystem->useOTPositions = true;
        animator->StartTransport();
    }
}

// Draw a mesh selector combo: first entry is the blob default, rest are scanned .obj files.
static void DrawGaussianSelector(const char* label, int& meshIndex) {
    ImGui::Text("%s:", label);
    ImGui::PushID(label);

    // Build list: "(blob)" + mesh filenames
    int total = 1 + (int)Window::meshNames.size();
    auto getter = [](void* data, int idx, const char** out) -> bool {
        auto* names = static_cast<std::vector<std::string>*>(data);
        if (idx == 0) { *out = "Show Gaussian"; return true; }
        if (idx - 1 < (int)names->size()) { *out = (*names)[idx - 1].c_str(); return true; }
        return false;
    };
    ImGui::Combo("##mesh", &meshIndex, getter, &Window::meshNames, total);

    if (meshIndex > 0)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "  mesh selected");
    else
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "  Show Gaussian(default)");

    ImGui::PopID();
}

void Window::DrawMainGUI() {
    ImGui::Begin("Gaussian Cake");

    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "Gaussian Cake");
    ImGui::Separator();

    DrawGaussianSelector("Source (A)", meshIndexA);
    ImGui::Spacing();
    DrawGaussianSelector("Target (B)", meshIndexB);

    ImGui::Separator();
    ImGui::SliderInt("Particles",    &particleCount, 100, 10000);
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

    if (ImGui::Button("Revert"))
        animator->ReverseTransport();
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
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
