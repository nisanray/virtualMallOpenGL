#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>

// ---------------------------------------------------------
// Global Camera, Input & Physics Variables
// ---------------------------------------------------------
glm::vec3 cameraPos = glm::vec3(0.0f, 1.5f, 45.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 640.0f;
float lastY = 360.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
float programTime = 0.0f;

// Physics & Dynamics
float velocityY = 0.0f;
float gravity = 18.0f;
float jumpForce = 6.5f;
bool isGrounded = true;

// Dynamic Mall States
float entranceOpenness = 0.0f;
float elevatorY = 1.5f;
int elevatorState = 0;
float elevatorTimer = 0.0f;
float elevatorDoors = 0.0f;

// Interactivity & Time of Day
bool isNight = false;
bool nKeyPressed = false;
bool flashlightOn = false;
bool fKeyPressed = false;
bool droneMode = false;
bool vKeyPressed = false;

// ---------------------------------------------------------
// Sphere Generation Variables
// ---------------------------------------------------------
unsigned int sphereVAO = 0, sphereVBO, sphereEBO;
int sphereIndexCount = 0;

// ---------------------------------------------------------
// Voxel Font System (For Neon Signs) A-Z
// ---------------------------------------------------------
const int voxelFont[26] = {
    31725, 27566, 31015, 27502, 31143, 31140, 31023, 23533, 29847, 4655,
    23861, 18727, 24429, 31597, 31607, 31716, 31609, 31733, 31183, 29842,
    23407, 23402, 23423, 23213, 23186, 29351
};

// ---------------------------------------------------------
// Automated Cinematic Tour System
// ---------------------------------------------------------
bool tourMode = false;
bool tKeyPressed = false;
float tourProgress = 0.0f;
int currentWaypoint = 0;
struct Waypoint { glm::vec3 pos; glm::vec3 lookAt; };

std::vector<Waypoint> tourPoints = {
    {{0.0f, 1.5f, 45.0f}, {0.0f, 6.0f, 28.0f}},
    {{0.0f, 1.5f, 26.0f}, {0.0f, 1.5f, 0.0f}},
    {{0.0f, 1.5f, 10.0f}, {0.0f, 1.5f, 0.0f}},
    {{-5.0f, 1.5f, 0.0f}, {0.0f, 5.0f, 0.0f}},
    {{0.0f, 1.5f, -2.0f}, {0.0f, 6.5f, -10.0f}},
    {{0.0f, 6.5f, -12.0f}, {0.0f, 6.5f, -20.0f}},
    {{-4.5f, 6.5f, -15.0f}, {0.0f, 1.5f, 0.0f}},
    {{0.0f,  6.5f, 20.0f}, {0.0f, 10.0f, 10.0f}},
    {{0.0f,  1.5f, 20.0f}, {0.0f, 1.5f, 45.0f}}
};

// ---------------------------------------------------------
// Shaders
// ---------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;
out vec3 LocalPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform int matType;

void main() {
    LocalPos = aPos;
    vec3 pos = aPos;
    
    if (matType == 7) {
        pos.x += sin(time * 2.0 + pos.y * 5.0) * 0.05 * pos.y;
    }

    FragPos = vec3(model * vec4(pos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 LocalPos;

uniform vec3 viewPos;
uniform vec4 objectColor; 
uniform int matType; 
uniform float time;

uniform vec3 dirLightDir;
uniform vec3 dirLightColor;
#define NR_POINT_LIGHTS 16
uniform vec3 pointLightPositions[NR_POINT_LIGHTS];
uniform vec3 pointLightColors[NR_POINT_LIGHTS];

uniform vec3 spotLightPos; uniform vec3 spotLightDir; uniform vec3 spotLightColor;
uniform float spotCutOff; uniform float spotOuterCutOff; uniform bool flashlightOn;
uniform bool isNight;

vec4 getMaterialProps() {
    vec3 albedo = objectColor.rgb;
    float specStr = 0.5; float shiny = 32.0; float alpha = objectColor.a;

    if (matType == 1) { 
        float scale = 2.0; vec2 grid = fract(FragPos.xz * scale);
        float border = 0.05;
        if(grid.x < border || grid.y < border) {
            albedo *= 0.4; specStr = 0.2;
        } else {
            float check = mod(floor(FragPos.x * scale) + floor(FragPos.z * scale), 2.0);
            albedo *= (check > 0.5 ? 1.0 : 0.85);
            specStr = 1.5; shiny = 256.0; 
        }
    } 
    else if (matType == 2) { 
        specStr = 2.0; shiny = 512.0; alpha = 0.35;
    }
    else if (matType == 3) { 
        float grain = fract(LocalPos.x * 20.0 + sin(LocalPos.z * 10.0) * 0.1);
        albedo *= (0.8 + 0.2 * grain); specStr = 0.1; shiny = 16.0;
    }
    else if (matType == 5) { 
        float noise = fract(sin(dot(FragPos.xz, vec2(12.9898, 78.233))) * 43758.5453);
        albedo *= (0.8 + 0.2 * noise); specStr = 0.0; shiny = 2.0;
    }
    else if (matType == 6) { 
        float wave = sin(FragPos.x * 5.0 + time * 3.0) * cos(FragPos.z * 5.0 + time * 2.0);
        albedo = mix(vec3(0.1, 0.5, 0.8), vec3(0.4, 0.8, 1.0), (wave + 1.0) * 0.5);
        specStr = 2.0; shiny = 256.0; alpha = 0.8;
    }
    else if (matType == 7) { 
        albedo *= (0.8 + 0.2 * sin(LocalPos.x * 50.0));
        specStr = 0.1; shiny = 8.0;
    }
    else if (matType == 8) { 
        float grain = fract(LocalPos.y * 50.0);
        albedo *= (0.9 + 0.1 * grain);
        specStr = 1.8; shiny = 128.0;
    }
    return vec4(albedo, specStr);
}

vec3 CalcDirLight(vec3 normal, vec3 viewDir, vec3 albedo, float specStr, float shiny) {
    vec3 lightDir = normalize(-dirLightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shiny);
    return (0.2 * albedo + diff * albedo + specStr * spec) * dirLightColor;
}

vec3 CalcPointLight(int i, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo, float specStr, float shiny) {
    vec3 lightDir = normalize(pointLightPositions[i] - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shiny);
    float distance = length(pointLightPositions[i] - fragPos);
    float attenuation = 1.0 / (1.0 + 0.045 * distance + 0.0075 * (distance * distance));    
    return (0.05 * albedo + diff * albedo + specStr * spec) * pointLightColors[i] * attenuation;
}

void main() {
    if (matType == 4) { 
        FragColor = vec4(objectColor.rgb * 2.0, objectColor.a);
        return;
    }

    vec4 props = getMaterialProps();
    vec3 albedo = props.rgb; float specStr = props.a; float shiny = (matType==1||matType==2||matType==6) ? 256.0 : 32.0;
    float alpha = (matType==2||matType==6) ? objectColor.a : 1.0;
    
    vec3 norm = normalize(Normal);
    if(matType == 6) { 
        norm.x += sin(FragPos.z * 10.0 + time * 5.0) * 0.1;
        norm.z += cos(FragPos.x * 10.0 + time * 5.0) * 0.1;
        norm = normalize(norm);
    }

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 result = CalcDirLight(norm, viewDir, albedo, specStr, shiny);
    for(int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(i, norm, FragPos, viewDir, albedo, specStr, shiny);    

    if(flashlightOn) {
        vec3 lightDir = normalize(spotLightPos - FragPos);
        float theta = dot(lightDir, normalize(-spotLightDir)); 
        if(theta > spotOuterCutOff) {
            float diff = max(dot(norm, lightDir), 0.0);
            float intensity = clamp((theta - spotOuterCutOff) / (spotCutOff - spotOuterCutOff), 0.0, 1.0);
            float dist = length(spotLightPos - FragPos);
            float atten = 1.0 / (1.0 + 0.045 * dist + 0.0075 * (dist * dist));
            result += (diff * albedo + specStr * pow(max(dot(viewDir, reflect(-lightDir, norm)), 0.0), shiny)) * spotLightColor * intensity * atten;
        }
    }
    
    if (matType == 1 || matType == 2 || matType == 6) {
        vec3 I = normalize(FragPos - viewPos);
        vec3 R = reflect(I, norm);
        vec3 envColor = mix(vec3(0.1, 0.1, 0.1), vec3(0.8, 0.9, 1.0), clamp(R.y, 0.0, 1.0));
        result += envColor * specStr * 0.3;
    }

    float distance = length(viewPos - FragPos);
    float fogFactor = clamp((distance - 35.0) / 45.0, 0.0, 1.0);
    
    // Dynamic Sky Color based on time of day
    vec3 skyColor = isNight ? vec3(0.01, 0.01, 0.03) : vec3(0.55, 0.75, 0.95); 
    
    FragColor = vec4(mix(result, skyColor, fogFactor), alpha);
}
)";

// ---------------------------------------------------------
// Initialization: Geometric Spheres
// ---------------------------------------------------------
void initSphere() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int xSegments = 30;
    int ySegments = 30;
    const float PI = 3.14159265359f;

    for (int y = 0; y <= ySegments; ++y) {
        for (int x = 0; x <= xSegments; ++x) {
            float xSegment = (float)x / (float)xSegments;
            float ySegment = (float)y / (float)ySegments;
            float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
            float yPos = std::cos(ySegment * PI);
            float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

            vertices.push_back(xPos); vertices.push_back(yPos); vertices.push_back(zPos);
            vertices.push_back(xPos); vertices.push_back(yPos); vertices.push_back(zPos);
        }
    }

    for (int y = 0; y < ySegments; ++y) {
        for (int x = 0; x < xSegments; ++x) {
            indices.push_back((y + 1) * (xSegments + 1) + x);
            indices.push_back(y * (xSegments + 1) + x);
            indices.push_back(y * (xSegments + 1) + x + 1);

            indices.push_back((y + 1) * (xSegments + 1) + x);
            indices.push_back(y * (xSegments + 1) + x + 1);
            indices.push_back((y + 1) * (xSegments + 1) + x + 1);
        }
    }

    sphereIndexCount = static_cast<int>(indices.size());

    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);

    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// ---------------------------------------------------------
// Physics & Collision Subsystem
// ---------------------------------------------------------
float getFloorHeight(float x, float currentY, float z) {
    if (x >= -2.0f && x <= 2.0f && z >= -10.0f && z <= 0.0f) {
        float clampedZ = glm::clamp(z, -10.0f, 0.0f);
        return 1.5f + (((0.0f - clampedZ) / 10.0f) * 5.0f);
    }
    if (currentY > 4.5f && z <= 28.0f) return 6.5f;
    if (z > 29.0f) return 1.3f;
    return 1.5f;
}

float getCeilingHeight(float x, float y, float z) {
    if (z > 28.0f) return 100.0f;
    if (y < 4.5f) {
        if (x > -4.0f && x < 4.0f && z > -20.0f && z < 25.0f) return 100.0f;
        return 4.8f;
    }
    return 9.8f;
}

bool isPositionValid(float x, float y, float z) {
    if (z > 27.8f && z < 28.2f && y < 4.5f) {
        float clearWidth = 0.2f + (entranceOpenness * 2.8f);
        if (std::abs(x) > clearWidth) return false;
    }

    if (z < -24.4f && z > -28.0f && std::abs(x) < 2.5f) {
        if (z > -24.8f && z < -24.4f) {
            float currentFloorDoors = 0.0f;
            if (y < 4.5f && elevatorState == 0) currentFloorDoors = elevatorDoors;
            else if (y >= 4.5f && elevatorState == 2) currentFloorDoors = elevatorDoors;

            float clearWidth = 0.2f + (currentFloorDoors * 1.5f);
            if (std::abs(x) > clearWidth) return false;
        }
        if (x < -1.9f || x > 1.9f || z < -27.4f) return false;
    }

    if (z > 28.0f) {
        if (z > 50.0f || z < -28.0f) return false;
        if (x < -20.0f || x > 20.0f) return false;
        if (z > 32.8f && z < 34.2f) {
            if (x > -6.2f && x < -4.8f) return false;
            if (x > 4.8f && x < 6.2f) return false;
        }
        return true;
    }

    bool onEscalator = (x >= -2.0f && x <= 2.0f && z >= -10.0f && z <= 0.0f);
    if (getFloorHeight(x, y, z) > 4.5f && !onEscalator) {
        bool valid2F = false;
        if (x <= -3.0f || x >= 3.0f) valid2F = true;
        if (z <= -10.0f || z >= 15.0f) valid2F = true;

        if (!valid2F) return false;

        if (x > -3.3f && x < -2.9f && z > -10.0f && z < 15.0f) return false;
        if (x > 2.9f && x < 3.3f && z > -10.0f && z < 15.0f) return false;
        if (z > 14.7f && z < 15.3f && x > -3.0f && x < 3.0f) return false;
        if (z > -10.3f && z < -9.7f) {
            if (x > -3.0f && x < -2.0f) return false;
            if (x > 2.0f && x < 3.0f) return false;
        }
    }

    for (int sz = -20; sz <= 20; sz += 10) {
        if (std::abs(x) > 6.8f && std::abs(x) < 7.2f && z > sz - 5.0f && z < sz + 5.0f) {
            if (!(z > sz - 1.8f && z < sz + 1.8f)) return false;
        }
        if (z > sz + 4.8f && z < sz + 5.2f && std::abs(x) > 7.0f) return false;
        if (std::abs(x) > 8.0f && std::abs(x) < 11.8f && z > sz - 1.2f && z < sz + 1.2f) return false;
    }

    if (x < -6.0f && z < -24.0f) {
        if (x > -7.0f && z > -25.0f) return true;
        if (x < -7.0f) return false;
    }

    if (z < -28.0f) return false;
    if (z <= 28.0f && std::abs(x) > 14.0f) return false;
    if (y < 3.0f && std::abs(x) < 2.5f && z > 12.5f && z < 17.5f) return false;

    return true;
}

void updateDynamics() {
    float dEntrance = glm::length(glm::vec3(cameraPos.x, 0.0f, cameraPos.z) - glm::vec3(0.0f, 0.0f, 28.0f));
    if (dEntrance < 12.0f) entranceOpenness = std::min(entranceOpenness + deltaTime * 2.0f, 1.0f);
    else entranceOpenness = std::max(entranceOpenness - deltaTime * 2.0f, 0.0f);

    elevatorTimer += deltaTime;
    float elvSpeed = 2.5f;
    if (elevatorState == 0) {
        elevatorDoors = std::min(elevatorDoors + deltaTime * 2.0f, 1.0f);
        if (elevatorTimer > 6.0f) { elevatorState = 1; elevatorTimer = 0.0f; }
    }
    else if (elevatorState == 1) {
        elevatorDoors = std::max(elevatorDoors - deltaTime * 3.0f, 0.0f);
        if (elevatorDoors == 0.0f) {
            elevatorY += deltaTime * elvSpeed;
            if (elevatorY >= 6.5f) { elevatorY = 6.5f; elevatorState = 2; elevatorTimer = 0.0f; }
        }
    }
    else if (elevatorState == 2) {
        elevatorDoors = std::min(elevatorDoors + deltaTime * 2.0f, 1.0f);
        if (elevatorTimer > 6.0f) { elevatorState = 3; elevatorTimer = 0.0f; }
    }
    else if (elevatorState == 3) {
        elevatorDoors = std::max(elevatorDoors - deltaTime * 3.0f, 0.0f);
        if (elevatorDoors == 0.0f) {
            elevatorY -= deltaTime * elvSpeed;
            if (elevatorY <= 1.5f) { elevatorY = 1.5f; elevatorState = 0; elevatorTimer = 0.0f; }
        }
    }
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        if (!tKeyPressed) { tourMode = !tourMode; droneMode = false; tKeyPressed = true; if (!tourMode) firstMouse = true; }
    }
    else tKeyPressed = false;

    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!fKeyPressed) { flashlightOn = !flashlightOn; fKeyPressed = true; }
    }
    else fKeyPressed = false;

    // Day / Night Toggle
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
        if (!nKeyPressed) { isNight = !isNight; nKeyPressed = true; }
    }
    else nKeyPressed = false;

    // Drone View Toggle
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
        if (!vKeyPressed) { droneMode = !droneMode; tourMode = false; vKeyPressed = true; }
    }
    else vKeyPressed = false;

    if (tourMode) {
        float tourSpeed = 0.15f; tourProgress += deltaTime * tourSpeed;
        if (tourProgress >= 1.0f) { tourProgress -= 1.0f; currentWaypoint = (currentWaypoint + 1) % tourPoints.size(); }
        int nextIndex = (currentWaypoint + 1) % tourPoints.size();
        float t = tourProgress * tourProgress * (3.0f - 2.0f * tourProgress);
        cameraPos = glm::mix(tourPoints[currentWaypoint].pos, tourPoints[nextIndex].pos, t);
        cameraFront = glm::normalize(glm::mix(tourPoints[currentWaypoint].lookAt, tourPoints[nextIndex].lookAt, t) - cameraPos);
        pitch = glm::degrees(asin(cameraFront.y)); yaw = glm::degrees(atan2(cameraFront.z, cameraFront.x));
        return;
    }
    else if (droneMode) {
        // Free Flight (No gravity, no collision limits)
        float cameraSpeed = 12.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += cameraFront * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= cameraFront * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos += cameraUp * cameraSpeed; // Fly Up
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cameraPos -= cameraUp * cameraSpeed; // Fly Down

        velocityY = 0.0f; // Disable physics gravity
    }
    else {
        // Standard Grounded Movement
        float cameraSpeed = 6.0f * deltaTime;
        float moveX = 0.0f, moveZ = 0.0f;
        glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
        glm::vec3 flatRight = glm::normalize(glm::cross(flatFront, cameraUp));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { moveX += flatFront.x; moveZ += flatFront.z; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { moveX -= flatFront.x; moveZ -= flatFront.z; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { moveX -= flatRight.x; moveZ -= flatRight.z; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { moveX += flatRight.x; moveZ += flatRight.z; }

        if (moveX != 0.0f || moveZ != 0.0f) {
            glm::vec3 moveDir = glm::normalize(glm::vec3(moveX, 0.0f, moveZ));
            float dx = moveDir.x * cameraSpeed; float dz = moveDir.z * cameraSpeed;
            if (isPositionValid(cameraPos.x + dx, cameraPos.y, cameraPos.z)) cameraPos.x += dx;
            if (isPositionValid(cameraPos.x, cameraPos.y, cameraPos.z + dz)) cameraPos.z += dz;
        }

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded) { velocityY = jumpForce; isGrounded = false; }

        velocityY -= gravity * deltaTime;
        cameraPos.y += velocityY * deltaTime;

        float ceilH = getCeilingHeight(cameraPos.x, cameraPos.y, cameraPos.z);
        if (cameraPos.y + 0.4f > ceilH) { cameraPos.y = ceilH - 0.4f; if (velocityY > 0) velocityY = 0.0f; }

        float currentFloorHeight = getFloorHeight(cameraPos.x, cameraPos.y, cameraPos.z);
        bool onElevator = (cameraPos.x >= -1.9f && cameraPos.x <= 1.9f && cameraPos.z >= -27.4f && cameraPos.z <= -24.6f);

        if (onElevator) {
            cameraPos.y = elevatorY;
            velocityY = 0.0f;
            isGrounded = true;
        }
        else if (cameraPos.y <= currentFloorHeight) {
            cameraPos.y = currentFloorHeight;
            velocityY = 0.0f;
            isGrounded = true;
        }

        if (isGrounded && cameraPos.x >= -2.0f && cameraPos.x <= 2.0f && cameraPos.z >= -10.0f && cameraPos.z <= 0.0f) {
            if (cameraPos.x > 0.0f) {
                float autoMove = 2.5f * deltaTime;
                if (isPositionValid(cameraPos.x, cameraPos.y, cameraPos.z - autoMove)) cameraPos.z -= autoMove;
            }
            else {
                float autoMove = 2.5f * deltaTime;
                if (isPositionValid(cameraPos.x, cameraPos.y, cameraPos.z + autoMove)) cameraPos.z += autoMove;
            }
        }
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (tourMode) return;
    float xpos = static_cast<float>(xposIn); float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    yaw += (xpos - lastX) * 0.1f; pitch += (lastY - ypos) * 0.1f;
    lastX = xpos; lastY = ypos;
    if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = normalize(front);
}

// ---------------------------------------------------------
// Draw Calls & World Building
// ---------------------------------------------------------
void drawBox(unsigned int shader, unsigned int VAO, glm::vec3 pos, glm::vec3 scale, glm::vec4 color, float rotY = 0.0f, int matType = 0) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    if (rotY != 0.0f) model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(color));
    glUniform1i(glGetUniformLocation(shader, "matType"), matType);
    glBindVertexArray(VAO); glDrawArrays(GL_TRIANGLES, 0, 36);
}

