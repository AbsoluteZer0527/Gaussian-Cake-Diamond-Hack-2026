#include "Window.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "OBJLoader.h"
#include "MeshSampler.h"

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
int              Window::presetIndexA  = 0;
int              Window::presetIndexB  = 1;
char             Window::objPathA[256] = {};
char             Window::objPathB[256] = {};
int              Window::sampleCount   = 2000;

//Interaction Variables
bool LeftDown, RightDown;
int MouseX, MouseY;

//The shader program id
GLuint Window::shaderProgram;

//Gaussian presets (diagonal covariance, mean at origin)
//A cake built by gaussian
struct GaussianPreset {
    const char* name;
    Eigen::Matrix3d cov;
};

static GaussianPreset presets[] = {
    { "Cake Tier",       Eigen::Vector3d(2.0, 0.3, 2.0).asDiagonal() },  // flat disc
    { "Diamond",         Eigen::Vector3d(0.5, 2.5, 0.5).asDiagonal() },  // tall spike
    { "Frosting Sphere", Eigen::Vector3d(1.5, 1.5, 1.5).asDiagonal() },  // isotropic
    { "Sprinkle Disc",   Eigen::Vector3d(3.0, 0.1, 3.0).asDiagonal() },  // very flat
};
static const int presetCount = (int)(sizeof(presets) / sizeof(presets[0]));


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

    gaussianA     = new Gaussian();
    gaussianB     = new Gaussian();
    otMap         = nullptr;
    animator      = new TransportAnimator();
    otMapComputed = false;
    particleCount = 1000;
    presetIndexA  = 0;
    presetIndexB  = 1;

    Cam->SetDistance(8.0f);
    Cam->SetIncline(-20.0f);

    // Spawn initial shape so the scene isn't empty on startup
    gaussianA->mean       = Eigen::Vector3d::Zero();
    gaussianA->covariance = presets[0].cov;
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

        animator->InitializeWithEndPoints(ptsA, endPts);
        particleSystem->useOTPositions = true;
        animator->StartTransport();

    } else {
        // === Gaussian OT (presets, or single mesh → Gaussian) ===
        if (hasMeshA) {
            if (!LoadMeshGaussian(objPathA, sampleCount, gaussianA)) return;
        } else {
            gaussianA->mean       = Eigen::Vector3d::Zero();
            gaussianA->covariance = presets[presetIndexA].cov;
        }

        if (hasMeshB) {
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
}

// Draw a Gaussian source selector: preset combo + optional OBJ path input.
static void DrawGaussianSelector(const char* label, int& presetIdx, char* pathBuf,
                                  const char** presetNames, int count) {
    ImGui::Text("%s:", label);
    ImGui::PushID(label);

    bool usingMesh = (pathBuf[0] != '\0');

    // Preset combo (greyed out when a mesh path is entered)
    if (usingMesh) ImGui::BeginDisabled();
    ImGui::Combo("##preset", &presetIdx, presetNames, count);
    if (usingMesh) ImGui::EndDisabled();

    // OBJ path input + clear button
    ImGui::SetNextItemWidth(-60);
    ImGui::InputText("##obj", pathBuf, 256);
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        pathBuf[0] = '\0';

    if (usingMesh)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "  mesh loaded");
    else
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "  using preset");

    ImGui::PopID();
}

void Window::DrawMainGUI() {
    ImGui::Begin("Gaussian Cake");

    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "Gaussian Cake");
    ImGui::Separator();

    const char* presetNames[presetCount];
    for (int i = 0; i < presetCount; i++) presetNames[i] = presets[i].name;

    DrawGaussianSelector("Source (A)", presetIndexA, objPathA, presetNames, presetCount);
    ImGui::Spacing();
    DrawGaussianSelector("Target (B)", presetIndexB, objPathB, presetNames, presetCount);

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
