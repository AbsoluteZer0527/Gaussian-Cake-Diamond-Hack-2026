#include "ParticleSystem.h"
#include <cstdlib>
#include <ctime>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include <cmath>

ParticleSystem::ParticleSystem(){
    srand(time(0));
    emitPosition = glm::vec3(0,8,0);
    emitPositionVariance = glm::vec3(1,1,1);
    
    emitterShape = NONE;
    boundaryRadius = 2.0f;
    
    initialVelocity = glm::vec3(0,2,0);
    initialVelocityVariance = glm::vec3(1,1,1);
    emitRate = 10.0f;
    baseLife = 3.0f;
    lifeVariance = 1.0f;
    spawnAccumulator = 0.0f; //leftover from last frame
    particleRadius = 0.1f;

    //physics, can modify in Imgui
    gravity = 9.8f;
    airDensity = 1.225f;
    dragCoefficient = 0.47f;
    elasticity = 0.5f;
    friction = 0.3f;
    groundHeight = 0.0f;

}

// When a particle is created, one must set its initial position, velocity, and other attributes
//It is often nice to be able to specify some type of geometry of the 
//Particle source, along with a particle creation rate
//This source geometry could also animate over time
void ParticleSystem::SpawnParticles(){
    float randomLife = baseLife + lifeVariance * ((float)rand() / RAND_MAX* 2.0f - 1.0f);
    Particle p(randomLife, particleRadius);
    //Position variance
    
    float s  = (float)rand() / RAND_MAX;
    //caps t below 1.0
    float t = (float)rand() / RAND_MAX * 0.9999f;

    float u = 2* glm::pi<float>() * s; //convert s into a random Y axis angle 0 to 2pi
    float v = sqrt(t * (1 - t)); //stretch factor from t that controls dx and dz
    
    float dx = 2 * v * cos(u);
    float dy = 1 - 2*t;
    float dz = 2 * v * sin(u);

    switch(emitterShape){
        case SPHERE:{
            float randomRadius = boundaryRadius * cbrt(((float)rand() / RAND_MAX));
            p.position = emitPosition + glm::normalize(glm::vec3(dx,dy,dz)) * randomRadius;
            break;
        }
        case CUBE:{
            p.position.x = emitPosition.x + ((float)rand() / RAND_MAX * 2.0f - 1.0f) * boundaryRadius;
            p.position.y = emitPosition.y + ((float)rand() / RAND_MAX * 2.0f - 1.0f) * boundaryRadius;
            p.position.z = emitPosition.z + ((float)rand() / RAND_MAX * 2.0f - 1.0f) * boundaryRadius;
            break;
        }
        case NONE:
        default: {
            p.position.x = emitPosition.x + dx * emitPositionVariance.x;
            p.position.y = emitPosition.y + dy * emitPositionVariance.y;
            p.position.z = emitPosition.z + dz * emitPositionVariance.z;
            break;
        }
    }

    

    p.velocity.x = initialVelocity.x + dx * initialVelocityVariance.x;
    p.velocity.y = initialVelocity.y + dy * initialVelocityVariance.y;
    p.velocity.z = initialVelocity.z + dz * initialVelocityVariance.z;

    particles.push_back(p);
}

void ParticleSystem::ApplyGravity(Particle& p){
    glm::vec3 f = p.mass * gravity *glm::vec3(0,-1,0);
    p.ApplyForce(f);
}

//f_drag = ½ ρ |v|² c_d · a · (-v̂)
void ParticleSystem::ApplyDrag(Particle& p){
    //|v|
    float speed = glm::length(p.velocity);
    if (speed < 0.0001f) return;

    glm::vec3 dragDirection = -(p.velocity / speed);
    //a sphere right now
    float area = glm::pi<float>() * p.radius* p.radius;
    glm::vec3 f_drag = 0.5f * airDensity * speed*speed * dragCoefficient * area * dragDirection;

    p.ApplyForce(f_drag);

}