void drawSphere(unsigned int shader, glm::vec3 pos, float radius, glm::vec4 color, float rotY = 0.0f, int matType = 0) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    if (rotY != 0.0f) model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(radius));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(color));
    glUniform1i(glGetUniformLocation(shader, "matType"), matType);

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
}

void drawNeonText(unsigned int shader, unsigned int VAO, std::string text, glm::vec3 pos, float scale, glm::vec4 color, float rotY = 0.0f) {
    float cursorX = -(text.length() * 4.0f * scale) / 2.0f;
    for (char c : text) {
        if (c == ' ') { cursorX += 4.0f * scale; continue; }
        int idx = toupper(c) - 'A'; if (idx < 0 || idx > 25) continue;
        int bits = voxelFont[idx];
        for (int i = 0; i < 15; i++) {
            if ((bits >> (14 - i)) & 1) {
                int col = i % 3; int row = i / 3;
                glm::vec3 offset(cursorX + col * scale, -row * scale, 0.0f);
                glm::mat4 rotMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
                drawBox(shader, VAO, pos + glm::vec3(rotMatrix * glm::vec4(offset, 1.0f)), glm::vec3(scale * 0.9f), color, rotY, 4);
            }
        }
        cursorX += 4.0f * scale;
    }
}

void drawCharacter(unsigned int shader, unsigned int VAO, glm::vec3 basePos, float rotY, glm::vec4 shirtColor, float walkCycle) {
    glm::vec3 pos = basePos;
    pos.y += abs(sin(walkCycle)) * 0.05f;

    float armSwing = sin(walkCycle) * 35.0f;
    float legSwing = sin(walkCycle) * 25.0f;

    glm::vec4 skinColor = glm::vec4(0.9f, 0.7f, 0.5f, 1.0f);
    glm::vec4 pantsColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    glm::vec4 shoeColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.55f, 0.0f), glm::vec3(0.15f, 0.15f, 0.15f), skinColor, rotY, 0);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.25f, 0.0f), glm::vec3(0.7f, 0.45f, 0.35f), shirtColor, rotY, 4);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.95f, 0.0f), glm::vec3(0.55f, 0.35f, 0.3f), shirtColor * 0.8f, rotY, 4);
    drawSphere(shader, pos + glm::vec3(0.0f, 1.75f, 0.0f), 0.20f, skinColor, rotY, 0);

    auto drawLimb = [&](glm::vec3 jointOffset, glm::vec3 scale, glm::vec4 color, float swingAngle, int mType, bool isArm) {
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec3 worldJoint = glm::vec3(rotMat * glm::vec4(jointOffset, 1.0f));

        model = glm::translate(model, pos + worldJoint); // Move to the joint's attachment point
        model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f)); // Face character direction
        model = glm::rotate(model, glm::radians(swingAngle), glm::vec3(1.0f, 0.0f, 0.0f)); // Swing the limb

        // Shift the limb mesh DOWNwards so its top aligns with the joint pivot point
        model = glm::translate(model, glm::vec3(0.0f, -scale.y / 2.0f, 0.0f));

        glm::mat4 limbModel = model; // Save for extremity attachments (hands/feet)
        model = glm::scale(model, scale);

        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(color));
        glUniform1i(glGetUniformLocation(shader, "matType"), mType);
        glBindVertexArray(VAO); glDrawArrays(GL_TRIANGLES, 0, 36);

        if (isArm) {
            glm::mat4 handModel = glm::translate(limbModel, glm::vec3(0.0f, -scale.y / 2.0f - 0.05f, 0.0f));
            handModel = glm::scale(handModel, glm::vec3(0.12f));
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(handModel));
            glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(skinColor));
            glUniform1i(glGetUniformLocation(shader, "matType"), 0);
            glBindVertexArray(sphereVAO); glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
        else {
            glm::mat4 shoeModel = glm::translate(limbModel, glm::vec3(0.0f, -scale.y / 2.0f, 0.08f));
            shoeModel = glm::scale(shoeModel, glm::vec3(scale.x * 1.1f, 0.15f, scale.z * 1.5f));
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(shoeModel));
            glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(shoeColor));
            glUniform1i(glGetUniformLocation(shader, "matType"), 0);
            glBindVertexArray(VAO); glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        };

    drawLimb(glm::vec3(-0.15f, 0.90f, 0.0f), glm::vec3(0.2f, 0.9f, 0.25f), pantsColor, legSwing, 0, false);
    drawLimb(glm::vec3(0.15f, 0.90f, 0.0f), glm::vec3(0.2f, 0.9f, 0.25f), pantsColor, -legSwing, 0, false);
    drawLimb(glm::vec3(-0.42f, 1.45f, 0.0f), glm::vec3(0.16f, 0.75f, 0.16f), shirtColor, -armSwing, 4, true);
    drawLimb(glm::vec3(0.42f, 1.45f, 0.0f), glm::vec3(0.16f, 0.75f, 0.16f), shirtColor, armSwing, 4, true);
}

