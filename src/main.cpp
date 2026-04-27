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

// ---------------------------------------------------------
// Global Camera, Input & Physics Variables
// ---------------------------------------------------------
glm::vec3 cameraPos   = glm::vec3(0.0f, 1.5f,  25.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

bool firstMouse = true;
float yaw   = -90.0f;   
float pitch =  0.0f;
float lastX =  640.0f;
float lastY =  360.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Physics
float velocityY = 0.0f;
float gravity = 15.0f;
float jumpForce = 6.0f;
bool isGrounded = true;

// Interactivity
bool flashlightOn = false;
bool fKeyPressed = false;

// ---------------------------------------------------------
// NPC / Wandering Characters Data
// ---------------------------------------------------------
struct NPC {
    glm::vec3 pos;
    glm::vec3 target;
    float speed;
    glm::vec4 color;
    float rotY;
    float waitTimer;
    bool isWalking;
    bool hasBounds;
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;
    int floor; // 1 = Ground Floor, 2 = Balcony Floor
};
std::vector<NPC> npcList;

float randFloat(float min, float max) {
    return min + (max - min) * (rand() / (float)RAND_MAX);
}

// ---------------------------------------------------------
// Automated Cinematic Tour System
// ---------------------------------------------------------
bool tourMode = false; 
bool tKeyPressed = false;
float tourProgress = 0.0f;
int currentWaypoint = 0;

struct Waypoint {
    glm::vec3 pos;
    glm::vec3 lookAt;
};

// Updated tour points to utilize the continuous 2nd floor corridor loop!
std::vector<Waypoint> tourPoints = {
    {{0.0f, 1.5f, 28.0f}, {0.0f, 1.5f, 10.0f}},    // Entrance
    {{0.0f, 1.5f, 10.0f}, {-10.0f, 3.0f, 10.0f}},  // Center, pan left
    {{0.0f, 1.5f, 0.0f},  {10.0f, 3.0f, 0.0f}},    // Center, pan right
    {{-1.5f, 1.5f, -2.0f}, {-1.5f, 6.5f, -10.0f}}, // Base of escalator (up lane)
    {{-1.5f, 6.5f, -10.0f}, {-6.0f, 6.5f, -10.0f}},// Top of escalator on bridge
    {{-4.5f, 6.5f, -10.0f}, {-4.5f, 6.5f, 0.0f}},  // Left balcony corridor start
    {{-4.5f, 6.5f, 10.0f}, {0.0f, 1.5f, 5.0f}},    // Look down at plants
    {{-4.5f, 6.5f, 20.0f}, {4.5f, 6.5f, 20.0f}},   // Move to back bridge
    {{0.0f,  6.5f, 20.0f}, {0.0f, 10.0f, 10.0f}},  // Center back bridge look skylight
    {{4.5f,  6.5f, 20.0f}, {4.5f, 6.5f, 0.0f}},    // Right balcony corridor
    {{4.5f,  6.5f, 10.0f}, {0.0f, 1.5f, 5.0f}},    // Right look down
    {{4.5f,  6.5f, -10.0f}, {1.5f, 6.5f, -10.0f}}, // Back to front landing bridge
    {{1.5f,  6.5f, -10.0f}, {1.5f, 1.5f, -2.0f}},  // Top of escalator (down lane)
    {{1.5f,  1.5f, -2.0f}, {0.0f, 1.5f, 28.0f}}    // Base of escalator look entrance
};

void updateTour(float dt) {
    if (!tourMode) return;
    float tourSpeed = 0.2f; 
    tourProgress += dt * tourSpeed;
    if (tourProgress >= 1.0f) {
        tourProgress -= 1.0f;
        currentWaypoint = (currentWaypoint + 1) % tourPoints.size();
    }
    int nextIndex = (currentWaypoint + 1) % tourPoints.size();
    float t = tourProgress * tourProgress * (3.0f - 2.0f * tourProgress);
    
    Waypoint p1 = tourPoints[currentWaypoint];
    Waypoint p2 = tourPoints[nextIndex];
    
    cameraPos = glm::mix(p1.pos, p2.pos, t);
    glm::vec3 currentLookAt = glm::mix(p1.lookAt, p2.lookAt, t);
    cameraFront = glm::normalize(currentLookAt - cameraPos);
    
    pitch = glm::degrees(asin(cameraFront.y));
    yaw = glm::degrees(atan2(cameraFront.z, cameraFront.x));
}

// ---------------------------------------------------------
// Advanced Shaders (Procedural Materials & Lighting)
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

void main() {
    LocalPos = aPos;
    FragPos = vec3(model * vec4(aPos, 1.0));
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
uniform int matType; // 0=Solid, 1=Marble Floor, 2=Glass, 3=Wood, 4=Neon

// Lights
uniform vec3 dirLightDir;
uniform vec3 dirLightColor;

#define NR_POINT_LIGHTS 10
uniform vec3 pointLightPositions[NR_POINT_LIGHTS];
uniform vec3 pointLightColors[NR_POINT_LIGHTS];

uniform vec3 spotLightPos;
uniform vec3 spotLightDir;
uniform vec3 spotLightColor;
uniform float spotCutOff;
uniform float spotOuterCutOff;
uniform bool flashlightOn;

vec3 getMaterialAlbedo(out float specularStrength, out float shininess) {
    vec3 albedo = objectColor.rgb;
    specularStrength = 0.5;
    shininess = 32.0;

    if (matType == 1) { // Marble Tile Floor
        float scale = 1.5;
        vec2 grid = fract(FragPos.xz * scale);
        float border = 0.03;
        if(grid.x < border || grid.y < border) {
            albedo *= 0.5; // Dark grout
            specularStrength = 0.1;
        } else {
            // Procedural marble-like variance
            float check = mod(floor(FragPos.x * scale) + floor(FragPos.z * scale), 2.0);
            albedo *= (check > 0.5 ? 1.0 : 0.85);
            specularStrength = 1.0; // Highly reflective
            shininess = 128.0;
        }
    } 
    else if (matType == 2) { // Glass
        specularStrength = 1.5;
        shininess = 256.0;
    }
    else if (matType == 3) { // Wood
        float grain = fract(LocalPos.x * 20.0 + sin(LocalPos.z * 10.0) * 0.1);
        albedo *= (0.8 + 0.2 * grain);
        specularStrength = 0.2;
        shininess = 16.0;
    }
    return albedo;
}

vec3 CalcDirLight(vec3 normal, vec3 viewDir, vec3 albedo, float specStr, float shiny) {
    vec3 lightDir = normalize(-dirLightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shiny);
    
    vec3 ambient  = 0.45 * dirLightColor * albedo;
    vec3 diffuse  = diff * dirLightColor * albedo;
    vec3 specular = specStr * spec * dirLightColor;
    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(int i, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo, float specStr, float shiny) {
    vec3 lightDir = normalize(pointLightPositions[i] - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shiny);
    
    float distance = length(pointLightPositions[i] - fragPos);
    float attenuation = 1.0 / (1.0 + 0.045 * distance + 0.0075 * (distance * distance));    
    
    vec3 ambient  = 0.15 * pointLightColors[i] * albedo;
    vec3 diffuse  = diff * pointLightColors[i] * albedo;
    vec3 specular = specStr * spec * pointLightColors[i];
    return (ambient + diffuse + specular) * attenuation;
}

vec3 CalcSpotLight(vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo, float specStr, float shiny) {
    if(!flashlightOn) return vec3(0.0);
    vec3 lightDir = normalize(spotLightPos - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shiny);
    
    float distance = length(spotLightPos - fragPos);
    float attenuation = 1.0 / (1.0 + 0.045 * distance + 0.0075 * (distance * distance));    
    
    float theta = dot(lightDir, normalize(-spotLightDir)); 
    float epsilon = spotCutOff - spotOuterCutOff;
    float intensity = clamp((theta - spotOuterCutOff) / epsilon, 0.0, 1.0);
    
    vec3 ambient = 0.1 * spotLightColor * albedo;
    vec3 diffuse = diff * spotLightColor * albedo;
    vec3 specular = specStr * spec * spotLightColor;
    return (ambient + diffuse + specular) * attenuation * intensity;
}

void main() {
    if (matType == 4) { // Neon / Emissive
        FragColor = vec4(objectColor.rgb * 1.5, objectColor.a);
        return;
    }

    float specStr, shiny;
    vec3 albedo = getMaterialAlbedo(specStr, shiny);
    
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    vec3 result = CalcDirLight(norm, viewDir, albedo, specStr, shiny);
    for(int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(i, norm, FragPos, viewDir, albedo, specStr, shiny);    
    result += CalcSpotLight(norm, FragPos, viewDir, albedo, specStr, shiny);
    
    FragColor = vec4(result, objectColor.a);
}
)";

// ---------------------------------------------------------
// Callbacks & Input
// ---------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (tourMode) return; 

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos; lastY = ypos;

    float sensitivity = 0.1f;
    yaw += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = normalize(front);
}

// ---------------------------------------------------------
// Strict 3D Collision Map (SOLID WALLS & RAILINGS)
// ---------------------------------------------------------
bool isPositionValid(float x, float y, float z) {
    // Determine if on the escalator ramp
    bool onEscalator = (x >= -3.0f && x <= 3.0f && z >= -8.5f && z <= 0.5f);
    
    // --- ELEVATION AND VOID PROTECTION ---
    if (y > 4.0f && !onEscalator) {
        // Must be standing on a valid 2nd Floor surface to avoid falling in the void
        bool valid2F = false;
        if (x <= -3.0f && x >= -14.0f) valid2F = true; // Left balcony corridor & shops
        if (x >= 3.0f && x <= 14.0f) valid2F = true; // Right balcony corridor & shops
        if (x > -3.0f && x < 3.0f && z <= -8.0f && z >= -12.0f) valid2F = true; // Front Bridge
        if (x > -3.0f && x < 3.0f && z <= 22.0f && z >= 18.0f) valid2F = true; // Back Bridge

        if (!valid2F) return false; 

        // 2nd Floor Glass Railings (Physical barriers)
        // Left side continuous railing
        if (x > -3.3f && x < -2.9f) {
            if (z < -12.0f) return false;
            if (z > -8.0f && z < 18.0f) return false;
            if (z > 22.0f) return false;
        }
        // Right side continuous railing
        if (x > 2.9f && x < 3.3f) {
            if (z < -12.0f) return false;
            if (z > -8.0f && z < 18.0f) return false;
            if (z > 22.0f) return false;
        }
        // Bridge guardrails
        if (z > -12.1f && z < -11.7f && x > -3.0f && x < 3.0f) return false; // Front bridge back
        if (z > 21.7f && z < 22.1f && x > -3.0f && x < 3.0f) return false; // Back bridge back
        if (z > 17.9f && z < 18.3f && x > -3.0f && x < 3.0f) return false; // Back bridge front

    } else if (y < 4.0f && !onEscalator) {
        // 1st Floor Area - Block the solid side rails of the escalator, leaving lanes open
        if (x > -3.2f && x < -2.0f && z > -8.2f && z < 0.2f) return false; // Left rail
        if (x >  2.0f && x <  3.2f && z > -8.2f && z < 0.2f) return false; // Right rail
        if (x > -0.5f && x <  0.5f && z > -8.2f && z < 0.2f) return false; // Center divider
    }

    // --- SOLID SHOP WALLS AND OBSTACLES ---
    for (int sz = -20; sz <= 20; sz += 10) {
        bool fullyOpen = (sz % 20 == 0); // Every other shop is fully open layout
        
        // Grand Pillars Collision
        if (x > -6.2f && x < -5.4f && z > sz + 4.4f && z < sz + 5.2f) return false;
        if (x >  5.4f && x <  6.2f && z > sz + 4.4f && z < sz + 5.2f) return false;

        // ONLY CHECK WALLS IF WE ARE STANDING NEAR THIS SPECIFIC SHOP
        // (This prevents the infinite glass wall projection bug!)
        if (z > sz - 5.0f && z < sz + 5.0f) {
            // Left Side Shops
            if (x < -5.5f) {
                // Solid Side walls between shops
                if (z > sz - 4.1f && z < sz - 3.7f) return false;
                if (z > sz + 3.7f && z < sz + 4.1f) return false;
                // Back wall of shop
                if (x < -13.7f) return false;
                
                // Glass Front Wall (if not an open layout)
                if (!fullyOpen && x > -6.5f && x < -5.5f) {
                    if (z < sz - 1.2f || z > sz + 1.2f) return false; 
                }
                
                // Interior objects
                if (x > -11.8f && x < -8.2f && z > sz - 1.3f && z < sz + 1.3f) return false; // Center display table
            }
            
            // Right Side Shops
            if (x > 5.5f) {
                // Solid Side walls between shops
                if (z > sz - 4.1f && z < sz - 3.7f) return false;
                if (z > sz + 3.7f && z < sz + 4.1f) return false;
                // Back wall of shop
                if (x > 13.7f) return false;
                
                // Glass Front Wall
                if (!fullyOpen && x < 6.5f && x > 5.5f) {
                    if (z < sz - 1.2f || z > sz + 1.2f) return false;
                }
                
                // Interior objects
                if (x > 8.2f && x < 11.8f && z > sz - 1.3f && z < sz + 1.3f) return false; // Center display table
            }
        }
    }
    
    // Absolute limits of the mall structure
    if (z < -28.0f || z > 28.0f) return false;
    if (x < -14.0f || x > 14.0f) return false;

    return true;
}

float getFloorHeight(float x, float currentY, float z) {
    // Escalator Ramp
    if (x >= -3.0f && x <= 3.0f && z >= -8.5f && z <= 0.5f) {
        float clampedZ = glm::clamp(z, -8.0f, 0.0f);
        float normalizedZ = (0.0f - clampedZ) / 8.0f; 
        return 1.5f + (normalizedZ * 5.0f); 
    }
    
    // 2nd Floor (Includes the balconies and bridges)
    if (currentY > 4.0f) {
        if (x <= -3.0f || x >= 3.0f || 
           (x > -3.0f && x < 3.0f && z <= -8.0f && z >= -12.0f) || 
           (x > -3.0f && x < 3.0f && z <= 22.0f && z >= 18.0f)) {
            return 6.5f; 
        }
    }
    
    return 1.5f; 
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        if (!tKeyPressed) { 
            tourMode = !tourMode; 
            tKeyPressed = true; 
            std::cout << "Tour Mode: " << (tourMode ? "ON" : "OFF") << std::endl;
            if (!tourMode) firstMouse = true; 
        }
    } else tKeyPressed = false;

    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!fKeyPressed) { flashlightOn = !flashlightOn; fKeyPressed = true; }
    } else fKeyPressed = false;

    if (tourMode) { updateTour(deltaTime); return; }

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
        float dx = moveDir.x * cameraSpeed;
        float dz = moveDir.z * cameraSpeed;
        
        if (isPositionValid(cameraPos.x + dx, cameraPos.y, cameraPos.z)) cameraPos.x += dx;
        if (isPositionValid(cameraPos.x, cameraPos.y, cameraPos.z + dz)) cameraPos.z += dz;
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded) {
        velocityY = jumpForce;
        isGrounded = false;
    }

    velocityY -= gravity * deltaTime;
    cameraPos.y += velocityY * deltaTime;

    float currentFloorHeight = getFloorHeight(cameraPos.x, cameraPos.y, cameraPos.z);
    if (cameraPos.y <= currentFloorHeight) {
        cameraPos.y = currentFloorHeight;
        velocityY = 0.0f;
        isGrounded = true;
    }
}