void ParticleSystem::Update(float deltaTime){

    // Determine how many new particles to create this frame
    float num=deltaTime*emitRate+spawnAccumulator;
    int newParticles=int(num); //round up and check if spawnning 
    spawnAccumulator=num-float(newParticles);

    // Create particles
    for(int i = 0; i < newParticles; i++)
        SpawnParticles();

    //update all particles
    for(Particle& p:particles){
        ApplyGravity(p);
        ApplyDrag(p);
        p.Integrate(deltaTime);
        p.CheckGroundCollision(groundHeight, elasticity, friction);
        
    }
    //swap with last removal of dead particles
    int i = 0;
    while (i< particles.size()){
        particles[i].life-=deltaTime;
        if (!particles[i].HaveLife()){
            Particle temp = particles[i];
            particles[i] = particles[particles.size()-1];
            particles[particles.size()-1] = temp;
            particles.pop_back();
        }else{
            i++;
        }
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


void ParticleSystem::Draw(glm::mat4 viewProjection){
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    for(Particle& p : particles){
        glPushMatrix();
        glLoadMatrixf(glm::value_ptr(viewProjection));
        glTranslatef(p.position.x, p.position.y, p.position.z);

        // fade from bright to dark as particle ages
        float lifeRatio = p.life / p.maxLife;
        glColor3f(1.0f, (0.6f + 0.4f * lifeRatio), 66/255.0f * lifeRatio);

        DrawSphere(p.radius, 12, 12);
        glPopMatrix();
    }
    glDisable(GL_LIGHTING);
    //draw ground
    glPushMatrix();
    glLoadMatrixf(glm::value_ptr(viewProjection));
    glColor3f(84/255.0f, 120/255.0f, 255/255.0f);
    glBegin(GL_QUADS);
    glVertex3f (-20, groundHeight, -20);
    glVertex3f ( 20, groundHeight, -20);
    glVertex3f ( 20, groundHeight,  20);
    glVertex3f (-20, groundHeight,  20);
    glEnd();
    glPopMatrix();

    glEnable(GL_LIGHTING);
}

void ParticleSystem::DrawGUI(){
    ImGui::Begin("Particle System");

    // live particle count
    ImGui::Text("Active Particles: %d", (int)particles.size());
    ImGui::Separator();

    if(ImGui::CollapsingHeader("Emitter")){
        ImGui::DragFloat3("Emit Position",    glm::value_ptr(emitPosition), 0.1f);
        ImGui::DragFloat3("Position Variance",glm::value_ptr(emitPositionVariance), 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat3("Initial Velocity", glm::value_ptr(initialVelocity), 0.1f);
        ImGui::DragFloat3("Velocity Variance",glm::value_ptr(initialVelocityVariance), 0.1f, 0.0f, 10.0f);
        ImGui::SliderFloat("Emit Rate",       &emitRate, 0.0f, 200.0f);
    }

    if(ImGui::CollapsingHeader("Lifespan")){
        ImGui::SliderFloat("Base Life",     &baseLife,     0.1f, 10.0f);
        ImGui::SliderFloat("Life Variance", &lifeVariance, 0.0f, 5.0f);
    }

    if(ImGui::CollapsingHeader("Physics")){
        ImGui::SliderFloat("Gravity",           &gravity,          0.0f, 20.0f);
        ImGui::SliderFloat("Air Density",        &airDensity,       0.0f, 5.0f);
        ImGui::SliderFloat("Drag Coefficient",   &dragCoefficient,  0.0f, 2.0f);
        ImGui::SliderFloat("Particle Radius",    &particleRadius,   0.01f, 1.0f);
    }

    if(ImGui::CollapsingHeader("Collision")){
        ImGui::SliderFloat("Elasticity",    &elasticity,   0.0f, 1.0f);
        ImGui::SliderFloat("Friction",      &friction,     0.0f, 1.0f);
        ImGui::SliderFloat("Ground Height", &groundHeight, -10.0f, 10.0f);
    }

    const char* shapes[] = { "Sphere", "Cube", "None" };
    ImGui::Combo("Emitter Shape", (int*)&emitterShape, shapes,3);
    ImGui::SliderFloat("Boundary Size", &boundaryRadius, 0.1f, 5.0f);
    ImGui::End();
}