void drawPlant(unsigned int shader, unsigned int VAO, glm::vec3 pos) {
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.3f, 0.0f), glm::vec3(0.6f, 0.6f, 0.6f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f), 45.0f, 1);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.8f, 1.0f, 0.8f), glm::vec4(0.1f, 0.6f, 0.2f, 1.0f), 0.0f, 7);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.6f, 0.8f, 0.6f), glm::vec4(0.15f, 0.7f, 0.25f, 1.0f), 30.0f, 7);
}

void drawFountain(unsigned int shader, unsigned int VAO, glm::vec3 pos) {
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(4.0f, 0.4f, 4.0f), glm::vec4(0.6f, 0.6f, 0.65f, 1.0f), 0.0f, 1);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(3.8f, 0.05f, 3.8f), glm::vec4(0.2f, 0.5f, 0.9f, 0.8f), 0.0f, 6);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.8f, 2.0f, 0.8f), glm::vec4(0.7f, 0.7f, 0.75f, 1.0f), 45.0f, 1);
    float rotOffset = programTime * 50.0f;
    for (int i = 0; i < 4; i++) {
        drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.8f, 0.0f), glm::vec3(0.1f, 1.5f, 0.1f), glm::vec4(0.5f, 0.8f, 1.0f, 0.6f), rotOffset + i * 90.0f, 6);
    }
}

void drawElevator(unsigned int shader, unsigned int VAO) {
    float baseY = elevatorY - 1.5f;
    glm::vec3 pos(0.0f, baseY, -26.0f);

    glm::vec4 steelCol = glm::vec4(0.6f, 0.6f, 0.65f, 1.0f);

    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.5f, -1.4f), glm::vec3(3.8f, 3.0f, 0.1f), steelCol, 0.0f, 8);
    drawBox(shader, VAO, pos + glm::vec3(-1.9f, 1.5f, 0.0f), glm::vec3(0.1f, 3.0f, 2.8f), steelCol, 0.0f, 8);
    drawBox(shader, VAO, pos + glm::vec3(1.9f, 1.5f, 0.0f), glm::vec3(0.1f, 3.0f, 2.8f), steelCol, 0.0f, 8);

    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.5f, -1.34f), glm::vec3(3.0f, 2.0f, 0.05f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f), 0.0f, 1);

    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(3.8f, 0.1f, 2.8f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 0.0f, 5);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(3.6f, 0.1f, 2.6f), glm::vec4(1.0f, 1.0f, 0.95f, 1.0f), 0.0f, 4);

    float doorL = -0.95f - (elevatorDoors * 0.95f);
    float doorR = 0.95f + (elevatorDoors * 0.95f);
    drawBox(shader, VAO, pos + glm::vec3(doorL, 1.5f, 1.4f), glm::vec3(1.9f, 3.0f, 0.08f), steelCol, 0.0f, 8);
    drawBox(shader, VAO, pos + glm::vec3(doorR, 1.5f, 1.4f), glm::vec3(1.9f, 3.0f, 0.08f), steelCol, 0.0f, 8);
}