// ---------------------------------------------------------
// Drawing Utilities
// ---------------------------------------------------------
void drawBox(unsigned int shader, unsigned int VAO, glm::vec3 position, glm::vec3 scale, glm::vec4 color, float rotY = 0.0f, int matType = 0) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    if (rotY != 0.0f) model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);
    
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(color));
    glUniform1i(glGetUniformLocation(shader, "matType"), matType);
    
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

// Draws a detailed humanoid NPC
void drawMan(unsigned int shader, unsigned int VAO, glm::vec3 pos, float rotY, glm::vec4 shirtColor, bool isWalking, float time) {
    float swing = isWalking ? sin(time * 12.0f) * 35.0f : 0.0f;
    glm::vec4 skin = glm::vec4(0.8f, 0.6f, 0.45f, 1.0f);
    glm::vec4 pants = glm::vec4(0.25f, 0.25f, 0.3f, 1.0f);

    // Torso
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.3f, 0.0f), glm::vec3(0.5f, 0.8f, 0.25f), shirtColor, rotY);
    // Head
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.9f, 0.0f), glm::vec3(0.3f, 0.35f, 0.3f), skin, rotY);
    
    // Left Leg
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos + glm::vec3(-0.15f, 0.9f, 0.0f));
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(swing), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::translate(model, glm::vec3(0.0f, -0.45f, 0.0f));
    model = glm::scale(model, glm::vec3(0.2f, 0.9f, 0.2f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(pants));
    glUniform1i(glGetUniformLocation(shader, "matType"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Right Leg
    model = glm::mat4(1.0f);
    model = glm::translate(model, pos + glm::vec3(0.15f, 0.9f, 0.0f));
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(-swing), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::translate(model, glm::vec3(0.0f, -0.45f, 0.0f));
    model = glm::scale(model, glm::vec3(0.2f, 0.9f, 0.2f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Left Arm
    model = glm::mat4(1.0f);
    model = glm::translate(model, pos + glm::vec3(-0.35f, 1.6f, 0.0f));
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(-swing), glm::vec3(1.0f, 0.0f, 0.0f)); 
    model = glm::translate(model, glm::vec3(0.0f, -0.35f, 0.0f));
    model = glm::scale(model, glm::vec3(0.15f, 0.7f, 0.15f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(skin));
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Right Arm
    model = glm::mat4(1.0f);
    model = glm::translate(model, pos + glm::vec3(0.35f, 1.6f, 0.0f));
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(swing), glm::vec3(1.0f, 0.0f, 0.0f)); 
    model = glm::translate(model, glm::vec3(0.0f, -0.35f, 0.0f));
    model = glm::scale(model, glm::vec3(0.15f, 0.7f, 0.15f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void drawShopInterior(unsigned int shader, unsigned int VAO, glm::vec3 center, bool isLeft) {
    drawBox(shader, VAO, center + glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(3.0f, 0.8f, 2.0f), glm::vec4(0.5f, 0.3f, 0.2f, 1.0f), 0.0f, 3);
    drawBox(shader, VAO, center + glm::vec3(-1.0f, 0.9f, 0.0f), glm::vec3(0.4f), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
    drawBox(shader, VAO, center + glm::vec3( 0.0f, 0.9f, 0.0f), glm::vec3(0.3f, 0.6f, 0.3f), glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
    drawBox(shader, VAO, center + glm::vec3( 1.0f, 0.9f, 0.0f), glm::vec3(0.5f, 0.2f, 0.5f), glm::vec4(0.2f, 0.5f, 1.0f, 1.0f));
    
    float backX = isLeft ? center.x - 3.5f : center.x + 3.5f;
    drawBox(shader, VAO, glm::vec3(backX, 2.0f, center.z), glm::vec3(0.8f, 4.0f, 6.0f), glm::vec4(0.4f, 0.25f, 0.15f, 1.0f), 0.0f, 3);
    
    for(int i=-2; i<=2; i+=2) {
        float shelfZ = center.z + i;
        float itemX = backX + (isLeft ? 0.4f : -0.4f);
        drawBox(shader, VAO, glm::vec3(itemX, 1.5f, shelfZ), glm::vec3(0.4f), glm::vec4(0.9f, 0.8f, 0.1f, 1.0f));
        drawBox(shader, VAO, glm::vec3(itemX, 2.5f, shelfZ), glm::vec3(0.4f), glm::vec4(0.1f, 0.8f, 0.9f, 1.0f));
        drawBox(shader, VAO, glm::vec3(itemX, 3.5f, shelfZ), glm::vec3(0.3f, 0.5f, 0.3f), glm::vec4(0.8f, 0.1f, 0.8f, 1.0f));
    }
}

void drawPlanterTree(unsigned int shader, unsigned int VAO, glm::vec3 pos) {
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 0.3f, 0.0f), glm::vec3(1.5f, 0.6f, 1.5f), glm::vec4(0.3f, 0.2f, 0.1f, 1.0f), 0.0f, 3);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.2f, 2.0f, 0.2f), glm::vec4(0.4f, 0.25f, 0.1f, 1.0f), 0.0f, 3);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 2.5f, 0.0f), glm::vec3(1.8f, 1.0f, 1.8f), glm::vec4(0.15f, 0.5f, 0.15f, 1.0f));
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 3.2f, 0.0f), glm::vec3(1.2f, 1.0f, 1.2f), glm::vec4(0.2f, 0.6f, 0.2f, 1.0f));
}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Next-Gen Virtual Mall", NULL, NULL);
    if (window == NULL) {
        std::cout << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float vertices[] = {
        // positions          // normals
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f, -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL); glCompileShader(vertexShader);
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL); glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader); glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader); glDeleteShader(fragmentShader);

    glm::vec3 pointLightPositions[10];
    glm::vec3 pointLightColors[10];
    int lightIdx = 0;
    for (int z = -20; z <= 20; z += 10) {
        pointLightPositions[lightIdx] = glm::vec3(-9.0f, 4.0f, z);
        pointLightColors[lightIdx] = glm::vec3(1.0f, 0.9f, 0.8f); 
        lightIdx++;
        pointLightPositions[lightIdx] = glm::vec3(9.0f, 4.0f, z);
        pointLightColors[lightIdx] = glm::vec3(0.9f, 0.95f, 1.0f); 
        lightIdx++;
    }

    srand((unsigned int)time(NULL));
    
    // Shoppers (Free-roaming - Properly assigned to floors to prevent edge falls)
    for(int i=0; i<15; i++) {
        NPC npc;
        npc.floor = (rand() % 2 == 0) ? 1 : 2; // Assign safely to floor 1 or 2
        float startY = (npc.floor == 1) ? 1.5f : 6.5f;
        // Use the new widened corridors for spawning
        float startX = (npc.floor == 1) ? randFloat(-3.0f, 3.0f) : ((rand()%2==0) ? randFloat(-10.0f, -4.0f) : randFloat(4.0f, 10.0f));

        npc.pos = glm::vec3(startX, startY, randFloat(-25.0f, 25.0f));
        npc.target = npc.pos;
        npc.speed = randFloat(1.5f, 3.0f);
        npc.color = glm::vec4(randFloat(0.1f, 0.7f), randFloat(0.1f, 0.7f), randFloat(0.2f, 0.8f), 1.0f);
        npc.rotY = 0.0f;
        npc.waitTimer = randFloat(0.0f, 2.0f);
        npc.isWalking = true;
        npc.hasBounds = false;
        npcList.push_back(npc);
    }
    
    // Shopkeepers (Bounded behind tables)
    for (int z = -20; z <= 20; z += 10) {
        for(float y : {0.0f, 5.0f}) { 
            NPC skL;
            skL.pos = glm::vec3(-12.0f, y, z);
            skL.target = skL.pos;
            skL.speed = randFloat(1.0f, 2.0f);
            skL.color = glm::vec4(0.8f, 0.4f, 0.1f, 1.0f);
            skL.rotY = 90.0f;
            skL.waitTimer = randFloat(0.0f, 2.0f);
            skL.isWalking = false;
            skL.hasBounds = true; 
            skL.boundsMin = glm::vec3(-12.8f, y, z - 2.0f); 
            skL.boundsMax = glm::vec3(-11.8f, y, z + 2.0f);
            npcList.push_back(skL);

            NPC skR;
            skR.pos = glm::vec3(12.0f, y, z);
            skR.target = skR.pos;
            skR.speed = randFloat(1.0f, 2.0f);
            skR.color = glm::vec4(0.8f, 0.4f, 0.1f, 1.0f);
            skR.rotY = -90.0f;
            skR.waitTimer = randFloat(0.0f, 2.0f);
            skR.isWalking = false;
            skR.hasBounds = true; 
            skR.boundsMin = glm::vec3(11.8f, y, z - 2.0f); 
            skR.boundsMax = glm::vec3(12.8f, y, z + 2.0f);
            npcList.push_back(skR);
        }
    }

    std::cout << "--- NEXT-GEN VIRTUAL MALL ---" << std::endl;
    std::cout << "[WASD] + [SPACE] : Move freely" << std::endl;
    std::cout << "[T] : Cinematic Tour Mode" << std::endl;
    std::cout << "[F] : Flashlight" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.01f, 0.01f, 0.03f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glUniform3f(glGetUniformLocation(shaderProgram, "dirLightDir"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLightColor"), 0.5f, 0.5f, 0.6f);

        for(int i=0; i<10; i++) {
            std::string posStr = "pointLightPositions[" + std::to_string(i) + "]";
            std::string colStr = "pointLightColors[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shaderProgram, posStr.c_str()), 1, glm::value_ptr(pointLightPositions[i]));
            glUniform3fv(glGetUniformLocation(shaderProgram, colStr.c_str()), 1, glm::value_ptr(pointLightColors[i]));
        }

        glUniform3fv(glGetUniformLocation(shaderProgram, "spotLightPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "spotLightDir"), 1, glm::value_ptr(cameraFront));
        glUniform3f(glGetUniformLocation(shaderProgram, "spotLightColor"), 1.0f, 0.95f, 0.8f);
        glUniform1f(glGetUniformLocation(shaderProgram, "spotCutOff"), glm::cos(glm::radians(12.5f)));
        glUniform1f(glGetUniformLocation(shaderProgram, "spotOuterCutOff"), glm::cos(glm::radians(17.5f)));
        glUniform1i(glGetUniformLocation(shaderProgram, "flashlightOn"), flashlightOn);

        // ==========================================
        // 1. DRAW OPAQUE/SOLID OBJECTS FIRST
        // ==========================================
        
        // Ground Floor
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(40.0f, 0.5f, 60.0f), glm::vec4(0.8f, 0.8f, 0.85f, 1.0f), 0.0f, 1);
        
        // 2nd Floor Balconies (Extended inward to x=3.0 to create a spacious 3.0-wide corridor)
        drawBox(shaderProgram, VAO, glm::vec3(-8.5f, 4.75f, 0.0f), glm::vec3(11.0f, 0.5f, 60.0f), glm::vec4(0.65f, 0.65f, 0.7f, 1.0f), 0.0f, 1);
        drawBox(shaderProgram, VAO, glm::vec3( 8.5f, 4.75f, 0.0f), glm::vec3(11.0f, 0.5f, 60.0f), glm::vec4(0.65f, 0.65f, 0.7f, 1.0f), 0.0f, 1);
        
        // Front Landing Bridge (Over Escalator)
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 4.75f, -10.0f), glm::vec3(6.0f, 0.5f, 4.0f), glm::vec4(0.65f, 0.65f, 0.7f, 1.0f), 0.0f, 1);
        // Back Connecting Bridge (Forms the full corridor loop)
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 4.75f, 20.0f), glm::vec3(6.0f, 0.5f, 4.0f), glm::vec4(0.65f, 0.65f, 0.7f, 1.0f), 0.0f, 1);
        
        // Solid End Walls (Cool gray)
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.0f, -30.0f), glm::vec3(40.0f, 10.0f, 1.0f), glm::vec4(0.5f, 0.5f, 0.55f, 1.0f));
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.0f,  30.0f), glm::vec3(40.0f, 10.0f, 1.0f), glm::vec4(0.5f, 0.5f, 0.55f, 1.0f));

        // Ceiling Frame / Skylight Truss
        drawBox(shaderProgram, VAO, glm::vec3(-6.0f, 10.0f, 0.0f), glm::vec3(1.0f, 0.5f, 60.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        drawBox(shaderProgram, VAO, glm::vec3( 6.0f, 10.0f, 0.0f), glm::vec3(1.0f, 0.5f, 60.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        for(int i=-25; i<=25; i+=5) {
            drawBox(shaderProgram, VAO, glm::vec3(0.0f, 10.0f, i), glm::vec3(12.0f, 0.4f, 0.4f), glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
        }

        // Grand Dual Escalator 
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 2.5f, -4.0f), glm::vec3(0.6f, 5.0f, 8.5f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
        for (int i = 0; i < 40; i++) {
            float height = i * 0.125f;
            float zOffset = 0.0f - (i * 0.2f);
            drawBox(shaderProgram, VAO, glm::vec3(-1.5f, height, zOffset), glm::vec3(2.4f, 0.2f, 0.3f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
            drawBox(shaderProgram, VAO, glm::vec3( 1.5f, height, zOffset), glm::vec3(2.4f, 0.2f, 0.3f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
        }
        
        // Draw Shops
        for (int z = -20; z <= 20; z += 10) {
            bool fullyOpen = (z % 20 == 0); 
            
            for(float y : {0.0f, 5.0f}) { 
                // LEFT SHOP (Walls set to soft beige for contrast)
                glm::vec3 lPos(-10.0f, y + 2.5f, z);
                drawBox(shaderProgram, VAO, lPos + glm::vec3(-3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 8.0f), glm::vec4(0.75f, 0.7f, 0.65f, 1.0f)); 
                drawBox(shaderProgram, VAO, lPos + glm::vec3(0.0f, 0.0f, -3.9f), glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.75f, 0.7f, 0.65f, 1.0f)); 
                drawBox(shaderProgram, VAO, lPos + glm::vec3(0.0f, 0.0f, 3.9f),  glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.75f, 0.7f, 0.65f, 1.0f)); 
                
                if (!fullyOpen) {
                    drawBox(shaderProgram, VAO, lPos + glm::vec3(4.0f, 0.0f, -2.5f), glm::vec3(0.2f, 5.0f, 3.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f)); 
                    drawBox(shaderProgram, VAO, lPos + glm::vec3(4.0f, 0.0f,  2.5f), glm::vec3(0.2f, 5.0f, 3.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f)); 
                }
                drawBox(shaderProgram, VAO, lPos + glm::vec3(4.0f, 2.0f,  0.0f), glm::vec3(0.2f, 1.0f, 8.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f)); 
                
                glm::vec4 signColorL = glm::vec4(0.2f + abs(z%3)*0.4f, 0.8f, 0.9f - abs(z%4)*0.2f, 1.0f);
                drawBox(shaderProgram, VAO, glm::vec3(-5.9f, y + 4.5f, z), glm::vec3(0.1f, 0.8f, 4.0f), signColorL, 0.0f, 5);
                drawShopInterior(shaderProgram, VAO, glm::vec3(-10.0f, y, z), true);

                // RIGHT SHOP (Walls set to soft beige)
                glm::vec3 rPos(10.0f, y + 2.5f, z);
                drawBox(shaderProgram, VAO, rPos + glm::vec3(3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 8.0f), glm::vec4(0.75f, 0.7f, 0.65f, 1.0f)); 
                drawBox(shaderProgram, VAO, rPos + glm::vec3(0.0f, 0.0f, -3.9f), glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.75f, 0.7f, 0.65f, 1.0f)); 
                drawBox(shaderProgram, VAO, rPos + glm::vec3(0.0f, 0.0f, 3.9f),  glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.75f, 0.7f, 0.65f, 1.0f)); 
                
                if (!fullyOpen) {
                    drawBox(shaderProgram, VAO, rPos + glm::vec3(-4.0f, 0.0f, -2.5f), glm::vec3(0.2f, 5.0f, 3.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
                    drawBox(shaderProgram, VAO, rPos + glm::vec3(-4.0f, 0.0f,  2.5f), glm::vec3(0.2f, 5.0f, 3.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
                }
                drawBox(shaderProgram, VAO, rPos + glm::vec3(-4.0f, 2.0f,  0.0f), glm::vec3(0.2f, 1.0f, 8.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
                
                glm::vec4 signColorR = glm::vec4(0.9f, 0.3f + abs(z%2)*0.3f, 0.3f, 1.0f);
                drawBox(shaderProgram, VAO, glm::vec3(5.9f, y + 4.5f, z), glm::vec3(0.1f, 0.8f, 4.0f), signColorR, 0.0f, 5);
                drawShopInterior(shaderProgram, VAO, glm::vec3(10.0f, y, z), false);
            }

            // Pillars
            drawBox(shaderProgram, VAO, glm::vec3(-5.8f, 5.0f, z + 4.8f), glm::vec3(0.6f, 10.0f, 0.6f), glm::vec4(0.55f, 0.55f, 0.6f, 1.0f), 0.0f, 1);
            drawBox(shaderProgram, VAO, glm::vec3( 5.8f, 5.0f, z + 4.8f), glm::vec3(0.6f, 10.0f, 0.6f), glm::vec4(0.55f, 0.55f, 0.6f, 1.0f), 0.0f, 1);

            // Ground Floor Centerpieces
            if (z != 0 && z != -10) {
                drawPlanterTree(shaderProgram, VAO, glm::vec3(0.0f, 0.0f, z));
                drawBox(shaderProgram, VAO, glm::vec3(0.0f, 0.4f, z + 2.0f), glm::vec3(2.0f, 0.1f, 0.6f), glm::vec4(0.4f, 0.2f, 0.1f, 1.0f), 0.0f, 3);
                drawBox(shaderProgram, VAO, glm::vec3(0.0f, 0.4f, z - 2.0f), glm::vec3(2.0f, 0.1f, 0.6f), glm::vec4(0.4f, 0.2f, 0.1f, 1.0f), 0.0f, 3);
            }
        }

        // NPCs Update & Draw
        for(auto& npc : npcList) {
            if(npc.waitTimer > 0.0f) {
                npc.waitTimer -= deltaTime;
                npc.isWalking = false;
            } else {
                npc.isWalking = true;
                glm::vec3 dir = npc.target - npc.pos;
                dir.y = 0.0f;
                float dist = glm::length(dir);
                
                if(dist < 0.5f) {
                    if (npc.hasBounds) {
                        npc.target = glm::vec3(randFloat(npc.boundsMin.x, npc.boundsMax.x), npc.pos.y, randFloat(npc.boundsMin.z, npc.boundsMax.z));
                    } else {
                        if (npc.floor == 1) {
                            npc.target = glm::vec3(randFloat(-4.0f, 4.0f), 1.5f, randFloat(-25.0f, 25.0f));
                            if (rand() % 4 == 0) { 
                                float signX = (rand()%2 == 0) ? -8.0f : 8.0f;
                                float shopZs[] = {-20, -10, 0, 10, 20};
                                npc.target = glm::vec3(signX, 1.5f, shopZs[rand()%5]);
                            }
                        } else {
                            bool isLeft = (npc.pos.x < 0.0f);
                            float tX = isLeft ? randFloat(-10.0f, -4.0f) : randFloat(4.0f, 10.0f);
                            npc.target = glm::vec3(tX, 6.5f, randFloat(-25.0f, 25.0f));
                            if (rand() % 4 == 0) {
                                float sX = isLeft ? -8.0f : 8.0f;
                                float shopZs[] = {-20, -10, 0, 10, 20};
                                npc.target = glm::vec3(sX, 6.5f, shopZs[rand()%5]);
                            }
                        }
                    }
                    npc.waitTimer = randFloat(1.0f, 3.0f);
                } else {
                    dir = glm::normalize(dir);
                    float dx = dir.x * npc.speed * deltaTime;
                    float dz = dir.z * npc.speed * deltaTime;
                    
                    if (isPositionValid(npc.pos.x + dx, npc.pos.y, npc.pos.z + dz)) {
                        npc.pos.x += dx;
                        npc.pos.z += dz;
                    } else {
                        npc.waitTimer = randFloat(0.5f, 1.5f); // Hit a wall, wait, then recalculate below
                        if (npc.hasBounds) {
                            npc.target = glm::vec3(randFloat(npc.boundsMin.x, npc.boundsMax.x), npc.pos.y, randFloat(npc.boundsMin.z, npc.boundsMax.z));
                        } else {
                            if (npc.floor == 1) {
                                npc.target = glm::vec3(randFloat(-4.0f, 4.0f), 1.5f, randFloat(-25.0f, 25.0f));
                            } else {
                                bool isLeft = (npc.pos.x < 0.0f);
                                float tX = isLeft ? randFloat(-10.0f, -4.0f) : randFloat(4.0f, 10.0f);
                                npc.target = glm::vec3(tX, 6.5f, randFloat(-25.0f, 25.0f));
                            }
                        }
                    }
                    npc.rotY = glm::degrees(atan2(dir.x, dir.z));
                }
            }
            
            float renderY = npc.pos.y;
            if (!npc.hasBounds) {
                renderY = getFloorHeight(npc.pos.x, npc.pos.y, npc.pos.z) - 1.5f; 
            }
            drawMan(shaderProgram, VAO, glm::vec3(npc.pos.x, renderY, npc.pos.z), npc.rotY, npc.color, npc.isWalking, currentFrame);
        }

        // ==========================================
        // 2. DRAW TRANSPARENT OBJECTS LAST (Crucial for Alpha Blending)
        // ==========================================
        
        // Storefront Glass Panels 
        for (int z = -20; z <= 20; z += 10) {
            bool fullyOpen = (z % 20 == 0); 
            if (!fullyOpen) {
                for(float y : {0.0f, 5.0f}) { 
                    // Left glass
                    drawBox(shaderProgram, VAO, glm::vec3(-6.0f, y + 2.0f, z - 2.5f), glm::vec3(0.05f, 4.0f, 2.5f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                    drawBox(shaderProgram, VAO, glm::vec3(-6.0f, y + 2.0f, z + 2.5f), glm::vec3(0.05f, 4.0f, 2.5f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                    // Right glass
                    drawBox(shaderProgram, VAO, glm::vec3( 6.0f, y + 2.0f, z - 2.5f), glm::vec3(0.05f, 4.0f, 2.5f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                    drawBox(shaderProgram, VAO, glm::vec3( 6.0f, y + 2.0f, z + 2.5f), glm::vec3(0.05f, 4.0f, 2.5f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);
                }
            }
        }

        // Glass Railings for 2nd Floor Balcony - Segments to allow for the bridges!
        // Left Side 
        drawBox(shaderProgram, VAO, glm::vec3(-3.1f, 5.5f, -21.0f), glm::vec3(0.05f, 1.5f, 18.0f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(-3.1f, 5.5f,  5.0f), glm::vec3(0.05f, 1.5f, 26.0f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3(-3.1f, 5.5f,  26.0f), glm::vec3(0.05f, 1.5f, 8.0f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
        // Right Side 
        drawBox(shaderProgram, VAO, glm::vec3( 3.1f, 5.5f, -21.0f), glm::vec3(0.05f, 1.5f, 18.0f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3( 3.1f, 5.5f,  5.0f), glm::vec3(0.05f, 1.5f, 26.0f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3( 3.1f, 5.5f,  26.0f), glm::vec3(0.05f, 1.5f, 8.0f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
        
        // Front Landing Bridge Railings
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.5f, -11.9f), glm::vec3(6.0f, 1.5f, 0.05f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2); // Back edge

        // Back Bridge Railings
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.5f, 21.9f), glm::vec3(6.0f, 1.5f, 0.05f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2); // Back edge
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.5f, 18.1f), glm::vec3(6.0f, 1.5f, 0.05f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2); // Front edge

        // Escalator Glass Sides
        drawBox(shaderProgram, VAO, glm::vec3(-2.8f, 3.5f, -4.0f), glm::vec3(0.1f, 7.0f, 8.5f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2);
        drawBox(shaderProgram, VAO, glm::vec3( 2.8f, 3.5f, -4.0f), glm::vec3(0.1f, 7.0f, 8.5f), glm::vec4(0.7f, 0.9f, 1.0f, 0.3f), 0.0f, 2);

        // Huge Glass Skylight Ceiling
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 10.2f, 0.0f), glm::vec3(12.0f, 0.1f, 60.0f), glm::vec4(0.6f, 0.8f, 1.0f, 0.3f), 0.0f, 2);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}