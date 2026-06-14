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
float elevatorY = 1.5f; // Tracks Camera Eye Level
int elevatorState = 0;
float elevatorTimer = 0.0f;
float elevatorDoors = 0.0f;

// Interactivity
bool flashlightOn = false;
bool fKeyPressed = false;

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
    vec3 skyColor = vec3(0.01, 0.01, 0.03); 
    
    FragColor = vec4(mix(result, skyColor, fogFactor), alpha);
}
)";

// ---------------------------------------------------------
// Physics & Collision Subsystem
// ---------------------------------------------------------
float getFloorHeight(float x, float currentY, float z) {
    if (x >= -2.0f && x <= 2.0f && z >= -10.0f && z <= 0.0f) {
        float clampedZ = glm::clamp(z, -10.0f, 0.0f);
        return 1.5f + (((0.0f - clampedZ) / 10.0f) * 5.0f);
    }
    // Elevator is now cleanly tracked by an override, so we just return normal mall floors here
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

    // Elevator Shaft Collision
    if (z < -24.4f && z > -28.0f && std::abs(x) < 2.5f) {
        if (z > -24.8f && z < -24.4f) {
            float clearWidth = 0.2f + (elevatorDoors * 1.5f);
            if (std::abs(x) > clearWidth) return false;
        }
        if (x < -1.9f || x > 1.9f || z < -27.4f) return false; // Solid Shaft Walls
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
        if (std::abs(x) > 6.8f && std::abs(x) < 7.2f && z > sz - 4.5f && z < sz + 4.5f) {
            if (!(z > sz - 1.5f && z < sz + 1.5f)) return false;
        }
        if (z > sz + 4.5f && z < sz + 5.5f && std::abs(x) > 7.0f) return false;
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
    float elvSpeed = 2.5f; // Faster modern elevator
    if (elevatorState == 0) {
        elevatorDoors = std::min(elevatorDoors + deltaTime * 2.0f, 1.0f);
        if (elevatorTimer > 6.0f) { elevatorState = 1; elevatorTimer = 0.0f; } // Wait 6 secs
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
        if (!tKeyPressed) { tourMode = !tourMode; tKeyPressed = true; if (!tourMode) firstMouse = true; }
    }
    else tKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!fKeyPressed) { flashlightOn = !flashlightOn; fKeyPressed = true; }
    }
    else fKeyPressed = false;

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

    // FIXED: Flawless override for elevator tracking
    if (onElevator) {
        cameraPos.y = elevatorY; // Eye level lock
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

void drawShopkeeper(unsigned int shader, unsigned int VAO, glm::vec3 pos, float rotY, glm::vec4 shirtColor) {
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(0.5f, 0.8f, 0.3f), glm::vec4(0.15f, 0.15f, 0.15f, 1.0f), rotY);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.15f, 0.0f), glm::vec3(0.6f, 0.7f, 0.35f), shirtColor, rotY, 4); // Neon glow
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.65f, 0.0f), glm::vec3(0.35f, 0.35f, 0.35f), glm::vec4(0.9f, 0.7f, 0.5f, 1.0f), rotY);
    glm::mat4 rMat = glm::rotate(glm::mat4(1.0f), glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    drawBox(shader, VAO, pos + glm::vec3(rMat * glm::vec4(0.4f, 1.15f, 0.0f, 1.0f)), glm::vec3(0.2f, 0.7f, 0.2f), shirtColor, rotY, 4);
    drawBox(shader, VAO, pos + glm::vec3(rMat * glm::vec4(-0.4f, 1.15f, 0.0f, 1.0f)), glm::vec3(0.2f, 0.7f, 0.2f), shirtColor, rotY, 4);
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
    // FIXED: Real-World Elevator Architecture
    float baseY = elevatorY - 1.5f; // Translate eye level to floor level
    glm::vec3 pos(0.0f, baseY, -26.0f);

    glm::vec4 steelCol = glm::vec4(0.6f, 0.6f, 0.65f, 1.0f);

    // Premium Steel Cabin Walls
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.5f, -1.4f), glm::vec3(3.8f, 3.0f, 0.1f), steelCol, 0.0f, 8); // Back
    drawBox(shader, VAO, pos + glm::vec3(-1.9f, 1.5f, 0.0f), glm::vec3(0.1f, 3.0f, 2.8f), steelCol, 0.0f, 8); // Left
    drawBox(shader, VAO, pos + glm::vec3(1.9f, 1.5f, 0.0f), glm::vec3(0.1f, 3.0f, 2.8f), steelCol, 0.0f, 8); // Right

    // Glossy Back Mirror
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.5f, -1.34f), glm::vec3(3.0f, 2.0f, 0.05f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f), 0.0f, 1);

    // Matte Black Grip Floor
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(3.8f, 0.1f, 2.8f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 0.0f, 5);
    // Emissive LED Ceiling Light
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(3.6f, 0.1f, 2.6f), glm::vec4(1.0f, 1.0f, 0.95f, 1.0f), 0.0f, 4);

    // Dynamic Proper Split Sliding Doors
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

    drawBox(shader, VAO, glm::vec3(-2.6f, 1.0f, 12.0f), glm::vec3(1.0f, 2.0f, 0.8f), glm::vec4(0.8f, 0.1f, 0.1f, 1.0f));
    drawBox(shader, VAO, glm::vec3(-2.55f, 1.2f, 12.4f), glm::vec3(0.8f, 1.2f, 0.1f), glm::vec4(0.8f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
    drawBox(shader, VAO, glm::vec3(-2.6f, 1.8f, 12.41f), glm::vec3(0.8f, 0.2f, 0.05f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 4);
}

// ---------------------------------------------------------
// Main Entry
// ---------------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Virtual Mall 2.0", NULL, NULL);
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

    std::cout << "--- VIRTUAL MALL 2.0 ---" << std::endl;
    std::cout << "Find the premium real-world elevator in the back." << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime()); deltaTime = currentFrame - lastFrame; lastFrame = currentFrame; programTime += deltaTime;

        processInput(window);
        updateDynamics();

        glClearColor(0.01f, 0.01f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glUniform1f(glGetUniformLocation(shaderProgram, "time"), programTime);
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f)));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp)));

        glUniform3f(glGetUniformLocation(shaderProgram, "dirLightDir"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLightColor"), 0.1f, 0.1f, 0.2f);

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

        // Render external shaft structural elements on both 1F and 2F
        glm::vec4 shaftSteel = glm::vec4(0.3f, 0.3f, 0.35f, 1.0f);
        for (float yLvl : {0.0f, 5.0f}) {
            drawBox(shaderProgram, VAO, glm::vec3(-2.2f, yLvl + 1.5f, -24.5f), glm::vec3(0.6f, 3.0f, 0.2f), shaftSteel, 0.0f, 8); // L Frame
            drawBox(shaderProgram, VAO, glm::vec3(2.2f, yLvl + 1.5f, -24.5f), glm::vec3(0.6f, 3.0f, 0.2f), shaftSteel, 0.0f, 8); // R Frame
            drawBox(shaderProgram, VAO, glm::vec3(0.0f, yLvl + 3.2f, -24.5f), glm::vec3(5.0f, 0.4f, 0.2f), shaftSteel, 0.0f, 8); // Top Frame

            // LED Status Indicator Light (Green = Safe to board, Red = Moving)
            glm::vec4 indCol = (elevatorState == 0 || elevatorState == 2) ? glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            drawBox(shaderProgram, VAO, glm::vec3(0.0f, yLvl + 3.2f, -24.39f), glm::vec3(0.8f, 0.1f, 0.05f), indCol, 0.0f, 4);
        }

        drawMallProps(shaderProgram, VAO);

        int shopIdx = 0;
        for (int z = -20; z <= 20; z += 10) {
            for (float y : {0.0f, 5.0f}) {
                glm::vec3 lPos(-10.0f, y + 2.5f, z);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(-3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 8.0f), shopBaseColor);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(0.0f, 0.0f, -3.9f), glm::vec3(8.0f, 5.0f, 0.2f), shopBaseColor);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(0.0f, 0.0f, 3.9f), glm::vec3(8.0f, 5.0f, 0.2f), shopBaseColor);

                drawBox(shaderProgram, VAO, lPos + glm::vec3(3.0f, 0.0f, -2.5f), glm::vec3(0.2f, 5.0f, 3.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(3.0f, 0.0f, 2.5f), glm::vec3(0.2f, 5.0f, 3.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(3.0f, 2.0f, 0.0f), glm::vec3(0.2f, 1.0f, 8.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);

                glm::vec4 sColL = glm::vec4(0.2f + abs(z % 3) * 0.4f, 0.8f, 0.9f - abs(z % 4) * 0.2f, 1.0f);
                drawBox(shaderProgram, VAO, glm::vec3(-6.85f, y + 4.2f, z), glm::vec3(0.05f, 1.2f, 4.0f), glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), 0.0f, 8);
                drawNeonText(shaderProgram, VAO, shopNamesL[shopIdx], glm::vec3(-6.8f, y + 4.2f, z), 0.08f, sColL, 90.0f);
                drawBox(shaderProgram, VAO, glm::vec3(-10.0f, y + 0.5f, z), glm::vec3(3.0f, 1.0f, 2.0f), sColL * 0.5f);
                drawNeonText(shaderProgram, VAO, "SALE", glm::vec3(-8.4f, y + 1.3f, z), 0.05f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 90.0f);

                drawShopkeeper(shaderProgram, VAO, glm::vec3(-11.5f, y, z), 90.0f, keeperColorsL[shopIdx]);

                glm::vec3 rPos(10.0f, y + 2.5f, z);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 8.0f), shopBaseColor);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(0.0f, 0.0f, -3.9f), glm::vec3(8.0f, 5.0f, 0.2f), shopBaseColor);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(0.0f, 0.0f, 3.9f), glm::vec3(8.0f, 5.0f, 0.2f), shopBaseColor);

                drawBox(shaderProgram, VAO, rPos + glm::vec3(-3.0f, 0.0f, -2.5f), glm::vec3(0.2f, 5.0f, 3.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(-3.0f, 0.0f, 2.5f), glm::vec3(0.2f, 5.0f, 3.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(-3.0f, 2.0f, 0.0f), glm::vec3(0.2f, 1.0f, 8.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f), 0.0f, 8);

                glm::vec4 sColR = glm::vec4(0.9f, 0.3f + abs(z % 2) * 0.3f, 0.3f, 1.0f);
                drawBox(shaderProgram, VAO, glm::vec3(6.85f, y + 4.2f, z), glm::vec3(0.05f, 1.2f, 4.0f), glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), 0.0f, 8);
                drawNeonText(shaderProgram, VAO, shopNamesR[shopIdx], glm::vec3(6.8f, y + 4.2f, z), 0.08f, sColR, -90.0f);
                drawBox(shaderProgram, VAO, glm::vec3(10.0f, y + 0.5f, z), glm::vec3(3.0f, 1.0f, 2.0f), sColR * 0.5f);
                drawNeonText(shaderProgram, VAO, "NEW", glm::vec3(8.4f, y + 1.3f, z), 0.05f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), -90.0f);

                drawShopkeeper(shaderProgram, VAO, glm::vec3(11.5f, y, z), -90.0f, keeperColorsR[shopIdx]);
            }
            drawBox(shaderProgram, VAO, glm::vec3(-7.0f, 5.0f, z + 4.8f), glm::vec3(0.8f, 10.0f, 0.8f), glm::vec4(0.15f, 0.15f, 0.15f, 1.0f), 0.0f, 1);
            drawBox(shaderProgram, VAO, glm::vec3(7.0f, 5.0f, z + 4.8f), glm::vec3(0.8f, 10.0f, 0.8f), glm::vec4(0.15f, 0.15f, 0.15f, 1.0f), 0.0f, 1);
            shopIdx++;
        }

        // ==========================================
        // 4. TRANSPARENT OBJECTS (Draw Last)
        // ==========================================

        glm::vec4 gateGlassColor = glm::vec4(0.0f, 1.0f, 0.7f, 0.5f);
        drawBox(shaderProgram, VAO, glm::vec3(-1.5f - doorOffset, 2.5f, 28.0f), glm::vec3(2.6f, 4.6f, 0.1f), gateGlassColor, 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(1.5f + doorOffset, 2.5f, 28.0f), glm::vec3(2.6f, 4.6f, 0.1f), gateGlassColor, 0.0f, 2);

        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 4.5f, 27.95f), glm::vec3(1.5f, 0.5f, 0.05f), glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), 0.0f, 8);
        drawNeonText(shaderProgram, VAO, "EXIT", glm::vec3(0.0f, 4.5f, 27.9f), 0.05f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 180.0f);

        for (int z = -20; z <= 20; z += 10) {
            for (float y : {0.0f, 5.0f}) {
                drawBox(shaderProgram, VAO, glm::vec3(-7.0f, y + 2.0f, z - 2.5f), glm::vec3(0.05f, 4.0f, 2.5f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(-7.0f, y + 2.0f, z + 2.5f), glm::vec3(0.05f, 4.0f, 2.5f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(7.0f, y + 2.0f, z - 2.5f), glm::vec3(0.05f, 4.0f, 2.5f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(7.0f, y + 2.0f, z + 2.5f), glm::vec3(0.05f, 4.0f, 2.5f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(-7.0f, y + 2.0f, z), glm::vec3(0.05f, 4.0f, 2.0f), glm::vec4(0.6f, 0.8f, 1.0f, 0.1f), 0.0f, 2);
                drawBox(shaderProgram, VAO, glm::vec3(7.0f, y + 2.0f, z), glm::vec3(0.05f, 4.0f, 2.0f), glm::vec4(0.6f, 0.8f, 1.0f, 0.1f), 0.0f, 2);
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
    glDeleteProgram(shaderProgram); glfwTerminate(); return 0;
}