void drawCar(unsigned int shader, unsigned int VAO, glm::vec3 pos, float rotY, glm::vec4 color) {
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(2.0f, 0.5f, 4.5f), color, rotY, 8);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.0f, -0.2f), glm::vec3(1.8f, 0.6f, 2.5f), glm::vec4(0.2f, 0.2f, 0.2f, 0.8f), rotY, 2);
    for (float wx : {-1.0f, 1.0f}) for (float wz : {-1.5f, 1.5f}) {
        glm::mat4 rm = glm::rotate(glm::mat4(1.0f), glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
        drawBox(shader, VAO, pos + glm::vec3(rm * glm::vec4(wx, 0.25f, wz, 1.0f)), glm::vec3(0.3f, 0.5f, 0.5f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), rotY);
    }
    glm::mat4 rm = glm::rotate(glm::mat4(1.0f), glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    drawBox(shader, VAO, pos + glm::vec3(rm * glm::vec4(-0.7f, 0.5f, 2.26f, 1.0f)), glm::vec3(0.3f, 0.2f, 0.1f), glm::vec4(1.0f, 1.0f, 0.8f, 1.0f), rotY, 4);
    drawBox(shader, VAO, pos + glm::vec3(rm * glm::vec4(0.7f, 0.5f, 2.26f, 1.0f)), glm::vec3(0.3f, 0.2f, 0.1f), glm::vec4(1.0f, 1.0f, 0.8f, 1.0f), rotY, 4);
    drawBox(shader, VAO, pos + glm::vec3(rm * glm::vec4(0.7f, 0.5f, -2.26f, 1.0f)), glm::vec3(0.4f, 0.15f, 0.1f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), rotY, 4);
    drawBox(shader, VAO, pos + glm::vec3(rm * glm::vec4(-0.7f, 0.5f, -2.26f, 1.0f)), glm::vec3(0.4f, 0.15f, 0.1f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), rotY, 4);
}

void drawMallProps(unsigned int shader, unsigned int VAO) {
    drawBox(shader, VAO, glm::vec3(-6.0f, 3.5f, -24.0f), glm::vec3(2.0f, 0.6f, 0.1f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
    drawNeonText(shader, VAO, "RESTROOM", glm::vec3(-6.0f, 3.6f, -23.94f), 0.06f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    for (float z : {-10.0f, 10.0f}) {
        drawBox(shader, VAO, glm::vec3(0.0f, 4.7f, z), glm::vec3(0.1f, 0.4f, 0.1f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        drawBox(shader, VAO, glm::vec3(0.0f, 4.5f, z), glm::vec3(0.2f, 0.2f, 0.4f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 45.0f);
        drawBox(shader, VAO, glm::vec3(0.0f, 4.5f, z + 0.21f), glm::vec3(0.1f, 0.1f, 0.05f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 45.0f, 4);
    }

    drawBox(shader, VAO, glm::vec3(0.0f, 1.5f, 10.0f), glm::vec3(2.0f, 1.5f, 0.2f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 0.0f, 8);
    drawBox(shader, VAO, glm::vec3(0.0f, 1.5f, 10.01f), glm::vec3(1.8f, 1.3f, 0.2f), glm::vec4(0.2f, 0.8f, 0.9f, 1.0f), 0.0f, 4);
    drawNeonText(shader, VAO, "MALL MAP", glm::vec3(0.0f, 2.3f, 10.011f), 0.08f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    std::string mapMsg = "DIRECTORY *** L1: FASHION CAFE *** L2: TECH SHOES *** ";
    int mapLen = mapMsg.length();
    int mapOffset = static_cast<int>(programTime * 5.0f) % mapLen;
    std::string mapVis;
    for (int i = 0; i < 14; i++) {
        mapVis += mapMsg[(mapOffset + i) % mapLen];
    }
    drawNeonText(shader, VAO, mapVis, glm::vec3(0.0f, 1.5f, 10.12f), 0.045f, glm::vec4(1.0f, 1.0f, 0.2f, 1.0f));

    drawBox(shader, VAO, glm::vec3(0.0f, 0.4f, 10.0f), glm::vec3(0.4f, 0.8f, 0.4f), glm::vec4(0.4f, 0.4f, 0.4f, 1.0f), 0.0f, 8);

    for (float z : {-15.0f, 15.0f}) {
        drawBox(shader, VAO, glm::vec3(0.0f, 0.4f, z), glm::vec3(2.0f, 0.1f, 0.6f), glm::vec4(0.4f, 0.2f, 0.1f, 1.0f), 0.0f, 3);
        drawBox(shader, VAO, glm::vec3(-0.8f, 0.2f, z), glm::vec3(0.1f, 0.4f, 0.5f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f), 0.0f, 8);
        drawBox(shader, VAO, glm::vec3(0.8f, 0.2f, z), glm::vec3(0.1f, 0.4f, 0.5f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f), 0.0f, 8);

        drawBox(shader, VAO, glm::vec3(1.8f, 0.4f, z), glm::vec3(0.4f, 0.8f, 0.4f), glm::vec4(0.15f, 0.15f, 0.18f, 1.0f), 0.0f, 8);
        drawBox(shader, VAO, glm::vec3(1.8f, 0.4f, z), glm::vec3(0.4f, 0.8f, 0.4f), glm::vec4(0.15f, 0.15f, 0.18f, 1.0f), 45.0f, 8);
        drawBox(shader, VAO, glm::vec3(1.8f, 0.81f, z), glm::vec3(0.3f, 0.02f, 0.3f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        drawPlant(shader, VAO, glm::vec3(-2.5f, 0.0f, z + 2.0f));
        drawPlant(shader, VAO, glm::vec3(2.5f, 0.0f, z - 2.0f));
    }

    // Interactive Billboard Display (Red Kiosk)
    drawBox(shader, VAO, glm::vec3(-2.6f, 1.0f, 12.0f), glm::vec3(1.0f, 2.0f, 0.8f), glm::vec4(0.8f, 0.1f, 0.1f, 1.0f));
    drawBox(shader, VAO, glm::vec3(-2.55f, 1.2f, 12.4f), glm::vec3(0.8f, 1.2f, 0.1f), glm::vec4(0.8f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
    drawBox(shader, VAO, glm::vec3(-2.6f, 1.8f, 12.41f), glm::vec3(0.8f, 0.2f, 0.05f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 4);

    drawNeonText(shader, VAO, "INFO", glm::vec3(-2.6f, 1.6f, 12.46f), 0.06f, glm::vec4(0.2f, 0.8f, 1.0f, 1.0f));

    // Scrolling Marquee Logic
    std::string fullMsg = "WELCOME TO STARLIGHT MALL  ---  MEGA SALE  ---  ";
    int len = fullMsg.length();
    int offset = static_cast<int>(programTime * 4.0f) % len;
    std::string visibleMsg;
    for (int i = 0; i < 11; i++) {
        visibleMsg += fullMsg[(offset + i) % len];
    }
    drawNeonText(shader, VAO, visibleMsg, glm::vec3(-2.55f, 1.25f, 12.46f), 0.04f, glm::vec4(1.0f, 1.0f, 0.2f, 1.0f));
}

// ---------------------------------------------------------
// NEW: Shop Product Generation Logic
// ---------------------------------------------------------
void drawProducts(unsigned int shader, unsigned int VAO, std::string type, glm::vec3 pos, float rotY) {
    // Generates a local coordinate system matching the shop's facing direction
    glm::mat4 rotMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));

    auto drawLocalBox = [&](glm::vec3 offset, glm::vec3 scale, glm::vec4 color, float localRotY = 0.0f, int mat = 0) {
        glm::vec3 worldPos = pos + glm::vec3(rotMatrix * glm::vec4(offset, 1.0f));
        drawBox(shader, VAO, worldPos, scale, color, rotY + localRotY, mat);
        };
    auto drawLocalSphere = [&](glm::vec3 offset, float radius, glm::vec4 color, int mat = 0) {
        glm::vec3 worldPos = pos + glm::vec3(rotMatrix * glm::vec4(offset, 1.0f));
        drawSphere(shader, worldPos, radius, color, rotY, mat);
        };

    if (type == "STYLE") {
        // Display Table
        drawLocalBox(glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(1.4f, 0.8f, 0.8f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        // Folded Shirts
        drawLocalBox(glm::vec3(-0.4f, 0.85f, 0.0f), glm::vec3(0.3f, 0.1f, 0.4f), glm::vec4(0.2f, 0.4f, 0.8f, 1.0f));
        drawLocalBox(glm::vec3(-0.4f, 0.95f, 0.0f), glm::vec3(0.3f, 0.1f, 0.4f), glm::vec4(0.8f, 0.2f, 0.2f, 1.0f));
        drawLocalBox(glm::vec3(0.4f, 0.9f, 0.0f), glm::vec3(0.3f, 0.2f, 0.4f), glm::vec4(0.2f, 0.8f, 0.4f, 1.0f));
        // Mannequin
        drawLocalBox(glm::vec3(1.2f, 0.9f, 0.0f), glm::vec3(0.2f, 0.8f, 0.2f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Stand
        drawLocalBox(glm::vec3(1.2f, 1.5f, 0.0f), glm::vec3(0.5f, 0.6f, 0.25f), glm::vec4(0.9f, 0.2f, 0.3f, 1.0f)); // Torso
        drawLocalSphere(glm::vec3(1.2f, 1.9f, 0.0f), 0.15f, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f)); // Head
    }
    else if (type == "GAMES") {
        // Arcade Machine
        drawLocalBox(glm::vec3(0.0f, 0.6f, 0.0f), glm::vec3(0.8f, 1.2f, 0.8f), glm::vec4(0.15f, 0.15f, 0.15f, 1.0f)); // Base
        drawLocalBox(glm::vec3(0.0f, 1.25f, 0.2f), glm::vec3(0.8f, 0.1f, 0.4f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f)); // Control Panel
        drawLocalBox(glm::vec3(0.0f, 1.6f, -0.1f), glm::vec3(0.8f, 0.6f, 0.6f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Screen Housing
        drawLocalBox(glm::vec3(0.0f, 1.6f, 0.21f), glm::vec3(0.7f, 0.5f, 0.05f), glm::vec4(0.1f, 0.8f, 0.2f, 1.0f), 0.0f, 4); // Glowing Screen
        drawLocalBox(glm::vec3(0.0f, 1.95f, -0.1f), glm::vec3(0.8f, 0.2f, 0.7f), glm::vec4(0.9f, 0.1f, 0.9f, 1.0f), 0.0f, 4); // Glowing Marquee
    }
    else if (type == "CAFE") {
        // Coffee Table
        drawLocalBox(glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(0.1f, 0.8f, 0.1f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f)); // Leg
        drawLocalBox(glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(1.0f, 0.05f, 1.0f), glm::vec4(0.6f, 0.4f, 0.2f, 1.0f)); // Top
        // Chairs
        drawLocalBox(glm::vec3(-0.7f, 0.45f, 0.0f), glm::vec3(0.4f, 0.05f, 0.4f), glm::vec4(0.8f, 0.3f, 0.3f, 1.0f)); // Seat 1
        drawLocalBox(glm::vec3(-0.85f, 0.7f, 0.0f), glm::vec3(0.05f, 0.5f, 0.4f), glm::vec4(0.8f, 0.3f, 0.3f, 1.0f)); // Back 1
        drawLocalBox(glm::vec3(0.7f, 0.45f, 0.0f), glm::vec3(0.4f, 0.05f, 0.4f), glm::vec4(0.8f, 0.3f, 0.3f, 1.0f)); // Seat 2
        drawLocalBox(glm::vec3(0.85f, 0.7f, 0.0f), glm::vec3(0.05f, 0.5f, 0.4f), glm::vec4(0.8f, 0.3f, 0.3f, 1.0f)); // Back 2
        // Coffee Mug & Plant
        drawLocalBox(glm::vec3(-0.2f, 0.88f, 0.1f), glm::vec3(0.1f, 0.15f, 0.1f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawLocalBox(glm::vec3(0.2f, 0.88f, -0.1f), glm::vec3(0.15f, 0.1f, 0.15f), glm::vec4(0.3f, 0.2f, 0.1f, 1.0f));
        drawLocalBox(glm::vec3(0.2f, 1.0f, -0.1f), glm::vec3(0.1f, 0.2f, 0.1f), glm::vec4(0.2f, 0.8f, 0.2f, 1.0f), 0.0f, 7);
    }
    else if (type == "BOOKS") {
        // Bookshelf
        drawLocalBox(glm::vec3(0.0f, 1.0f, -0.4f), glm::vec3(2.0f, 2.0f, 0.05f), glm::vec4(0.3f, 0.2f, 0.1f, 1.0f)); // Back
        drawLocalBox(glm::vec3(-0.95f, 1.0f, -0.2f), glm::vec3(0.05f, 2.0f, 0.4f), glm::vec4(0.3f, 0.2f, 0.1f, 1.0f)); // L Side
        drawLocalBox(glm::vec3(0.95f, 1.0f, -0.2f), glm::vec3(0.05f, 2.0f, 0.4f), glm::vec4(0.3f, 0.2f, 0.1f, 1.0f)); // R Side
        // Shelves & Books
        for (float y : {0.2f, 0.8f, 1.4f}) {
            drawLocalBox(glm::vec3(0.0f, y, -0.2f), glm::vec3(1.9f, 0.05f, 0.4f), glm::vec4(0.3f, 0.2f, 0.1f, 1.0f));
            drawLocalBox(glm::vec3(-0.6f, y + 0.15f, -0.2f), glm::vec3(0.08f, 0.25f, 0.2f), glm::vec4(0.8f, 0.1f, 0.1f, 1.0f));
            drawLocalBox(glm::vec3(-0.4f, y + 0.13f, -0.2f), glm::vec3(0.06f, 0.2f, 0.2f), glm::vec4(0.1f, 0.3f, 0.8f, 1.0f));
            drawLocalBox(glm::vec3(0.2f, y + 0.18f, -0.2f), glm::vec3(0.08f, 0.3f, 0.25f), glm::vec4(0.1f, 0.8f, 0.3f, 1.0f));
            drawLocalBox(glm::vec3(0.3f, y + 0.15f, -0.2f), glm::vec3(0.05f, 0.25f, 0.2f), glm::vec4(0.9f, 0.8f, 0.1f, 1.0f));
        }
    }
    else if (type == "SPORT") {
        // Weight Rack
        drawLocalBox(glm::vec3(-0.8f, 0.6f, 0.0f), glm::vec3(0.1f, 1.2f, 0.4f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        drawLocalBox(glm::vec3(0.8f, 0.6f, 0.0f), glm::vec3(0.1f, 1.2f, 0.4f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        // Barbell
        drawLocalBox(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(2.0f, 0.05f, 0.05f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Bar
        drawLocalBox(glm::vec3(-0.9f, 1.0f, 0.0f), glm::vec3(0.1f, 0.4f, 0.4f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Weight L
        drawLocalBox(glm::vec3(0.9f, 1.0f, 0.0f), glm::vec3(0.1f, 0.4f, 0.4f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Weight R
        // Balls
        drawLocalSphere(glm::vec3(0.0f, 0.2f, 0.3f), 0.2f, glm::vec4(0.9f, 0.4f, 0.1f, 1.0f)); // Basketball
        drawLocalSphere(glm::vec3(-0.4f, 0.15f, 0.4f), 0.15f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Soccer
    }
    else if (type == "TECH") {
        // Desk
        drawLocalBox(glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(1.6f, 1.0f, 0.8f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        // PC Monitor
        drawLocalBox(glm::vec3(0.3f, 1.1f, -0.2f), glm::vec3(0.1f, 0.2f, 0.1f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Stand
        drawLocalBox(glm::vec3(0.3f, 1.3f, -0.15f), glm::vec3(0.7f, 0.4f, 0.05f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Bezel
        drawLocalBox(glm::vec3(0.3f, 1.3f, -0.12f), glm::vec3(0.65f, 0.35f, 0.02f), glm::vec4(0.2f, 0.5f, 1.0f, 1.0f), 0.0f, 4); // Glowing Screen
        // Laptop
        drawLocalBox(glm::vec3(-0.4f, 1.02f, 0.1f), glm::vec3(0.4f, 0.02f, 0.3f), glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)); // Base
        drawLocalBox(glm::vec3(-0.4f, 1.15f, -0.05f), glm::vec3(0.4f, 0.25f, 0.02f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Screen Back
        drawLocalBox(glm::vec3(-0.4f, 1.15f, -0.04f), glm::vec3(0.38f, 0.23f, 0.02f), glm::vec4(1.0f, 0.2f, 0.5f, 1.0f), 0.0f, 4); // Laptop Screen
    }
    else if (type == "SHOES") {
        // Tiered Display Stand
        drawLocalBox(glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(1.2f, 0.4f, 0.8f), glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
        drawLocalBox(glm::vec3(0.0f, 0.6f, -0.2f), glm::vec3(1.2f, 0.4f, 0.4f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        // Shoe 1 (Red)
        drawLocalBox(glm::vec3(-0.3f, 0.45f, 0.2f), glm::vec3(0.2f, 0.1f, 0.3f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawLocalBox(glm::vec3(-0.3f, 0.55f, 0.15f), glm::vec3(0.18f, 0.15f, 0.2f), glm::vec4(0.9f, 0.1f, 0.1f, 1.0f));
        // Shoe 2 (Blue)
        drawLocalBox(glm::vec3(0.3f, 0.45f, 0.2f), glm::vec3(0.2f, 0.1f, 0.3f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawLocalBox(glm::vec3(0.3f, 0.55f, 0.15f), glm::vec3(0.18f, 0.15f, 0.2f), glm::vec4(0.1f, 0.3f, 0.9f, 1.0f));
        // Shoe 3 (Green Top Tier)
        drawLocalBox(glm::vec3(0.0f, 0.85f, -0.2f), glm::vec3(0.2f, 0.1f, 0.3f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        drawLocalBox(glm::vec3(0.0f, 1.0f, -0.25f), glm::vec3(0.18f, 0.3f, 0.2f), glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));
    }
    else if (type == "MUSIC") {
        // Drum Kit
        drawLocalBox(glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(0.6f, 0.6f, 0.4f), glm::vec4(0.8f, 0.1f, 0.1f, 1.0f)); // Bass Drum
        drawLocalBox(glm::vec3(0.0f, 0.4f, 0.21f), glm::vec3(0.62f, 0.62f, 0.05f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Rim F
        drawLocalBox(glm::vec3(0.0f, 0.4f, -0.21f), glm::vec3(0.62f, 0.62f, 0.05f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Rim B
        drawLocalBox(glm::vec3(-0.5f, 0.6f, 0.0f), glm::vec3(0.3f, 0.2f, 0.3f), glm::vec4(0.8f, 0.1f, 0.1f, 1.0f)); // Snare
        drawLocalBox(glm::vec3(0.5f, 0.45f, 0.0f), glm::vec3(0.05f, 0.9f, 0.05f), glm::vec4(0.7f, 0.7f, 0.7f, 1.0f)); // Cymbal Stand
        drawLocalBox(glm::vec3(0.5f, 0.9f, 0.0f), glm::vec3(0.4f, 0.02f, 0.4f), glm::vec4(0.9f, 0.8f, 0.2f, 1.0f), 15.0f); // Cymbal
    }
    else if (type == "CANDY") {
        // Gumball Machine
        drawLocalBox(glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(0.6f, 0.8f, 0.6f), glm::vec4(0.9f, 0.1f, 0.1f, 1.0f)); // Base
        drawLocalBox(glm::vec3(0.0f, 0.3f, 0.31f), glm::vec3(0.2f, 0.2f, 0.05f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Slot
        drawLocalSphere(glm::vec3(0.0f, 1.1f, 0.0f), 0.45f, glm::vec4(0.8f, 0.9f, 1.0f, 0.4f), 2); // Glass Globe
        // Gumballs
        drawLocalSphere(glm::vec3(0.1f, 0.9f, 0.1f), 0.08f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        drawLocalSphere(glm::vec3(-0.1f, 0.95f, 0.0f), 0.08f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        drawLocalSphere(glm::vec3(0.0f, 0.85f, -0.1f), 0.08f, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        drawLocalSphere(glm::vec3(-0.15f, 0.88f, 0.15f), 0.08f, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
        drawLocalSphere(glm::vec3(0.1f, 1.05f, -0.1f), 0.08f, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
        drawLocalBox(glm::vec3(0.0f, 1.55f, 0.0f), glm::vec3(0.3f, 0.1f, 0.3f), glm::vec4(0.9f, 0.1f, 0.1f, 1.0f)); // Cap
    }
    else if (type == "GEAR") {
        // Camping Gear
        drawLocalBox(glm::vec3(0.0f, 0.1f, 0.0f), glm::vec3(1.6f, 0.2f, 1.6f), glm::vec4(0.2f, 0.5f, 0.2f, 1.0f)); // Tent Base
        drawLocalBox(glm::vec3(0.0f, 0.6f, 0.0f), glm::vec3(1.2f, 0.8f, 1.2f), glm::vec4(0.8f, 0.4f, 0.1f, 1.0f)); // Tent Body
        drawLocalBox(glm::vec3(0.0f, 0.5f, 0.61f), glm::vec3(0.4f, 0.6f, 0.05f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)); // Door
        drawLocalBox(glm::vec3(0.0f, 0.25f, 0.0f), glm::vec3(0.4f, 0.1f, 0.8f), glm::vec4(0.2f, 0.4f, 0.8f, 1.0f)); // Sleeping Bag
        // Campfire
        drawLocalBox(glm::vec3(-0.5f, 0.1f, 0.8f), glm::vec3(0.4f, 0.05f, 0.4f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f)); // Stones
        drawLocalBox(glm::vec3(-0.5f, 0.2f, 0.8f), glm::vec3(0.15f, 0.2f, 0.15f), glm::vec4(1.0f, 0.5f, 0.0f, 1.0f), 45.0f, 4); // Fire
    }
}

// ---------------------------------------------------------
// FSM Struct for Intelligent Avatars
// ---------------------------------------------------------
enum NPCBehavior {
    WANDER_MALL,
    GO_TO_ESCALATOR,
    RIDE_ESCALATOR,
    GO_TO_ELEVATOR,
    WAIT_FOR_ELEVATOR,
    RIDE_ELEVATOR,
    VISIT_SHOP
};

struct Shopper {
    glm::vec3 pos;
    glm::vec3 dir;
    glm::vec4 color;
    float speed;
    float walkCycle;

    NPCBehavior currentState;
    glm::vec3 targetPos;
    float waitTimer;
};

// ---------------------------------------------------------
// Main Entry
// ---------------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 900, "Virtual Shopping Mall", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); });
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float vertices[] = {
        -0.5f,-0.5f,-0.5f, 0.0f,0.0f,-1.0f,  0.5f,-0.5f,-0.5f, 0.0f,0.0f,-1.0f,  0.5f,0.5f,-0.5f, 0.0f,0.0f,-1.0f,
         0.5f,0.5f,-0.5f, 0.0f,0.0f,-1.0f, -0.5f,0.5f,-0.5f, 0.0f,0.0f,-1.0f, -0.5f,-0.5f,-0.5f, 0.0f,0.0f,-1.0f,
        -0.5f,-0.5f, 0.5f, 0.0f,0.0f, 1.0f,  0.5f,-0.5f, 0.5f, 0.0f,0.0f, 1.0f,  0.5f,0.5f, 0.5f, 0.0f,0.0f, 1.0f,
         0.5f,0.5f, 0.5f, 0.0f,0.0f, 1.0f, -0.5f,0.5f, 0.5f, 0.0f,0.0f, 1.0f, -0.5f,-0.5f, 0.5f, 0.0f,0.0f, 1.0f,
        -0.5f,0.5f, 0.5f,-1.0f,0.0f,0.0f, -0.5f,0.5f,-0.5f,-1.0f,0.0f,0.0f, -0.5f,-0.5f,-0.5f,-1.0f,0.0f,0.0f,
        -0.5f,-0.5f,-0.5f,-1.0f,0.0f,0.0f, -0.5f,-0.5f, 0.5f,-1.0f,0.0f,0.0f, -0.5f,0.5f, 0.5f,-1.0f,0.0f,0.0f,
         0.5f,0.5f, 0.5f, 1.0f,0.0f,0.0f,  0.5f,0.5f,-0.5f, 1.0f,0.0f,0.0f,  0.5f,-0.5f,-0.5f, 1.0f,0.0f,0.0f,
         0.5f,-0.5f,-0.5f, 1.0f,0.0f,0.0f,  0.5f,-0.5f, 0.5f, 1.0f,0.0f,0.0f,  0.5f,0.5f, 0.5f, 1.0f,0.0f,0.0f,
        -0.5f,-0.5f,-0.5f, 0.0f,-1.0f,0.0f,  0.5f,-0.5f,-0.5f, 0.0f,-1.0f,0.0f,  0.5f,-0.5f, 0.5f, 0.0f,-1.0f,0.0f,
         0.5f,-0.5f, 0.5f, 0.0f,-1.0f,0.0f, -0.5f,-0.5f, 0.5f, 0.0f,-1.0f,0.0f, -0.5f,-0.5f,-0.5f, 0.0f,-1.0f,0.0f,
        -0.5f,0.5f,-0.5f, 0.0f,1.0f,0.0f,  0.5f,0.5f,-0.5f, 0.0f,1.0f,0.0f,  0.5f,0.5f, 0.5f, 0.0f,1.0f,0.0f,
         0.5f,0.5f, 0.5f, 0.0f,1.0f,0.0f, -0.5f,0.5f, 0.5f, 0.0f,1.0f,0.0f, -0.5f,0.5f,-0.5f, 0.0f,1.0f,0.0f
    };

    unsigned int VAO, VBO; glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);

    initSphere();

    unsigned int vShader = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vShader, 1, &vertexShaderSource, NULL); glCompileShader(vShader);
    unsigned int fShader = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fShader, 1, &fragmentShaderSource, NULL); glCompileShader(fShader);
    unsigned int shaderProgram = glCreateProgram(); glAttachShader(shaderProgram, vShader); glAttachShader(shaderProgram, fShader); glLinkProgram(shaderProgram);

    glm::vec3 pointLightPositions[16]; glm::vec3 pointLightColors[16]; int lIdx = 0;
    for (int z = -20; z <= 20; z += 10) {
        pointLightPositions[lIdx] = glm::vec3(-7.0f, 4.0f, z); pointLightColors[lIdx++] = glm::vec3(1.0f, 0.9f, 0.8f);
        pointLightPositions[lIdx] = glm::vec3(7.0f, 4.0f, z);  pointLightColors[lIdx++] = glm::vec3(0.9f, 0.95f, 1.0f);
    }
    pointLightPositions[lIdx] = glm::vec3(-3.0f, 6.0f, 29.0f); pointLightColors[lIdx++] = glm::vec3(1.0f, 0.5f, 1.0f);
    pointLightPositions[lIdx] = glm::vec3(3.0f, 6.0f, 29.0f);  pointLightColors[lIdx++] = glm::vec3(0.5f, 1.0f, 1.0f);
    pointLightPositions[lIdx] = glm::vec3(-8.0f, 7.0f, 38.0f); pointLightColors[lIdx++] = glm::vec3(1.0f, 0.8f, 0.4f);
    pointLightPositions[lIdx] = glm::vec3(8.0f, 7.0f, 38.0f);  pointLightColors[lIdx++] = glm::vec3(1.0f, 0.8f, 0.4f);
    pointLightPositions[lIdx] = glm::vec3(-8.0f, 7.0f, 48.0f); pointLightColors[lIdx++] = glm::vec3(1.0f, 0.8f, 0.4f);
    pointLightPositions[lIdx] = glm::vec3(8.0f, 7.0f, 48.0f);  pointLightColors[lIdx++] = glm::vec3(1.0f, 0.8f, 0.4f);

    std::string shopNamesL[5] = { "STYLE", "GAMES", "CAFE", "BOOKS", "SPORT" };
    std::string shopNamesR[5] = { "TECH", "SHOES", "MUSIC", "CANDY", "GEAR" };

    glm::vec4 keeperColorsL[5] = {
        glm::vec4(0.9f, 0.1f, 0.9f, 1.0f), glm::vec4(1.0f, 0.9f, 0.0f, 1.0f),
        glm::vec4(0.1f, 1.0f, 0.2f, 1.0f), glm::vec4(1.0f, 0.4f, 0.0f, 1.0f), glm::vec4(0.2f, 0.5f, 1.0f, 1.0f)
    };
    glm::vec4 keeperColorsR[5] = {
        glm::vec4(0.0f, 0.9f, 0.9f, 1.0f), glm::vec4(1.0f, 0.1f, 0.1f, 1.0f),
        glm::vec4(0.7f, 0.1f, 1.0f, 1.0f), glm::vec4(0.9f, 0.8f, 0.3f, 1.0f), glm::vec4(1.0f, 0.5f, 0.8f, 1.0f)
    };

    std::vector<Shopper> roamingShoppers = {
        {{ -4.0f, 0.0f,   5.0f }, { 0.0f, 0.0f,  1.0f }, glm::vec4(0.2f, 0.8f, 0.9f, 1.0f), 1.5f, 0.0f, WANDER_MALL, {0,0,0}, 0.0f},
        {{  4.0f, 0.0f,   5.0f }, { 0.0f, 0.0f, -1.0f }, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f), 1.6f, 2.0f, GO_TO_ESCALATOR, {1.0f, 1.5f, 1.0f}, 0.0f},
        {{ -4.0f, 5.0f,  12.0f }, { 0.0f, 0.0f, -1.0f }, glm::vec4(0.3f, 0.9f, 0.4f, 1.0f), 1.4f, 1.5f, WANDER_MALL, {0,0,0}, 0.0f},
        {{  4.0f, 5.0f,  -2.0f }, { 0.0f, 0.0f, -1.0f }, glm::vec4(0.8f, 0.8f, 0.2f, 1.0f), 1.5f, 3.5f, GO_TO_ELEVATOR, {0.0f, 6.5f, -22.0f}, 0.0f},
        {{  0.0f, 0.0f,  18.0f }, { 0.0f, 0.0f, -1.0f }, glm::vec4(0.6f, 0.2f, 0.9f, 1.0f), 1.6f, 4.0f, VISIT_SHOP, {-8.0f, 1.5f, 10.0f}, 0.0f}
    };

    std::vector<Shopper> activeShopkeepers;
    int shopIdxLoad = 0;
    for (int z = -20; z <= 20; z += 10) {
        for (float y : {0.0f, 5.0f}) {
            activeShopkeepers.push_back({
                glm::vec3(-12.5f, y, z), glm::vec3(0.0f, 0.0f, 1.0f),
                keeperColorsL[shopIdxLoad], 1.0f + (rand() % 10) / 20.0f, static_cast<float>(rand() % 10),
                WANDER_MALL, {0,0,0}, 0.0f
                });
            activeShopkeepers.push_back({
                glm::vec3(12.5f, y, z), glm::vec3(0.0f, 0.0f, -1.0f),
                keeperColorsR[shopIdxLoad], 1.0f + (rand() % 10) / 20.0f, static_cast<float>(rand() % 10),
                WANDER_MALL, {0,0,0}, 0.0f
                });
        }
        shopIdxLoad++;
    }

    std::cout << "--- VIRTUAL SHOPPING MALL 2.0 ---" << std::endl;
    std::cout << "Added custom 3D products in the shops! Watch out for fixed mannequin arms!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << " [W/A/S/D] - Move around" << std::endl;
    std::cout << " [Space]   - Jump / Fly Up (Drone Mode)" << std::endl;
    std::cout << " [L-Shift] - Fly Down (Drone Mode)" << std::endl;
    std::cout << " [V]       - Toggle Drone View (Free Flight)" << std::endl;
    std::cout << " [N]       - Toggle Day/Night View" << std::endl;
    std::cout << " [F]       - Toggle Flashlight" << std::endl;
    std::cout << " [T]       - Toggle Cinematic Tour" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime()); deltaTime = currentFrame - lastFrame; lastFrame = currentFrame; programTime += deltaTime;

        processInput(window);
        updateDynamics();

        // Update clear color based on Day/Night
        if (isNight) {
            glClearColor(0.01f, 0.01f, 0.03f, 1.0f);
        }
        else {
            glClearColor(0.55f, 0.75f, 0.95f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glUniform1f(glGetUniformLocation(shaderProgram, "time"), programTime);
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f)));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp)));

        // Pass Day/Night state to Shader
        glUniform1i(glGetUniformLocation(shaderProgram, "isNight"), isNight);

        // Update Sun/Moon directional light based on Day/Night
        if (isNight) {
            glUniform3f(glGetUniformLocation(shaderProgram, "dirLightDir"), -0.2f, -1.0f, -0.3f);
            glUniform3f(glGetUniformLocation(shaderProgram, "dirLightColor"), 0.1f, 0.1f, 0.2f);
        }
        else {
            glUniform3f(glGetUniformLocation(shaderProgram, "dirLightDir"), -0.3f, -1.0f, -0.2f);
            glUniform3f(glGetUniformLocation(shaderProgram, "dirLightColor"), 0.8f, 0.8f, 0.8f);
        }

        for (int i = 0; i < 16; i++) {
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLightPositions[" + std::to_string(i) + "]").c_str()), 1, glm::value_ptr(pointLightPositions[i]));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLightColors[" + std::to_string(i) + "]").c_str()), 1, glm::value_ptr(pointLightColors[i]));
        }

        glUniform3fv(glGetUniformLocation(shaderProgram, "spotLightPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "spotLightDir"), 1, glm::value_ptr(cameraFront));
        glUniform3f(glGetUniformLocation(shaderProgram, "spotLightColor"), 1.0f, 0.95f, 0.8f);
        glUniform1f(glGetUniformLocation(shaderProgram, "spotCutOff"), glm::cos(glm::radians(12.5f)));
        glUniform1f(glGetUniformLocation(shaderProgram, "spotOuterCutOff"), glm::cos(glm::radians(17.5f)));
        glUniform1i(glGetUniformLocation(shaderProgram, "flashlightOn"), flashlightOn);

        // ==========================================
        // 1. EXTERIOR WORLD
        // ==========================================
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, -0.4f, 45.0f), glm::vec3(50.0f, 0.5f, 34.0f), glm::vec4(0.15f, 0.15f, 0.15f, 1.0f), 0.0f, 5);
        for (float z = 30.0f; z < 60.0f; z += 4.0f) drawBox(shaderProgram, VAO, glm::vec3(0.0f, -0.14f, z), glm::vec3(0.4f, 0.02f, 2.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, -0.25f, 29.0f), glm::vec3(50.0f, 0.5f, 2.0f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));

        for (float z : {38.0f, 48.0f}) for (float x : {-8.0f, 8.0f}) {
            drawBox(shaderProgram, VAO, glm::vec3(x, 3.5f, z), glm::vec3(0.2f, 7.0f, 0.2f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 0.0f, 8);
            drawBox(shaderProgram, VAO, glm::vec3(x, 7.0f, z), glm::vec3(1.0f, 0.2f, 0.4f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 0.0f, 8);
            drawBox(shaderProgram, VAO, glm::vec3(x, 6.9f, z), glm::vec3(0.8f, 0.1f, 0.3f), glm::vec4(1.0f, 0.9f, 0.6f, 1.0f), 0.0f, 4);
        }

        drawCar(shaderProgram, VAO, glm::vec3(-6.0f, -0.1f, 34.0f), 90.0f, glm::vec4(0.8f, 0.1f, 0.1f, 1.0f));
        drawCar(shaderProgram, VAO, glm::vec3(6.0f, -0.1f, 32.0f), -90.0f, glm::vec4(0.1f, 0.1f, 0.8f, 1.0f));

        // ==========================================
        // 2. MALL INTERIOR STRUCTURE
        // ==========================================
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(40.0f, 0.5f, 56.0f), glm::vec4(0.4f, 0.4f, 0.45f, 1.0f), 0.0f, 1);

        glm::vec4 floorCol2 = glm::vec4(0.4f, 0.4f, 0.45f, 1.0f);
        drawBox(shaderProgram, VAO, glm::vec3(-8.5f, 4.75f, 0.0f), glm::vec3(11.0f, 0.5f, 56.0f), floorCol2, 0.0f, 1);
        drawBox(shaderProgram, VAO, glm::vec3(8.5f, 4.75f, 0.0f), glm::vec3(11.0f, 0.5f, 56.0f), floorCol2, 0.0f, 1);
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 4.75f, -19.0f), glm::vec3(6.0f, 0.5f, 18.0f), floorCol2, 0.0f, 1);
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 4.75f, 21.5f), glm::vec3(6.0f, 0.5f, 13.0f), floorCol2, 0.0f, 1);

        glm::vec4 wallColor = glm::vec4(0.15f, 0.15f, 0.18f, 1.0f);
        glm::vec4 shopBaseColor = glm::vec4(0.2f, 0.2f, 0.22f, 1.0f);

        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.0f, -28.0f), glm::vec3(40.0f, 10.0f, 1.0f), wallColor);
        drawBox(shaderProgram, VAO, glm::vec3(-11.5f, 5.0f, 28.0f), glm::vec3(17.0f, 10.0f, 1.0f), wallColor);
        drawBox(shaderProgram, VAO, glm::vec3(11.5f, 5.0f, 28.0f), glm::vec3(17.0f, 10.0f, 1.0f), wallColor);
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 7.5f, 28.0f), glm::vec3(6.0f, 5.0f, 1.0f), wallColor);

        glm::vec4 gateFrameColor = glm::vec4(0.3f, 0.3f, 0.35f, 1.0f);
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 6.0f, 31.0f), glm::vec3(12.0f, 0.8f, 6.0f), gateFrameColor, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(-5.5f, 3.0f, 33.5f), glm::vec3(1.0f, 6.0f, 1.0f), gateFrameColor, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(5.5f, 3.0f, 33.5f), glm::vec3(1.0f, 6.0f, 1.0f), gateFrameColor, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 7.5f, 34.0f), glm::vec3(10.0f, 2.5f, 0.2f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
        drawNeonText(shaderProgram, VAO, "STARLIGHT MALL", glm::vec3(0.0f, 7.8f, 34.15f), 0.22f, glm::vec4(1.0f, 0.8f, 0.2f, 1.0f));
        drawNeonText(shaderProgram, VAO, "GRAND ENTRANCE", glm::vec3(0.0f, 6.8f, 34.15f), 0.1f, glm::vec4(1.0f, 0.8f, 0.2f, 1.0f));

        float doorOffset = entranceOpenness * 1.5f;
        glm::vec4 dFrameCol = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f);
        drawBox(shaderProgram, VAO, glm::vec3(-3.0f - doorOffset, 2.5f, 28.0f), glm::vec3(0.2f, 5.0f, 0.2f), dFrameCol, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(0.0f - doorOffset, 2.5f, 28.0f), glm::vec3(0.2f, 5.0f, 0.2f), dFrameCol, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(-1.5f - doorOffset, 0.1f, 28.0f), glm::vec3(3.0f, 0.2f, 0.2f), dFrameCol, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(-1.5f - doorOffset, 4.9f, 28.0f), glm::vec3(3.0f, 0.2f, 0.2f), dFrameCol, 0.0f, 8);

        drawBox(shaderProgram, VAO, glm::vec3(0.0f + doorOffset, 2.5f, 28.0f), glm::vec3(0.2f, 5.0f, 0.2f), dFrameCol, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(3.0f + doorOffset, 2.5f, 28.0f), glm::vec3(0.2f, 5.0f, 0.2f), dFrameCol, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(1.5f + doorOffset, 0.1f, 28.0f), glm::vec3(3.0f, 0.2f, 0.2f), dFrameCol, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(1.5f + doorOffset, 4.9f, 28.0f), glm::vec3(3.0f, 0.2f, 0.2f), dFrameCol, 0.0f, 8);

        drawBox(shaderProgram, VAO, glm::vec3(-4.0f, 10.0f, 0.0f), glm::vec3(0.5f, 0.5f, 56.0f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(4.0f, 10.0f, 0.0f), glm::vec3(0.5f, 0.5f, 56.0f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 0.0f, 8);
        for (float z = -25; z <= 25; z += 5.0f) drawBox(shaderProgram, VAO, glm::vec3(0.0f, 10.0f, z), glm::vec3(8.0f, 0.5f, 0.2f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 0.0f, 8);

        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 2.5f, -5.0f), glm::vec3(0.6f, 5.0f, 10.5f), glm::vec4(0.15f, 0.15f, 0.15f, 1.0f), 0.0f, 8);
        for (int i = 0; i < 40; i++) {
            float moveL = fmod(programTime * 2.0f + i * 0.25f, 10.0f);
            float zL = -10.0f + moveL;
            float yL = 5.0f - (moveL / 10.0f) * 5.0f;
            drawBox(shaderProgram, VAO, glm::vec3(-1.0f, yL, zL), glm::vec3(1.4f, 0.2f, 0.3f), glm::vec4(0.25f, 0.25f, 0.3f, 1.0f), 0.0f, 8);

            float moveR = fmod(programTime * 2.0f + i * 0.25f, 10.0f);
            float zR = 0.0f - moveR;
            float yR = (moveR / 10.0f) * 5.0f;
            drawBox(shaderProgram, VAO, glm::vec3(1.0f, yR, zR), glm::vec3(1.4f, 0.2f, 0.3f), glm::vec4(0.25f, 0.25f, 0.3f, 1.0f), 0.0f, 8);
        }

        drawFountain(shaderProgram, VAO, glm::vec3(0.0f, 0.0f, 15.0f));

        // ==========================================
        // 3. ELEVATOR & SHAFT RENDERING
        // ==========================================
        drawElevator(shaderProgram, VAO);

        glm::vec4 shaftWallCol = glm::vec4(0.2f, 0.2f, 0.22f, 1.0f);
        glm::vec4 metalDoorCol = glm::vec4(0.5f, 0.5f, 0.55f, 1.0f);
        glm::vec4 premiumMetal = glm::vec4(0.8f, 0.8f, 0.85f, 1.0f);

        for (float yLvl : {0.0f, 5.0f}) {
            drawBox(shaderProgram, VAO, glm::vec3(-2.95f, yLvl + 2.5f, -24.5f), glm::vec3(2.1f, 5.0f, 0.2f), shaftWallCol, 0.0f, 1);
            drawBox(shaderProgram, VAO, glm::vec3(2.95f, yLvl + 2.5f, -24.5f), glm::vec3(2.1f, 5.0f, 0.2f), shaftWallCol, 0.0f, 1);
            drawBox(shaderProgram, VAO, glm::vec3(0.0f, yLvl + 4.0f, -24.5f), glm::vec3(3.8f, 2.0f, 0.2f), shaftWallCol, 0.0f, 1);

            float currentFloorDoors = 0.0f;
            if (yLvl == 0.0f && elevatorState == 0) currentFloorDoors = elevatorDoors;
            if (yLvl == 5.0f && elevatorState == 2) currentFloorDoors = elevatorDoors;

            float sDoorL = -0.95f - (currentFloorDoors * 0.95f);
            float sDoorR = 0.95f + (currentFloorDoors * 0.95f);
            drawBox(shaderProgram, VAO, glm::vec3(sDoorL, yLvl + 1.5f, -24.5f), glm::vec3(1.9f, 3.0f, 0.05f), metalDoorCol, 0.0f, 8);
            drawBox(shaderProgram, VAO, glm::vec3(sDoorR, yLvl + 1.5f, -24.5f), glm::vec3(1.9f, 3.0f, 0.05f), metalDoorCol, 0.0f, 8);

            drawBox(shaderProgram, VAO, glm::vec3(-1.95f, yLvl + 1.5f, -24.45f), glm::vec3(0.1f, 3.0f, 0.1f), premiumMetal, 0.0f, 8);
            drawBox(shaderProgram, VAO, glm::vec3(1.95f, yLvl + 1.5f, -24.45f), glm::vec3(0.1f, 3.0f, 0.1f), premiumMetal, 0.0f, 8);
            drawBox(shaderProgram, VAO, glm::vec3(0.0f, yLvl + 3.05f, -24.45f), glm::vec3(4.0f, 0.1f, 0.1f), premiumMetal, 0.0f, 8);

            glm::vec4 indCol = (elevatorState == 0 || elevatorState == 2) ? glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            drawBox(shaderProgram, VAO, glm::vec3(0.0f, yLvl + 3.2f, -24.4f), glm::vec3(0.8f, 0.1f, 0.05f), indCol, 0.0f, 4);
        }

        drawMallProps(shaderProgram, VAO);

        // ==========================================
        // 4. ANIMATED AVATARS (FSM AI LOGIC)
        // ==========================================
        for (auto& shopper : roamingShoppers) {
            bool isWalking = false;

            switch (shopper.currentState) {
            case WANDER_MALL: {
                glm::vec3 nextPos = shopper.pos + shopper.dir * (shopper.speed * deltaTime);
                if (isPositionValid(nextPos.x + (shopper.dir.x * 0.4f), nextPos.y + 1.5f, nextPos.z + (shopper.dir.z * 0.4f))) {
                    shopper.pos = nextPos;
                }
                else {
                    float angle = (rand() % 360) * 3.14159f / 180.0f;
                    shopper.dir = glm::vec3(cos(angle), 0.0f, sin(angle));
                }
                isWalking = true;
                break;
            }
            case GO_TO_ESCALATOR: {
                glm::vec3 diff = shopper.targetPos - shopper.pos;
                diff.y = 0;
                if (glm::length(diff) > 0.2f) {
                    shopper.dir = glm::normalize(diff);
                    shopper.pos += shopper.dir * shopper.speed * deltaTime;
                    isWalking = true;
                }
                else {
                    shopper.currentState = RIDE_ESCALATOR;
                }
                break;
            }
            case RIDE_ESCALATOR: {
                if (shopper.pos.x > 0.0f) {
                    shopper.dir = glm::vec3(0.0f, 0.0f, -1.0f);
                    shopper.pos.z -= 2.5f * deltaTime;
                    if (shopper.pos.z <= -10.5f) shopper.currentState = WANDER_MALL;
                }
                else {
                    shopper.dir = glm::vec3(0.0f, 0.0f, 1.0f);
                    shopper.pos.z += 2.5f * deltaTime;
                    if (shopper.pos.z >= 0.5f) shopper.currentState = WANDER_MALL;
                }
                isWalking = false;
                break;
            }
            case GO_TO_ELEVATOR: {
                glm::vec3 diff = shopper.targetPos - shopper.pos;
                diff.y = 0;
                if (glm::length(diff) > 0.2f) {
                    shopper.dir = glm::normalize(diff);
                    shopper.pos += shopper.dir * shopper.speed * deltaTime;
                    isWalking = true;
                }
                else {
                    shopper.currentState = WAIT_FOR_ELEVATOR;
                    shopper.dir = glm::vec3(0.0f, 0.0f, -1.0f);
                }
                break;
            }
            case WAIT_FOR_ELEVATOR: {
                isWalking = false;
                bool elevatorHere = (abs(elevatorY - (shopper.pos.y + 1.5f)) < 1.0f);
                bool doorsOpen = (elevatorDoors > 0.8f);

                if (elevatorHere && doorsOpen) {
                    shopper.targetPos = glm::vec3(0.0f, shopper.pos.y, -26.0f);
                    glm::vec3 diff = shopper.targetPos - shopper.pos;
                    diff.y = 0;
                    if (glm::length(diff) > 0.2f) {
                        shopper.dir = glm::normalize(diff);
                        shopper.pos += shopper.dir * shopper.speed * deltaTime;
                        isWalking = true;
                    }
                    else {
                        shopper.currentState = RIDE_ELEVATOR;
                        shopper.dir = glm::vec3(0.0f, 0.0f, 1.0f);
                    }
                }
                break;
            }
            case RIDE_ELEVATOR: {
                isWalking = false;
                shopper.pos.y = elevatorY - 1.5f;

                bool doorsOpen = (elevatorDoors > 0.8f);
                if (doorsOpen && (elevatorState == 0 || elevatorState == 2)) {
                    shopper.targetPos = glm::vec3(0.0f, shopper.pos.y, -20.0f);
                    glm::vec3 diff = shopper.targetPos - shopper.pos;
                    diff.y = 0;
                    if (glm::length(diff) > 0.2f) {
                        shopper.dir = glm::normalize(diff);
                        shopper.pos += shopper.dir * shopper.speed * deltaTime;
                        isWalking = true;
                    }
                    else {
                        shopper.currentState = WANDER_MALL;
                    }
                }
                break;
            }
            case VISIT_SHOP: {
                glm::vec3 diff = shopper.targetPos - shopper.pos;
                diff.y = 0;
                if (glm::length(diff) > 0.2f) {
                    shopper.dir = glm::normalize(diff);
                    shopper.pos += shopper.dir * shopper.speed * deltaTime;
                    isWalking = true;
                }
                else {
                    shopper.waitTimer += deltaTime;
                    isWalking = false;
                    if (shopper.waitTimer > 5.0f) {
                        shopper.currentState = WANDER_MALL;
                        float angle = (rand() % 360) * 3.14159f / 180.0f;
                        shopper.dir = glm::vec3(cos(angle), 0.0f, sin(angle));
                    }
                }
                break;
            }
            }

            if (shopper.currentState != RIDE_ELEVATOR) {
                shopper.pos.y = getFloorHeight(shopper.pos.x, shopper.pos.y + 1.5f, shopper.pos.z) - 1.5f;
            }

            if (isWalking) shopper.walkCycle += deltaTime * shopper.speed * 4.0f;
            else shopper.walkCycle = 0.0f;

            float rotY = glm::degrees(atan2(shopper.dir.x, shopper.dir.z));
            drawCharacter(shaderProgram, VAO, shopper.pos, rotY, shopper.color, shopper.walkCycle);
        }

        // ==========================================
        // 5. SEAMLESS SHOPS AND SHOPKEEPERS
        // ==========================================
        for (auto& keeper : activeShopkeepers) {
            glm::vec3 nextPos = keeper.pos + keeper.dir * (keeper.speed * deltaTime);
            if (isPositionValid(nextPos.x + (keeper.dir.x * 0.4f), nextPos.y + 1.5f, nextPos.z + (keeper.dir.z * 0.4f))) {
                keeper.pos = nextPos;
            }
            else {
                keeper.dir *= -1.0f;
            }
            keeper.pos.y = getFloorHeight(keeper.pos.x, keeper.pos.y + 1.5f, keeper.pos.z) - 1.5f;
            keeper.walkCycle += deltaTime * keeper.speed * 4.0f;
            float rotY = (keeper.dir.z > 0.0f) ? 0.0f : 180.0f;
            drawCharacter(shaderProgram, VAO, keeper.pos, rotY, keeper.color, keeper.walkCycle);
        }

        int shopIdx = 0;
        for (int z = -20; z <= 20; z += 10) {
            for (float y : {0.0f, 5.0f}) {
                glm::vec3 lPos(-10.0f, y + 2.5f, z);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(-3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 10.0f), shopBaseColor);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(8.0f, 5.0f, 0.2f), shopBaseColor);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(8.0f, 5.0f, 0.2f), shopBaseColor);

                drawBox(shaderProgram, VAO, lPos + glm::vec3(3.0f, 0.0f, -3.4f), glm::vec3(0.2f, 5.0f, 3.2f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(3.0f, 0.0f, 3.4f), glm::vec3(0.2f, 5.0f, 3.2f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(3.0f, 2.0f, 0.0f), glm::vec3(0.2f, 1.0f, 10.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);

                glm::vec4 sColL = glm::vec4(0.2f + abs(z % 3) * 0.4f, 0.8f, 0.9f - abs(z % 4) * 0.2f, 1.0f);
                drawBox(shaderProgram, VAO, glm::vec3(-6.85f, y + 4.2f, z), glm::vec3(0.05f, 1.2f, 4.0f), glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), 0.0f, 8);
                drawNeonText(shaderProgram, VAO, shopNamesL[shopIdx], glm::vec3(-6.8f, y + 4.2f, z), 0.08f, sColL, 90.0f);

                drawBox(shaderProgram, VAO, glm::vec3(-10.0f, y + 0.5f, z), glm::vec3(3.0f, 1.0f, 2.0f), sColL * 0.5f);
                drawNeonText(shaderProgram, VAO, "SALE", glm::vec3(-8.4f, y + 1.3f, z), 0.05f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 90.0f);

                // ---> NEW: Render 3D Products in Left Shops
                drawProducts(shaderProgram, VAO, shopNamesL[shopIdx], glm::vec3(-8.5f, y, z), 90.0f);

                glm::vec3 rPos(10.0f, y + 2.5f, z);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 10.0f), shopBaseColor);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(8.0f, 5.0f, 0.2f), shopBaseColor);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(8.0f, 5.0f, 0.2f), shopBaseColor);

                drawBox(shaderProgram, VAO, rPos + glm::vec3(-3.0f, 0.0f, -3.4f), glm::vec3(0.2f, 5.0f, 3.2f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(-3.0f, 0.0f, 3.4f), glm::vec3(0.2f, 5.0f, 3.2f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(-3.0f, 2.0f, 0.0f), glm::vec3(0.2f, 1.0f, 10.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);

                glm::vec4 sColR = glm::vec4(0.9f, 0.3f + abs(z % 2) * 0.3f, 0.3f, 1.0f);
                drawBox(shaderProgram, VAO, glm::vec3(6.85f, y + 4.2f, z), glm::vec3(0.05f, 1.2f, 4.0f), glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), 0.0f, 8);
                drawNeonText(shaderProgram, VAO, shopNamesR[shopIdx], glm::vec3(6.8f, y + 4.2f, z), 0.08f, sColR, -90.0f);

                drawBox(shaderProgram, VAO, glm::vec3(10.0f, y + 0.5f, z), glm::vec3(3.0f, 1.0f, 2.0f), sColR * 0.5f);
                drawNeonText(shaderProgram, VAO, "NEW", glm::vec3(8.4f, y + 1.3f, z), 0.05f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), -90.0f);

                // ---> NEW: Render 3D Products in Right Shops
                drawProducts(shaderProgram, VAO, shopNamesR[shopIdx], glm::vec3(8.5f, y, z), -90.0f);
            }
            shopIdx++;
        }

        // ==========================================
        // 6. TRANSPARENT OBJECTS (Draw Last)
        // ==========================================
        glm::vec4 gateGlassColor = glm::vec4(0.0f, 1.0f, 0.7f, 0.5f);

        float lX = -1.5f - doorOffset;
        drawBox(shaderProgram, VAO, glm::vec3(lX, 2.4f, 28.0f), glm::vec3(2.8f, 4.8f, 0.05f), gateGlassColor, 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(lX - 1.4f, 2.4f, 28.0f), glm::vec3(0.1f, 4.8f, 0.1f), premiumMetal, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(lX + 1.4f, 2.4f, 28.0f), glm::vec3(0.1f, 4.8f, 0.1f), premiumMetal, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(lX, 0.05f, 28.0f), glm::vec3(3.0f, 0.1f, 0.1f), premiumMetal, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(lX, 4.75f, 28.0f), glm::vec3(3.0f, 0.1f, 0.1f), premiumMetal, 0.0f, 8);

        float rX = 1.5f + doorOffset;
        drawBox(shaderProgram, VAO, glm::vec3(rX, 2.4f, 28.0f), glm::vec3(2.8f, 4.8f, 0.05f), gateGlassColor, 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(rX - 1.4f, 2.4f, 28.0f), glm::vec3(0.1f, 4.8f, 0.1f), premiumMetal, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(rX + 1.4f, 2.4f, 28.0f), glm::vec3(0.1f, 4.8f, 0.1f), premiumMetal, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(rX, 0.05f, 28.0f), glm::vec3(3.0f, 0.1f, 0.1f), premiumMetal, 0.0f, 8);
        drawBox(shaderProgram, VAO, glm::vec3(rX, 4.75f, 28.0f), glm::vec3(3.0f, 0.1f, 0.1f), premiumMetal, 0.0f, 8);

        for (int z = -20; z <= 20; z += 10) {
            for (float y : {0.0f, 5.0f}) {
                drawBox(shaderProgram, VAO, glm::vec3(-7.0f, y + 2.0f, z - 3.4f), glm::vec3(0.05f, 4.0f, 3.2f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(-7.0f, y + 2.0f, z + 3.4f), glm::vec3(0.05f, 4.0f, 3.2f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(7.0f, y + 2.0f, z - 3.4f), glm::vec3(0.05f, 4.0f, 3.2f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(7.0f, y + 2.0f, z + 3.4f), glm::vec3(0.05f, 4.0f, 3.2f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);

                drawBox(shaderProgram, VAO, glm::vec3(-7.0f, y + 2.0f, z), glm::vec3(0.05f, 4.0f, 3.6f), glm::vec4(0.6f, 0.8f, 1.0f, 0.1f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(7.0f, y + 2.0f, z), glm::vec3(0.05f, 4.0f, 3.6f), glm::vec4(0.6f, 0.8f, 1.0f, 0.1f), 0.0f, 2);
            }
        }

        glm::vec4 glassRailingColor = glm::vec4(0.7f, 0.9f, 1.0f, 0.3f);
        drawBox(shaderProgram, VAO, glm::vec3(-3.1f, 5.5f, 2.5f), glm::vec3(0.05f, 1.5f, 25.0f), glassRailingColor, 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(3.1f, 5.5f, 2.5f), glm::vec3(0.05f, 1.5f, 25.0f), glassRailingColor, 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.5f, 15.0f), glm::vec3(6.0f, 1.5f, 0.05f), glassRailingColor, 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(-2.5f, 5.5f, -10.0f), glm::vec3(1.0f, 1.5f, 0.05f), glassRailingColor, 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(2.5f, 5.5f, -10.0f), glm::vec3(1.0f, 1.5f, 0.05f), glassRailingColor, 0.0f, 2);

        drawBox(shaderProgram, VAO, glm::vec3(-1.8f, 3.5f, -5.0f), glm::vec3(0.05f, 7.0f, 10.5f), glm::vec4(0.7f, 0.9f, 1.0f, 0.4f), 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(1.8f, 3.5f, -5.0f), glm::vec3(0.05f, 7.0f, 10.5f), glm::vec4(0.7f, 0.9f, 1.0f, 0.4f), 0.0f, 2);

        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 10.2f, 0.0f), glm::vec3(8.0f, 0.1f, 56.0f), glm::vec4(0.6f, 0.8f, 1.0f, 0.2f), 0.0f, 2);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &sphereVAO); glDeleteBuffers(1, &sphereVBO); glDeleteBuffers(1, &sphereEBO);
    glDeleteProgram(shaderProgram); glfwTerminate(); return 0;
}