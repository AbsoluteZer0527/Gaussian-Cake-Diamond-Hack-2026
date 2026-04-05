#pragma once

#include "Camera.h"
#include "Cube.h"
#include "Shader.h"
#include "core.h"
#include "ParticleSystem.h"
#include "Gaussian.h"
#include "OptimalTransport.h"
#include "TransportAnimator.h"

class Window {
public:
    // Window Properties
    static int width;
    static int height;
    static const char* windowTitle;

    // Objects to render
    static ParticleSystem* particleSystem;

    // Gaussian OT pipeline
    static Gaussian* gaussianA;
    static Gaussian* gaussianB;
    static OTMap* otMap;
    static TransportAnimator* animator;
    static bool otMapComputed;
    static int particleCount;

    // OBJ mesh input — empty string means "use preset"
    static char objPathA[256];
    static char objPathB[256];
    static int  sampleCount;   // points sampled from mesh surface

    // Mesh morph rendering (mesh-to-mesh case)
    static std::vector<glm::vec3> morphSrcVerts;  // source triangle vertices (3 per tri)
    static std::vector<glm::vec3> morphDstVerts;  // matched destination vertices
    static bool    useMeshMorph;
    static GLuint  morphVAO;
    static GLuint  morphVBOPos;
    static GLuint  morphVBONorm;

    // Shader Program
    static GLuint shaderProgram;

    // Act as Constructors and desctructors
    static bool initializeProgram();
    static bool initializeObjects();
    static void cleanUp();

    // for the Window
    static GLFWwindow* createWindow(int width, int height);
    static void resizeCallback(GLFWwindow* window, int width, int height);

    // update and draw functions
    static void idleCallback();
    static void displayCallback(GLFWwindow*);

    // helper to reset the camera
    static void resetCamera();

    // callbacks - for interaction
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_callback(GLFWwindow* window, double currX, double currY);

    // OT pipeline
    static void ComputeOTAndSpawnParticles();
    static void DrawMainGUI();
};
