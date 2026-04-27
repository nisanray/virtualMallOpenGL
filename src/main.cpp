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
};
std::vector<NPC> npcList;

float randFloat(float min, float max) {
    return min + (max - min) * (rand() / (float)RAND_MAX);
}

// ---------------------------------------------------------
// Automated Cinematic Tour System
// ---------------------------------------------------------
bool tourMode = false; // Set to false so you can walk manually by default!
bool tKeyPressed = false;
float tourProgress = 0.0f;
int currentWaypoint = 0;

struct Waypoint {
    glm::vec3 pos;
    glm::vec3 lookAt;
};

// Define the cinematic path through the mall
std::vector<Waypoint> tourPoints = {
    {{0.0f, 1.5f, 28.0f}, {0.0f, 1.5f, 10.0f}},    // 0: Entrance, looking in
    {{0.0f, 1.5f, 10.0f}, {-10.0f, 3.0f, 10.0f}},  // 1: Center, pan left to shop
    {{0.0f, 1.5f, 0.0f},  {10.0f, 3.0f, 0.0f}},    // 2: Center, pan right to shop
    {{0.0f, 1.5f, -2.0f}, {0.0f, 6.5f, -10.0f}},   // 3: Base of stairs, looking up
    {{0.0f, 6.5f, -8.0f}, {-6.0f, 6.5f, -8.0f}},   // 4: Top of stairs, looking left
    {{-6.0f, 6.5f, -8.0f}, {-6.0f, 6.5f, 0.0f}},   // 5: Move onto left balcony
    {{-6.0f, 6.5f, 10.0f}, {0.0f, 1.5f, 5.0f}},    // 6: Left balcony, looking down at central planters
    {{-6.0f, 6.5f, 20.0f}, {0.0f, 10.0f, 10.0f}},  // 7: Left balcony end, looking up at skylight
    {{-6.0f, 6.5f, -8.0f}, {6.0f, 6.5f, -8.0f}},   // 8: Back to stairs top, looking across to right side
    {{6.0f, 6.5f, -8.0f}, {6.0f, 6.5f, 0.0f}},     // 9: Move onto right balcony
    {{6.0f, 6.5f, 10.0f}, {0.0f, 1.5f, 5.0f}},     // 10: Right balcony, looking down
    {{6.0f, 6.5f, -8.0f}, {0.0f, 6.5f, -8.0f}},    // 11: Back to stairs
    {{0.0f, 6.5f, -8.0f}, {0.0f, 1.5f, 0.0f}},     // 12: Top of stairs looking down at ground floor
    {{0.0f, 1.5f, -2.0f}, {0.0f, 1.5f, 25.0f}}     // 13: Base of stairs looking back at entrance
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
// Advanced Shaders (Multi-Lighting & Alpha Blending)
// ---------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
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

uniform vec3 viewPos;
uniform vec4 objectColor; // xyzw (rgba)

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

vec3 CalcDirLight(vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-dirLightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 ambient  = 0.4 * dirLightColor; // Boosted ambient for a brighter mall
    vec3 diffuse  = diff * dirLightColor;
    vec3 specular = spec * dirLightColor * 0.5;
    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(int i, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(pointLightPositions[i] - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    float distance = length(pointLightPositions[i] - fragPos);
    float attenuation = 1.0 / (1.0 + 0.14 * distance + 0.07 * (distance * distance));    
    
    vec3 ambient  = 0.1 * pointLightColors[i];
    vec3 diffuse  = diff * pointLightColors[i];
    vec3 specular = spec * pointLightColors[i];
    return (ambient + diffuse + specular) * attenuation;
}

vec3 CalcSpotLight(vec3 normal, vec3 fragPos, vec3 viewDir) {
    if(!flashlightOn) return vec3(0.0);
    vec3 lightDir = normalize(spotLightPos - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    float distance = length(spotLightPos - fragPos);
    float attenuation = 1.0 / (1.0 + 0.045 * distance + 0.0075 * (distance * distance));    
    
    float theta = dot(lightDir, normalize(-spotLightDir)); 
    float epsilon = spotCutOff - spotOuterCutOff;
    float intensity = clamp((theta - spotOuterCutOff) / epsilon, 0.0, 1.0);
    
    vec3 ambient = 0.1 * spotLightColor;
    vec3 diffuse = diff * spotLightColor;
    vec3 specular = spec * spotLightColor;
    return (ambient + diffuse + specular) * attenuation * intensity;
}

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    vec3 result = CalcDirLight(norm, viewDir);
    for(int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(i, norm, FragPos, viewDir);    
    result += CalcSpotLight(norm, FragPos, viewDir);
    
    FragColor = vec4(result * objectColor.rgb, objectColor.a);
}
)";

// ---------------------------------------------------------
// Callbacks
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
// Simple Collision & Physics
// ---------------------------------------------------------
bool isPositionValid(float x, float z) {
    // Main Corridor
    if (x > -5.8f && x < 5.8f && z > -28.0f && z < 28.0f) return true;
    
    // Open Shops Interior
    for (int sz = -20; sz <= 20; sz += 10) {
        // Left Shop Area
        if (x >= -14.0f && x <= -5.8f && z > sz - 3.8f && z < sz + 3.8f) {
            // Exclude Central Table (Center: -10, Size: 3x2)
            if (x > -11.5f && x < -8.5f && z > sz - 1.0f && z < sz + 1.0f) return false;
            // Exclude Back Shelves (x < -13.2)
            if (x < -13.2f) return false;
            return true; 
        }
        // Right Shop Area
        if (x >= 5.8f && x <= 14.0f && z > sz - 3.8f && z < sz + 3.8f) {
            // Exclude Central Table (Center: 10, Size: 3x2)
            if (x > 8.5f && x < 11.5f && z > sz - 1.0f && z < sz + 1.0f) return false;
            // Exclude Back Shelves (x > 13.2)
            if (x > 13.2f) return false;
            return true;
        }
    }
    return false;
}

float getFloorHeight(float x, float z) {
    if (x > -2.0f && x < 2.0f && z > -2.0f && z < 6.0f) {
        float normalizedZ = (6.0f - z) / 8.0f; 
        return 1.5f + (normalizedZ * 5.0f); 
    }
    if (cameraPos.y > 4.0f) {
        if (x < -3.0f || x > 3.0f || z < -25.0f || z > 25.0f) return 6.5f; 
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
    } else {
        tKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!fKeyPressed) { flashlightOn = !flashlightOn; fKeyPressed = true; }
    } else {
        fKeyPressed = false;
    }

    if (tourMode) {
        updateTour(deltaTime);
        return; 
    }

    // --- Manual First Person Controls ---
    float cameraSpeed = 5.0f * deltaTime;
    
    float moveX = 0.0f, moveZ = 0.0f;
    glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 flatRight = glm::normalize(glm::cross(flatFront, cameraUp));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { moveX += flatFront.x; moveZ += flatFront.z; }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { moveX -= flatFront.x; moveZ -= flatFront.z; }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { moveX -= flatRight.x; moveZ -= flatRight.z; }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { moveX += flatRight.x; moveZ += flatRight.z; }

    // Sliding Collision
    if (moveX != 0.0f || moveZ != 0.0f) {
        glm::vec3 moveDir = glm::normalize(glm::vec3(moveX, 0.0f, moveZ));
        float dx = moveDir.x * cameraSpeed;
        float dz = moveDir.z * cameraSpeed;
        
        if (isPositionValid(cameraPos.x + dx, cameraPos.z)) cameraPos.x += dx;
        if (isPositionValid(cameraPos.x, cameraPos.z + dz)) cameraPos.z += dz;
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded) {
        velocityY = jumpForce;
        isGrounded = false;
    }

    velocityY -= gravity * deltaTime;
    cameraPos.y += velocityY * deltaTime;

    float currentFloorHeight = getFloorHeight(cameraPos.x, cameraPos.z);
    if (cameraPos.y <= currentFloorHeight) {
        cameraPos.y = currentFloorHeight;
        velocityY = 0.0f;
        isGrounded = true;
    }
}

// ---------------------------------------------------------
// Drawing Utilities
// ---------------------------------------------------------
void drawBox(unsigned int shader, unsigned int VAO, glm::vec3 position, glm::vec3 scale, glm::vec4 color, float rotY = 0.0f) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    if (rotY != 0.0f) {
        model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    }
    model = glm::scale(model, scale);
    
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(color));
    
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

// Draws an animated character (a "man")
void drawMan(unsigned int shader, unsigned int VAO, glm::vec3 pos, float rotY, glm::vec4 shirtColor, bool isWalking, float time) {
    float legSwing = isWalking ? sin(time * 10.0f) * 30.0f : 0.0f;
    
    // Left leg
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos + glm::vec3(-0.15f, 0.5f, 0.0f));
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0f)); 
    model = glm::rotate(model, glm::radians(legSwing), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::translate(model, glm::vec3(0.0f, -0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(0.2f, 1.0f, 0.2f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(glm::vec4(0.1f, 0.1f, 0.15f, 1.0f)));
    glBindVertexArray(VAO); glDrawArrays(GL_TRIANGLES, 0, 36);
    
    // Right leg
    model = glm::mat4(1.0f);
    model = glm::translate(model, pos + glm::vec3(0.15f, 0.5f, 0.0f));
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0f)); 
    model = glm::rotate(model, glm::radians(-legSwing), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::translate(model, glm::vec3(0.0f, -0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(0.2f, 1.0f, 0.2f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(glm::vec4(0.1f, 0.1f, 0.15f, 1.0f)));
    glBindVertexArray(VAO); glDrawArrays(GL_TRIANGLES, 0, 36);

    // Torso & Head
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.6f, 1.0f, 0.3f), shirtColor, rotY);
    drawBox(shader, VAO, pos + glm::vec3(0.0f, 2.2f, 0.0f), glm::vec3(0.35f, 0.4f, 0.35f), glm::vec4(0.9f, 0.7f, 0.6f, 1.0f), rotY);
}

void drawShopInterior(unsigned int shader, unsigned int VAO, glm::vec3 center, bool isLeft) {
    // Central display table
    drawBox(shader, VAO, center + glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(3.0f, 0.8f, 2.0f), glm::vec4(0.6f, 0.4f, 0.3f, 1.0f));
    
    // Featured Products on table
    drawBox(shader, VAO, center + glm::vec3(-1.0f, 0.9f, 0.0f), glm::vec3(0.4f), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
    drawBox(shader, VAO, center + glm::vec3( 0.0f, 0.9f, 0.0f), glm::vec3(0.3f, 0.6f, 0.3f), glm::vec4(0.2f, 1.0f, 0.2f, 1.0f)); // Green
    drawBox(shader, VAO, center + glm::vec3( 1.0f, 0.9f, 0.0f), glm::vec3(0.5f, 0.2f, 0.5f), glm::vec4(0.2f, 0.2f, 1.0f, 1.0f)); // Blue
    
    // Back wall shelves and items
    float backX = isLeft ? center.x - 3.5f : center.x + 3.5f;
    drawBox(shader, VAO, glm::vec3(backX, 2.0f, center.z), glm::vec3(0.6f, 4.0f, 6.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
    for(int i=-2; i<=2; i+=2) {
        float shelfZ = center.z + i;
        drawBox(shader, VAO, glm::vec3(backX + (isLeft?0.4f:-0.4f), 1.5f, shelfZ), glm::vec3(0.4f), glm::vec4(0.9f, 0.9f, 0.1f, 1.0f));
        drawBox(shader, VAO, glm::vec3(backX + (isLeft?0.4f:-0.4f), 2.5f, shelfZ), glm::vec3(0.4f), glm::vec4(0.1f, 0.9f, 0.9f, 1.0f));
    }
}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Multi-Level Virtual Mall", NULL, NULL);
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

    // Setup Shop Lights Data - Improved Natural Lighting
    glm::vec3 pointLightPositions[10];
    glm::vec3 pointLightColors[10];
    int lightIdx = 0;
    for (int z = -20; z <= 20; z += 10) {
        pointLightPositions[lightIdx] = glm::vec3(-9.0f, 4.0f, z);
        pointLightColors[lightIdx] = glm::vec3(0.9f, 0.85f, 0.8f); // Warm white light
        lightIdx++;
        pointLightPositions[lightIdx] = glm::vec3(9.0f, 4.0f, z);
        pointLightColors[lightIdx] = glm::vec3(0.85f, 0.9f, 1.0f); // Cool white light
        lightIdx++;
    }

    // Initialize NPCs
    srand((unsigned int)time(NULL));
    
    // Create random wandering shoppers
    for(int i=0; i<15; i++) {
        NPC npc;
        npc.pos = glm::vec3(randFloat(-4.0f, 4.0f), 0.0f, randFloat(-25.0f, 25.0f));
        npc.target = glm::vec3(randFloat(-4.0f, 4.0f), 0.0f, randFloat(-25.0f, 25.0f));
        npc.speed = randFloat(1.5f, 3.5f);
        npc.color = glm::vec4(randFloat(0.2f, 0.9f), randFloat(0.2f, 0.9f), randFloat(0.2f, 0.9f), 1.0f);
        npc.rotY = 0.0f;
        npc.waitTimer = randFloat(0.0f, 2.0f);
        npc.isWalking = true;
        npc.hasBounds = false; // Free roam
        npcList.push_back(npc);
    }

    // Create bounded Shopkeepers pacing in their specific shops
    for (int z = -20; z <= 20; z += 10) {
        for (float y : {0.0f, 5.0f}) { // Floor 1 and Floor 2
            // Left Shopkeeper
            NPC skL;
            skL.pos = glm::vec3(-12.0f, y, z); // Start correctly behind table
            skL.target = skL.pos;
            skL.speed = randFloat(1.0f, 2.0f); // Slower, relaxed walk inside store
            skL.color = glm::vec4(0.8f, 0.4f, 0.1f, 1.0f); // Bright orange uniform
            skL.rotY = 90.0f;
            skL.waitTimer = randFloat(0.0f, 2.0f);
            skL.isWalking = false;
            skL.hasBounds = true; 
            // Restrict bounds strictly behind the table aisle
            skL.boundsMin = glm::vec3(-13.0f, y, z - 2.5f); 
            skL.boundsMax = glm::vec3(-11.6f, y, z + 2.5f);
            npcList.push_back(skL);

            // Right Shopkeeper
            NPC skR;
            skR.pos = glm::vec3(12.0f, y, z); // Start correctly behind table
            skR.target = skR.pos;
            skR.speed = randFloat(1.0f, 2.0f);
            skR.color = glm::vec4(0.8f, 0.4f, 0.1f, 1.0f); // Bright orange uniform
            skR.rotY = -90.0f;
            skR.waitTimer = randFloat(0.0f, 2.0f);
            skR.isWalking = false;
            skR.hasBounds = true; 
            // Restrict bounds strictly behind the table aisle
            skR.boundsMin = glm::vec3(11.6f, y, z - 2.5f); 
            skR.boundsMax = glm::vec3(13.0f, y, z + 2.5f);
            npcList.push_back(skR);
        }
    }

    std::cout << "--- VIRTUAL MALL STARTED ---" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "[WASD] + [SPACE] : Move around freely" << std::endl;
    std::cout << "[T] : Toggle Cinematic Tour Mode" << std::endl;
    std::cout << "[F] : Toggle Flashlight" << std::endl;

    // Rendering Loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.02f, 0.02f, 0.05f, 1.0f); // Night sky out the skylight
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Update Camera Uniforms
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));

        // Uniforms: Directional Light - Set to a brighter natural ambient color
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLightDir"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLightColor"), 0.45f, 0.45f, 0.5f);

        // Uniforms: Point Lights (Shops)
        for(int i=0; i<10; i++) {
            std::string posStr = "pointLightPositions[" + std::to_string(i) + "]";
            std::string colStr = "pointLightColors[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shaderProgram, posStr.c_str()), 1, glm::value_ptr(pointLightPositions[i]));
            glUniform3fv(glGetUniformLocation(shaderProgram, colStr.c_str()), 1, glm::value_ptr(pointLightColors[i]));
        }

        // Uniforms: Spot Light (Flashlight)
        glUniform3fv(glGetUniformLocation(shaderProgram, "spotLightPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "spotLightDir"), 1, glm::value_ptr(cameraFront));
        glUniform3f(glGetUniformLocation(shaderProgram, "spotLightColor"), 1.0f, 0.95f, 0.8f);
        glUniform1f(glGetUniformLocation(shaderProgram, "spotCutOff"), glm::cos(glm::radians(12.5f)));
        glUniform1f(glGetUniformLocation(shaderProgram, "spotOuterCutOff"), glm::cos(glm::radians(17.5f)));
        glUniform1i(glGetUniformLocation(shaderProgram, "flashlightOn"), flashlightOn);

        // --- DRAW SOLID OBJECTS FIRST ---
        
        // Ground Floor
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(40.0f, 0.5f, 60.0f), glm::vec4(0.2f, 0.2f, 0.25f, 1.0f));
        // 2nd Floor Balconies (Left and Right)
        drawBox(shaderProgram, VAO, glm::vec3(-9.0f, 4.75f, 0.0f), glm::vec3(18.0f, 0.5f, 60.0f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
        drawBox(shaderProgram, VAO, glm::vec3( 9.0f, 4.75f, 0.0f), glm::vec3(18.0f, 0.5f, 60.0f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
        
        // Ceiling Frame (Around skylight)
        drawBox(shaderProgram, VAO, glm::vec3(-15.0f, 10.25f, 0.0f), glm::vec3(10.0f, 0.5f, 60.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        drawBox(shaderProgram, VAO, glm::vec3( 15.0f, 10.25f, 0.0f), glm::vec3(10.0f, 0.5f, 60.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        
        // Solid Walls (Back and Front)
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.0f, -30.0f), glm::vec3(40.0f, 10.0f, 1.0f), glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 5.0f,  30.0f), glm::vec3(40.0f, 10.0f, 1.0f), glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

        // Central Grand Staircase
        for (int i = 0; i < 20; i++) {
            float height = i * 0.25f;
            float depthOffset = 6.0f - (i * 0.4f);
            drawBox(shaderProgram, VAO, glm::vec3(0.0f, height/2.0f, depthOffset), glm::vec3(4.0f, height, 0.4f), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
        }

        // Update NPC positions
        for(auto& npc : npcList) {
            if(npc.waitTimer > 0.0f) {
                npc.waitTimer -= deltaTime;
                npc.isWalking = false;
            } else {
                npc.isWalking = true;
                glm::vec3 dir = npc.target - npc.pos;
                dir.y = 0.0f; // ignore Y axis when finding distance
                float dist = glm::length(dir);
                if(dist < 0.5f) {
                    if (npc.hasBounds) {
                        // Shopkeeper pathing (bounded)
                        npc.target = glm::vec3(
                            randFloat(npc.boundsMin.x, npc.boundsMax.x), 
                            npc.pos.y, 
                            randFloat(npc.boundsMin.z, npc.boundsMax.z)
                        );
                    } else {
                        // Free roaming shopper pathing
                        npc.target = glm::vec3(randFloat(-4.0f, 4.0f), 0.0f, randFloat(-25.0f, 25.0f));
                        if (rand() % 4 == 0) { // Sometimes enter a shop!
                            float signX = (rand()%2 == 0) ? -10.0f : 10.0f;
                            float shopZs[] = {-20, -10, 0, 10, 20};
                            npc.target = glm::vec3(signX, 0.0f, shopZs[rand()%5]);
                        }
                    }
                    npc.waitTimer = randFloat(1.0f, 4.0f);
                } else {
                    dir = glm::normalize(dir);
                    float dx = dir.x * npc.speed * deltaTime;
                    float dz = dir.z * npc.speed * deltaTime;
                    
                    // Stop sliding and check if the direct path is completely clear
                    if (isPositionValid(npc.pos.x + dx, npc.pos.z + dz)) {
                        npc.pos.x += dx;
                        npc.pos.z += dz;
                    } else {
                        // Bumping into a wall - abort and find a new place to go!
                        if (npc.hasBounds) {
                            npc.target = glm::vec3(
                                randFloat(npc.boundsMin.x, npc.boundsMax.x), 
                                npc.pos.y, 
                                randFloat(npc.boundsMin.z, npc.boundsMax.z)
                            );
                        } else {
                            npc.target = glm::vec3(randFloat(-4.0f, 4.0f), 0.0f, randFloat(-25.0f, 25.0f));
                            if (rand() % 4 == 0) { 
                                float signX = (rand()%2 == 0) ? -10.0f : 10.0f;
                                float shopZs[] = {-20, -10, 0, 10, 20};
                                npc.target = glm::vec3(signX, 0.0f, shopZs[rand()%5]);
                            }
                        }
                        npc.waitTimer = randFloat(0.5f, 1.5f);
                    }

                    npc.rotY = glm::degrees(atan2(dir.x, dir.z));
                }
            }
        }

        // Draw Shops, Pillars, and Display Products
        for (int z = -20; z <= 20; z += 10) {
            // LEFT SHOPS (Open layout)
            glm::vec3 lPos(-10.0f, 2.5f, z);
            drawBox(shaderProgram, VAO, lPos + glm::vec3(-3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 8.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Back Wall
            drawBox(shaderProgram, VAO, lPos + glm::vec3(0.0f, 0.0f, -3.9f), glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Side Wall
            drawBox(shaderProgram, VAO, lPos + glm::vec3(0.0f, 0.0f, 3.9f),  glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Side Wall
            drawShopInterior(shaderProgram, VAO, glm::vec3(-10.0f, 0.0f, z), true); // Floor 1 Interior

            glm::vec3 lPos2(-10.0f, 7.5f, z);
            drawBox(shaderProgram, VAO, lPos2 + glm::vec3(-3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 8.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Back Wall 2
            drawBox(shaderProgram, VAO, lPos2 + glm::vec3(0.0f, 0.0f, -3.9f), glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Side Wall 2
            drawBox(shaderProgram, VAO, lPos2 + glm::vec3(0.0f, 0.0f, 3.9f),  glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Side Wall 2
            drawShopInterior(shaderProgram, VAO, glm::vec3(-10.0f, 5.0f, z), true); // Floor 2 Interior
            
            // Left Store Signs (Above the open entrance)
            glm::vec4 signColorL = glm::vec4(abs(z%3)*0.4f, 0.3f, 0.8f - abs(z%4)*0.1f, 1.0f);
            drawBox(shaderProgram, VAO, glm::vec3(-6.1f, 4.5f, z), glm::vec3(0.2f, 1.0f, 8.0f), signColorL);

            // RIGHT SHOPS (Open layout)
            glm::vec3 rPos(10.0f, 2.5f, z);
            drawBox(shaderProgram, VAO, rPos + glm::vec3(3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 8.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Back Wall
            drawBox(shaderProgram, VAO, rPos + glm::vec3(0.0f, 0.0f, -3.9f), glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Side Wall
            drawBox(shaderProgram, VAO, rPos + glm::vec3(0.0f, 0.0f, 3.9f),  glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Side Wall
            drawShopInterior(shaderProgram, VAO, glm::vec3(10.0f, 0.0f, z), false); // Floor 1 Interior
            
            glm::vec3 rPos2(10.0f, 7.5f, z);
            drawBox(shaderProgram, VAO, rPos2 + glm::vec3(3.9f, 0.0f, 0.0f), glm::vec3(0.2f, 5.0f, 8.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Back Wall 2
            drawBox(shaderProgram, VAO, rPos2 + glm::vec3(0.0f, 0.0f, -3.9f), glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Side Wall 2
            drawBox(shaderProgram, VAO, rPos2 + glm::vec3(0.0f, 0.0f, 3.9f),  glm::vec3(8.0f, 5.0f, 0.2f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // Side Wall 2
            drawShopInterior(shaderProgram, VAO, glm::vec3(10.0f, 5.0f, z), false); // Floor 2 Interior

            // Right Store Signs
            glm::vec4 signColorR = glm::vec4(0.8f - abs(z%2)*0.2f, abs(z%3)*0.4f, 0.3f, 1.0f);
            drawBox(shaderProgram, VAO, glm::vec3(6.1f, 4.5f, z), glm::vec3(0.2f, 1.0f, 8.0f), signColorR);
            
            // Grand Pillars holding the skylight
            drawBox(shaderProgram, VAO, glm::vec3(-4.0f, 5.0f, z + 5.0f), glm::vec3(1.0f, 10.0f, 1.0f), glm::vec4(0.9f, 0.9f, 0.95f, 1.0f));
            drawBox(shaderProgram, VAO, glm::vec3( 4.0f, 5.0f, z + 5.0f), glm::vec3(1.0f, 10.0f, 1.0f), glm::vec4(0.9f, 0.9f, 0.95f, 1.0f));

            // Ground Floor Planters
            if (z != 0) {
                drawBox(shaderProgram, VAO, glm::vec3(0.0f, 0.3f, z), glm::vec3(2.5f, 0.6f, 1.5f), glm::vec4(0.4f, 0.3f, 0.2f, 1.0f)); // Wood box
                drawBox(shaderProgram, VAO, glm::vec3(0.0f, 1.0f, z), glm::vec3(2.0f, 1.0f, 1.0f), glm::vec4(0.2f, 0.6f, 0.2f, 1.0f)); // Leaves
            }
        }

        // --- DRAW NPCs ---
        for(auto& npc : npcList) {
            float renderY = npc.pos.y; 
            // Allow stairs to affect render height only for free-roaming mall walkers
            if (!npc.hasBounds) {
                renderY = getFloorHeight(npc.pos.x, npc.pos.z) - 1.5f; 
            }
            drawMan(shaderProgram, VAO, glm::vec3(npc.pos.x, renderY, npc.pos.z), npc.rotY, npc.color, npc.isWalking, currentFrame);
        }

        // --- DRAW TRANSPARENT OBJECTS LAST ---
        // (Render order matters for blending: Back to Front is ideal)

        // Glass Skylight Ceiling
        drawBox(shaderProgram, VAO, glm::vec3(0.0f, 10.25f, 0.0f), glm::vec3(20.0f, 0.2f, 60.0f), glm::vec4(0.6f, 0.8f, 0.9f, 0.3f));
        
        // Glass Railings for 2nd Floor Balcony
        drawBox(shaderProgram, VAO, glm::vec3(-4.1f, 5.5f, 0.0f), glm::vec3(0.2f, 1.5f, 60.0f), glm::vec4(0.7f, 0.9f, 1.0f, 0.4f));
        drawBox(shaderProgram, VAO, glm::vec3( 4.1f, 5.5f, 0.0f), glm::vec3(0.2f, 1.5f, 60.0f), glm::vec4(0.7f, 0.9f, 1.0f, 0.4f));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}