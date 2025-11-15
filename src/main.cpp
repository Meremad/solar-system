// src/main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <map>

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"

const unsigned int SCR_W = 1280;
const unsigned int SCR_H = 720;

// --- file helpers ---
std::string tryPrefixes(const std::string &rel) {
    const std::vector<std::string> prefixes = { "", "../", "./", "../../", "../../../" };
    for (const auto &p : prefixes) {
        std::string full = p + rel;
        std::ifstream f(full, std::ios::binary);
        if (f.is_open()) { f.close(); std::cout << "Found: " << full << std::endl; return full; }
    }
    return rel;
}

std::string readFile(const std::string &rel) {
    std::string path = tryPrefixes(rel);
    std::ifstream in(path);
    if (!in.is_open()) { std::cerr << "Cannot open file: " << path << std::endl; return std::string(); }
    std::stringstream ss; ss << in.rdbuf(); return ss.str();
}

// Format simulated time function
std::string formatSimulatedTime(float simulatedTimeDays) {
    int totalDays = static_cast<int>(simulatedTimeDays);
    int years = totalDays / 365;
    int weeks = (totalDays % 365) / 7;
    int days = (totalDays % 365) % 7;

    std::string res;
    if (years > 0) {
        res += std::to_string(years) + (years == 1 ? " year " : " years ");
    }
    if (weeks > 0) {
        res += std::to_string(weeks) + (weeks == 1 ? " week " : " weeks ");
    }
    res += std::to_string(days) + (days == 1 ? " day" : " days");

    return res;
}

// --- shader utils ---
GLuint compileShaderSrc(const char* src, GLenum type, const char* name) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(sh, 2048, NULL, log);
        std::cerr << "Shader compile error (" << name << "):\n" << log << std::endl;
    } else std::cout << "Compiled: " << name << std::endl;
    return sh;
}

GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetProgramInfoLog(p, 2048, NULL, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    } else std::cout << "Linked program\n";
    return p;
}

// --- sphere generator ---
void createSphere(float radius, unsigned int sectorCount, unsigned int stackCount,
                  std::vector<float>& vertices, std::vector<unsigned int>& indices)
{
    vertices.clear(); indices.clear();
    const float PI = acos(-1.0f);
    for (unsigned int i = 0; i <= stackCount; ++i) {

        float stackAngle = -PI/2 + (float)i * PI / stackCount; 
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);
        for (unsigned int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = (float)j * 2.0f * PI / sectorCount;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            glm::vec3 n = glm::normalize(glm::vec3(x, y, z));
            vertices.push_back(n.x); vertices.push_back(n.y); vertices.push_back(n.z);
            float s = (float)j / sectorCount;
            float t = (float)i / stackCount;
            vertices.push_back(s); vertices.push_back(t);
        }
    }
    for (unsigned int i = 0; i < stackCount; ++i) {
        unsigned int k1 = i * (sectorCount + 1);
        unsigned int k2 = k1 + sectorCount + 1;
        for (unsigned int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1); indices.push_back(k2); indices.push_back(k1+1);
            }
            if (i != (stackCount - 1)) {
                indices.push_back(k1+1); indices.push_back(k2); indices.push_back(k2+1);
            }
        }
    }
}

// --- orbit line generator ---
void createOrbitLine(float radius, int segments, std::vector<float> &vertices) {
    vertices.clear();
    const float PI = acos(-1.0f);
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / segments * 2.0f * PI;
        float x = cosf(theta) * radius;
        float z = sinf(theta) * radius;
        vertices.push_back(x);
        vertices.push_back(0.0f);
        vertices.push_back(z);
    }
}

// --- texture loader ---
GLuint loadTextureTry(const std::string &relPath) {
    std::string path = tryPrefixes(relPath);
    int w,h,comp;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 0);
    if (!data) { std::cerr << "Failed to load texture: " << path << std::endl; return 0; }
    GLenum fmt = (comp == 4) ? GL_RGBA : GL_RGB;
    GLuint tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(data);
    std::cout << "Loaded texture: " << path << " (" << w << "x" << h << ")\n";
    return tex;
}

GLuint loadCubemapFaces(const std::vector<std::string>& facePaths) {
    GLuint texID; glGenTextures(1, &texID); glBindTexture(GL_TEXTURE_CUBE_MAP, texID);
    stbi_set_flip_vertically_on_load(false);
    for (unsigned int i = 0; i < facePaths.size(); ++i) {
        std::string p = tryPrefixes(facePaths[i]);
        int w,h,comp;
        unsigned char* data = stbi_load(p.c_str(), &w, &h, &comp, 0);
        if (!data) { std::cerr << "Failed to load cubemap face: " << p << std::endl; continue; }
        GLenum fmt = (comp == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        std::cout << "Loaded cubemap face: " << p << std::endl;
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return texID;
}

// --- skybox VAO ---
void createSkyboxVAO(GLuint &vao, GLuint &vbo) {
    static const float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);
}

// --- ring mesh for Saturn ---
void createRingMesh(GLuint &ringVAO, GLuint &ringVBO, GLuint &ringEBO, GLsizei &ringIndexCount,
                    float innerR = 0.85f, float outerR = 1.5f, int segments = 128)
{
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    verts.reserve((segments+1)*2*8);
    idx.reserve(segments*6);

    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / segments * 2.0f * M_PI;
        float x = cosf(theta);
        float z = sinf(theta);

        verts.push_back(outerR * x); verts.push_back(0.0f); verts.push_back(outerR * z);
        verts.push_back(0.0f); verts.push_back(1.0f); verts.push_back(0.0f);
        verts.push_back((float)i / segments); verts.push_back(0.0f);

        verts.push_back(innerR * x); verts.push_back(0.0f); verts.push_back(innerR * z);
        verts.push_back(0.0f); verts.push_back(1.0f); verts.push_back(0.0f);
        verts.push_back((float)i / segments); verts.push_back(1.0f);
    }

    for (int i = 0; i < segments*2; i += 2) {
        idx.push_back(i);
        idx.push_back(i+1);
        idx.push_back(i+2);
        idx.push_back(i+1);
        idx.push_back(i+3);
        idx.push_back(i+2);
    }

    glGenVertexArrays(1, &ringVAO);
    glGenBuffers(1, &ringVBO);
    glGenBuffers(1, &ringEBO);
    glBindVertexArray(ringVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ringEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));
    glBindVertexArray(0);
    
    ringIndexCount = (GLsizei)idx.size();
}

// --- camera ---
bool flyMode = false;
bool keysArr[1024] = { false };

struct OrbitCam {
    glm::vec3 target = glm::vec3(0.0f);
    float distance = 6.0f;
    float yaw = glm::radians(90.0f);
    float pitch = 0.0f;
    float minD = 0.5f, maxD = 200.0f;
    glm::vec3 flyPos = glm::vec3(0.0f, 0.0f, 6.0f);

    glm::vec3 pos() const {
        if (flyMode) return flyPos;
        float x = distance * cosf(pitch) * cosf(yaw);
        float y = distance * sinf(pitch);
        float z = distance * cosf(pitch) * sinf(yaw);
        return target + glm::vec3(x,y,z);
    }

    glm::mat4 view() const {
        if (flyMode) {
            glm::vec3 front;
            front.x = cosf(pitch) * cosf(yaw);
            front.y = sinf(pitch);
            front.z = cosf(pitch) * sinf(yaw);
            front = glm::normalize(front);
            return glm::lookAt(flyPos, flyPos + front, glm::vec3(0.0f,1.0f,0.0f));
        }
        return glm::lookAt(pos(), target, glm::vec3(0.0f,1.0f,0.0f));
    }

    void moveFly(float dt) {
        float speed = 40.0f;
        glm::vec3 front;
        front.x = cosf(pitch) * cosf(yaw);
        front.y = sinf(pitch);
        front.z = cosf(pitch) * sinf(yaw);
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f,1.0f,0.0f)));
        glm::vec3 up = glm::vec3(0.0f,1.0f,0.0f);

        glm::vec3 move(0.0f);
        if (keysArr[GLFW_KEY_W]) move += front;
        if (keysArr[GLFW_KEY_S]) move -= front;
        if (keysArr[GLFW_KEY_A]) move -= right;
        if (keysArr[GLFW_KEY_D]) move += right;
        if (keysArr[GLFW_KEY_SPACE]) move += up;
        if (keysArr[GLFW_KEY_LEFT_SHIFT]) move -= up;

        if (glm::length(move) > 0.0001f) {
            flyPos += glm::normalize(move) * speed * dt;
        }
    }
    
    void setTarget(const glm::vec3& t) {
        target = t;
    }
} cam;

// input states
static bool leftDown = false;
static double lastX = 0.0, lastY = 0.0;

void mouseBtnCB(GLFWwindow* w, int button, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return; 
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftDown = true;
            glfwGetCursorPos(w, &lastX, &lastY);
        } else leftDown = false;
    }
}

void cursorPosCB(GLFWwindow* w, double x, double y) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    
    double dx = x - lastX;
    double dy = y - lastY;
    float sens = 0.0045f;
    
    if (leftDown && !flyMode) {
        cam.yaw   -= (float)(dx * sens);
        cam.pitch -= (float)(dy * sens);
        float lim = glm::radians(89.0f);
        if (cam.pitch > lim) cam.pitch = lim;
        if (cam.pitch < -lim) cam.pitch = -lim;
    } else if (flyMode) {
        cam.yaw   -= (float)(dx * sens);
        cam.pitch -= (float)(dy * sens);
        float lim = glm::radians(89.0f);
        if (cam.pitch > lim) cam.pitch = lim;
        if (cam.pitch < -lim) cam.pitch = -lim;
    }
    
    lastX = x; lastY = y;
}

void scrollCB(GLFWwindow* w, double xoff, double yoff) {
    if (ImGui::GetIO().WantCaptureMouse) return; 
    
    if (!flyMode) {
        cam.distance *= (float)pow(0.9, yoff);
        if (cam.distance < cam.minD) cam.distance = cam.minD;
        if (cam.distance > cam.maxD) cam.distance = cam.maxD;
    } else {
        glm::vec3 front;
        front.x = cosf(cam.pitch) * cosf(cam.yaw);
        front.y = sinf(cam.pitch);
        front.z = cosf(cam.pitch) * sinf(cam.yaw);
        cam.flyPos += front * (float)yoff * 2.0f;
    }
}

void keyCB(GLFWwindow* w, int key, int sc, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(w, true);
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) keysArr[key] = true;
        else if (action == GLFW_RELEASE) keysArr[key] = false;
    }
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        flyMode = !flyMode;
        if (flyMode) {
            cam.flyPos = cam.pos();
        }
        std::cout << "flyMode: " << (flyMode ? "ON" : "OFF") << std::endl;
    }
}

auto degPerSec = [](float days) -> float {
    return 360.0f / (days * 86400.0f);
};

bool simulationRunning = true;
float timeMultiplier = 1.0f;
float simulatedTimeDays = 0.0f;

struct Moon {
    std::string name;
    float orbitRadius;
    float orbitSpeed;
    float rotationSpeed;
    float size;
    GLuint texture;
    float orbitAngle;
    float rotationAngle;
};

struct Planet {
    std::string name;
    float orbitRadius;
    float orbitSpeed;
    float rotationSpeed;
    float axialTilt;
    float size;
    GLuint texture;
    float orbitAngle;
    float rotationAngle;
    bool hasRing;
    GLuint ringTex;
    std::vector<Moon> moons; 
};

int main() {
    int selectedPlanetIndex = -1;

    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "Solar System Explorer", NULL, NULL);
    if (!window) { std::cerr << "Window create failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glfwSetMouseButtonCallback(window, mouseBtnCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetScrollCallback(window, scrollCB);
    glfwSetKeyCallback(window, keyCB);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed\n"; return -1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // === Шейдеры, текстуры, VAO/VBO инициализация ===
    std::string planetVSs = readFile("shaders/planet.vert");
    std::string planetFSs = readFile("shaders/planet.frag");

    std::string skyVSs = readFile("shaders/skybox.vert");
    std::string skyFSs = readFile("shaders/skybox.frag");

    std::string sunVSs = readFile("shaders/sun.vert");
    std::string sunFSs = readFile("shaders/sun.frag");

    if (skyVSs.empty() || skyFSs.empty()) std::cerr << "Missing skybox shaders\n";
    GLuint planetV = compileShaderSrc(planetVSs.c_str(), GL_VERTEX_SHADER, "planet.vert");
    GLuint planetF = compileShaderSrc(planetFSs.c_str(), GL_FRAGMENT_SHADER, "planet.frag");
    GLuint planetProg = linkProgram(planetV, planetF);

    GLuint skyV = compileShaderSrc(skyVSs.c_str(), GL_VERTEX_SHADER, "skybox.vert");
    GLuint skyF = compileShaderSrc(skyFSs.c_str(), GL_FRAGMENT_SHADER, "skybox.frag");
    GLuint skyProg = linkProgram(skyV, skyF);

    GLuint sunV = compileShaderSrc(sunVSs.c_str(), GL_VERTEX_SHADER, "sun.vert");
    GLuint sunF = compileShaderSrc(sunFSs.c_str(), GL_FRAGMENT_SHADER, "sun.frag");
    GLuint sunProg = linkProgram(sunV, sunF);


    std::vector<float> verts; std::vector<unsigned int> inds;
    createSphere(1.0f, 64, 64, verts, inds);
    GLuint sunVAO, sunVBO, sunEBO;
    glGenVertexArrays(1, &sunVAO);
    glGenBuffers(1, &sunVBO);
    glGenBuffers(1, &sunEBO);
    glBindVertexArray(sunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO); glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sunEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size()*sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));
    glBindVertexArray(0);

    GLsizei sphereIndexCount = (GLsizei)inds.size();

    GLuint skyVAO, skyVBO; 
    createSkyboxVAO(skyVAO, skyVBO);

    GLuint sunTex = loadTextureTry("assets/sun.jpg");
    GLuint texMercury = loadTextureTry("assets/mercury.jpg");
    GLuint texVenus   = loadTextureTry("assets/venus.jpg");
    GLuint texEarth   = loadTextureTry("assets/earth.jpg");
    GLuint texMars    = loadTextureTry("assets/mars.jpg");
    GLuint texJupiter = loadTextureTry("assets/jupiter.jpg");
    GLuint texSaturn  = loadTextureTry("assets/saturn.jpg");
    GLuint texUranus  = loadTextureTry("assets/uranus.jpg");
    GLuint texNeptune = loadTextureTry("assets/neptune.jpg");
    GLuint texSaturnRing = loadTextureTry("assets/saturn_ring.png");
    GLuint texMoon = loadTextureTry("assets/moon.jpg"); // Новый текстур Луны

    std::vector<std::string> faces = {
        "assets/skybox/starfield_rt.tga",
        "assets/skybox/starfield_lf.tga",
        "assets/skybox/starfield_up.tga",
        "assets/skybox/starfield_dn.tga",
        "assets/skybox/starfield_ft.tga",
        "assets/skybox/starfield_bk.tga"
    };
    GLuint cubemap = loadCubemapFaces(faces);

    glUseProgram(planetProg);
    GLint sunTexLoc = glGetUniformLocation(planetProg, "sunTex");
    if (sunTexLoc >= 0) glUniform1i(sunTexLoc, 0);
    glUseProgram(skyProg);
    GLint skyLoc = glGetUniformLocation(skyProg, "skybox");
    if (skyLoc >= 0) glUniform1i(skyLoc, 0);

    std::map<std::string, float> orbitalPeriods = {
        {"Mercury", 87.97f},
        {"Venus", 224.7f},
        {"Earth", 365.256f},
        {"Mars", 687.0f},
        {"Jupiter", 4331.0f},
        {"Saturn", 10747.0f},
        {"Uranus", 30589.0f},
        {"Neptune", 59800.0f}
    };
    auto rotSpeedH = [](float hours) -> float {
        return 360.0f / (hours * 3600.0f);
    };

    std::vector<Planet> planets = {
        {"Mercury", 2.0f, degPerSec(orbitalPeriods["Mercury"]), rotSpeedH(1407.6f), 0.01f, 0.09f, texMercury, 0.0f, 0.0f, false, 0, {}},
        {"Venus",   3.0f, degPerSec(orbitalPeriods["Venus"]),   rotSpeedH(-5832.5f), 177.4f, 0.19f, texVenus, 60.0f, 0.0f, false, 0, {}},
        {"Earth",   4.0f, degPerSec(orbitalPeriods["Earth"]),   rotSpeedH(23.93f), 23.44f, 0.205f, texEarth, 120.0f, 0.0f, false, 0, {}},
        {"Mars",    5.0f, degPerSec(orbitalPeriods["Mars"]),    rotSpeedH(24.62f), 25.19f, 0.14f, texMars, 200.0f, 0.0f, false, 0, {}},
        {"Jupiter", 7.0f, degPerSec(orbitalPeriods["Jupiter"]), rotSpeedH(9.93f), 3.13f, 0.48f, texJupiter, 20.0f, 0.0f, false, 0, {}},
        {"Saturn",  9.0f, degPerSec(orbitalPeriods["Saturn"]),  rotSpeedH(10.56f), 26.73f, 0.42f, texSaturn, 300.0f, 0.0f, true,  texSaturnRing, {}},
        {"Uranus",  11.5f, degPerSec(orbitalPeriods["Uranus"]),  rotSpeedH(-17.24f), 97.77f, 0.28f, texUranus, 340.0f, 0.0f, false, 0, {}},
        {"Neptune", 14.0f, degPerSec(orbitalPeriods["Neptune"]), rotSpeedH(16.11f), 28.32f, 0.27f, texNeptune, 80.0f, 0.0f, false, 0, {}}
    };
    planets[2].moons.push_back({
        "Moon",  
        0.3f,                
        degPerSec(27.3f),     
        degPerSec(27.3f),
        0.05f,               
        texMoon,
        0.0f, 0.0f
    });

    GLuint ringVAO = 0, ringVBO = 0, ringEBO = 0; GLsizei ringIndexCount = 0;
    if (texSaturnRing) {
        createRingMesh(ringVAO, ringVBO, ringEBO, ringIndexCount, 0.85f, 1.1f, 256);
    }

    GLuint planetVAO, planetVBO, planetEBO;
    glGenVertexArrays(1, &planetVAO);
    glGenBuffers(1, &planetVBO);
    glGenBuffers(1, &planetEBO);
    glBindVertexArray(planetVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planetVBO); glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planetEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size()*sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));
    glBindVertexArray(0);

    std::vector<GLuint> orbitVAOs(planets.size());
    std::vector<GLuint> orbitVBOs(planets.size());
    std::vector<int> orbitVertexCounts(planets.size());
    for (size_t i = 0; i < planets.size(); ++i) {
        std::vector<float> orbitLineVerts;
        createOrbitLine(planets[i].orbitRadius, 128, orbitLineVerts);
        orbitVertexCounts[i] = (int)orbitLineVerts.size()/3;
        glGenVertexArrays(1, &orbitVAOs[i]);
        glGenBuffers(1, &orbitVBOs[i]);
        glBindVertexArray(orbitVAOs[i]);
        glBindBuffer(GL_ARRAY_BUFFER, orbitVBOs[i]);
        glBufferData(GL_ARRAY_BUFFER, orbitLineVerts.size()*sizeof(float), orbitLineVerts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
        glBindVertexArray(0);
    }

    cam.distance = 12.0f; cam.yaw = glm::radians(90.0f); cam.pitch = glm::radians(-10.0f);
    cam.flyPos = glm::vec3(0.0f, 0.0f, 12.0f);

    glUseProgram(planetProg);
    GLint planetLightPosLoc = glGetUniformLocation(planetProg, "lightPos");
    GLint planetViewPosLoc = glGetUniformLocation(planetProg, "viewPos");
    GLint planetAmbientKLoc = glGetUniformLocation(planetProg, "ambientK");
    GLint planetModelLoc = glGetUniformLocation(planetProg, "model");
    GLint planetViewLoc = glGetUniformLocation(planetProg, "view");
    GLint planetProjLoc = glGetUniformLocation(planetProg, "projection");
    GLint planetTexLoc = glGetUniformLocation(planetProg, "planetTex");
    
    if (planetLightPosLoc < 0) {
        std::cerr << "ERROR: planetProg missing uniform 'lightPos'! Shader may not be compiled correctly.\n";
        std::cerr << "Make sure planet.frag uses 'uniform vec3 lightPos;'\n";
        std::cerr << "Current shader may still use 'lightDir' instead of 'lightPos'!\n";
    } else {
        std::cout << "✓ planetProg lightPos location: " << planetLightPosLoc << std::endl;
    }
    
    GLint testLightDir = glGetUniformLocation(planetProg, "lightDir");
    if (testLightDir >= 0) {
        std::cerr << "WARNING: planetProg still has 'lightDir' uniform! Shader may not be updated!\n";
        std::cerr << "Make sure planet.frag uses 'uniform vec3 lightPos;' NOT 'lightDir'!\n";
    }

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dtReal = (float)(now - lastTime); 
        lastTime = now;
        
        if (flyMode) cam.moveFly(dtReal);
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // === ImGui панели ===
        ImGui::Begin("Focus");
        const char* planetNames[] = {"Sun","Mercury","Venus","Earth","Mars","Jupiter","Saturn","Uranus","Neptune"};
        static int focusIdx = selectedPlanetIndex+1;
        if(ImGui::Combo("Focus",&focusIdx,planetNames,9)) {
            selectedPlanetIndex=focusIdx-1; 
            flyMode=false;
        }
        ImGui::End();

        ImGui::Begin("Simulation");
        if(ImGui::Button(simulationRunning?"Pause":"Start")) simulationRunning=!simulationRunning;
        ImGui::SameLine(); 
        if(ImGui::Button("Reset")) {
            simulatedTimeDays=0.0f; 
            for(auto &p:planets){
                p.orbitAngle=0.0f;
                p.rotationAngle=0.0f;
                for(auto &m:p.moons){
                    m.orbitAngle=0.0f;
                    m.rotationAngle=0.0f;
                }
            }
        }
        ImGui::SliderFloat("Speed",&timeMultiplier,0.1f,315360000.f,"%.0fx",ImGuiSliderFlags_Logarithmic);
        std::string simTimeStr=formatSimulatedTime(simulatedTimeDays);
        ImGui::Text("Simulated Time: %s",simTimeStr.c_str());
        ImGui::End();

        if(simulationRunning) simulatedTimeDays+=dtReal*timeMultiplier/86400.f;
        float dtSim = dtReal * timeMultiplier;

        for(size_t i = 0; i < planets.size(); ++i) {
            Planet &p = planets[i];
            p.orbitAngle += dtSim * p.orbitSpeed;
            if(p.orbitAngle > 360.f) p.orbitAngle -= 360.f;
            float oldRotation = p.rotationAngle;
            p.rotationAngle += dtSim * p.rotationSpeed;
            if(p.rotationAngle > 360.f) p.rotationAngle -= 360.f;
            
            static int debugPlanetFrameCount = 0;
            if (debugPlanetFrameCount++ == 0 && i == 0) { 
                std::cout << "=== DEBUG: Планета " << p.name << " rotationAngle обновлен: " << oldRotation 
                          << " -> " << p.rotationAngle << " (dtSim=" << dtSim << ", speed=" << p.rotationSpeed << ")\n";
                std::cout << "=== DEBUG: rotationAngle НЕ зависит от камеры, только от времени!\n";
            }

            for(auto &m : p.moons) {
                m.orbitAngle += dtSim * m.orbitSpeed;
                if(m.orbitAngle > 360.0f) m.orbitAngle -= 360.0f;
                m.rotationAngle += dtSim * m.rotationSpeed;
                if(m.rotationAngle > 360.0f) m.rotationAngle -= 360.0f;
            }
        }


        // === CAMERA TARGET ===
        if (selectedPlanetIndex >= 0 && selectedPlanetIndex < (int)planets.size()) {
            Planet& p = planets[selectedPlanetIndex];
            float angRad = glm::radians(p.orbitAngle);
            glm::vec3 planetCenter(cosf(angRad) * p.orbitRadius, 0.f, sinf(angRad) * p.orbitRadius);
            cam.setTarget(planetCenter); 
        } else {
            cam.setTarget(glm::vec3(0.0f)); 
        }


        glClearColor(0,0,0,1); 
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f),(float)SCR_W/(float)SCR_H,0.1f,200.0f);
        glm::mat4 view = cam.view();  
        glm::vec3 camPos = cam.pos();
        
        glm::vec3 sunPos = glm::vec3(0.0f, 0.0f, 0.0f);

        // === 1. SKYBOX ===
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glUseProgram(skyProg);
        glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(glGetUniformLocation(skyProg,"view"),1,GL_FALSE,glm::value_ptr(viewNoTrans));
        glUniformMatrix4fv(glGetUniformLocation(skyProg,"projection"),1,GL_FALSE,glm::value_ptr(proj));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
        glBindVertexArray(skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);

        // === 2. SUN ===
        glm::mat4 sunModel = glm::mat4(1.0f);

        sunModel = glm::rotate(sunModel, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Полюса вверх
        sunModel = glm::rotate(sunModel, (float)glfwGetTime() * glm::radians(12.0f), glm::vec3(0.0f, 0.0f, 1.0f)); // Вращение (только от времени!)
        sunModel = glm::scale(sunModel, glm::vec3(1.4f));


        glUseProgram(sunProg);
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"model"),1,GL_FALSE,glm::value_ptr(sunModel));
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"projection"),1,GL_FALSE,glm::value_ptr(proj));

        glm::vec3 sunLightDir = glm::normalize(sunPos - camPos);
        glUniform3f(glGetUniformLocation(sunProg, "lightPos"), sunPos.x, sunPos.y, sunPos.z);
        glUniform3f(glGetUniformLocation(sunProg,"viewPos"), camPos.x, camPos.y, camPos.z);
        int sunTexLoc = glGetUniformLocation(sunProg, "sunTex");
        glUniform1i(sunTexLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTex);
        glBindVertexArray(planetVAO);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);


        // === 3. ORBIT LINES ===
        for (size_t i = 0; i < planets.size(); ++i) {
            glUseProgram(planetProg);
            glUniformMatrix4fv(glGetUniformLocation(planetProg,"model"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
            glUniformMatrix4fv(glGetUniformLocation(planetProg,"view"),1,GL_FALSE,glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(planetProg,"projection"),1,GL_FALSE,glm::value_ptr(proj));

            glBindVertexArray(orbitVAOs[i]);
            glDrawArrays(GL_LINE_STRIP, 0, orbitVertexCounts[i]);
            glBindVertexArray(0);
        }

        // === 4. PLANETS & MOONS ===
        glUseProgram(planetProg); 
        
        if (planetLightPosLoc >= 0) {
            glUniform3f(planetLightPosLoc, sunPos.x, sunPos.y, sunPos.z);

            static int debugFrameCount = 0;
            if (debugFrameCount++ == 0) {  
                std::cout << "=== DEBUG: lightPos установлен в: (" << sunPos.x << ", " << sunPos.y << ", " << sunPos.z << ")\n";
                std::cout << "=== DEBUG: camPos = (" << camPos.x << ", " << camPos.y << ", " << camPos.z << ")\n";
                std::cout << "=== DEBUG: Источник света = СОЛНЦЕ (0,0,0), НЕ камера!\n";
            }
        } else {
            std::cerr << "ERROR: planetLightPosLoc is invalid! Shadows will not work!\n";
            std::cerr << "ERROR: Шейдер может не перекомпилироваться! Проверьте planet.frag\n";
        }
        
        if (planetViewPosLoc >= 0) {
            glUniform3f(planetViewPosLoc, camPos.x, camPos.y, camPos.z);
        }
        
        for(auto &p:planets) {
            float angRad=glm::radians(p.orbitAngle);
            glm::vec3 planetPos(cosf(angRad)*p.orbitRadius,0.f,sinf(angRad)*p.orbitRadius);

            glm::mat4 pModel = glm::mat4(1.0f);
            pModel = glm::translate(pModel, planetPos);  
            pModel = glm::rotate(pModel, glm::radians(-90.0f), glm::vec3(1.0f,0.0f,0.0f)); 
            pModel = glm::rotate(pModel, glm::radians(p.axialTilt), glm::vec3(0.0f,1.0f,0.0f)); 
            pModel = glm::rotate(pModel, glm::radians(p.rotationAngle), glm::vec3(0.0f,0.0f,1.0f)); 
            pModel = glm::scale(pModel, glm::vec3(p.size));

            if (planetModelLoc >= 0) glUniformMatrix4fv(planetModelLoc, 1, GL_FALSE, glm::value_ptr(pModel));
            if (planetViewLoc >= 0) glUniformMatrix4fv(planetViewLoc, 1, GL_FALSE, glm::value_ptr(view));
            if (planetProjLoc >= 0) glUniformMatrix4fv(planetProjLoc, 1, GL_FALSE, glm::value_ptr(proj));
                    
            if (planetAmbientKLoc >= 0) {
                glUniform1f(planetAmbientKLoc, 0.10f); 
            }
                    
            if (planetTexLoc >= 0) glUniform1i(planetTexLoc, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p.texture); 
            glBindVertexArray(planetVAO);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);



            // === MOONS ===
            for (auto &m : p.moons) {
                float moonAngRad = glm::radians(m.orbitAngle);
                glm::vec3 moonPos = planetPos + glm::vec3(cosf(moonAngRad) * m.orbitRadius, 0.0f, sinf(moonAngRad) * m.orbitRadius);

                glm::mat4 moonModel = glm::mat4(1.0f);
                moonModel = glm::translate(moonModel, moonPos);
                moonModel = glm::rotate(moonModel, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); 
                moonModel = glm::rotate(moonModel, glm::radians(m.rotationAngle), glm::vec3(0.0f, 0.0f, 1.0f)); 
                moonModel = glm::scale(moonModel, glm::vec3(m.size));

                if (planetModelLoc >= 0) glUniformMatrix4fv(planetModelLoc, 1, GL_FALSE, glm::value_ptr(moonModel));


                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m.texture);
                glBindVertexArray(planetVAO);
                glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
            }       

            // === SATURN RING ===
            if (p.hasRing && ringVAO && p.ringTex) {
                glm::mat4 rModel=glm::translate(glm::mat4(1.0f),planetPos);
                rModel = glm::rotate(rModel, glm::radians(26.7f), glm::vec3(1.0f,0.0f,0.0f));
                rModel = glm::scale(rModel, glm::vec3(p.size*2.0f));

                if (planetModelLoc >= 0) glUniformMatrix4fv(planetModelLoc, 1, GL_FALSE, glm::value_ptr(rModel));
                if (planetViewLoc >= 0) glUniformMatrix4fv(planetViewLoc, 1, GL_FALSE, glm::value_ptr(view));
                if (planetProjLoc >= 0) glUniformMatrix4fv(planetProjLoc, 1, GL_FALSE, glm::value_ptr(proj));


                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D,p.ringTex);
                glBindVertexArray(ringVAO); 
                glDrawElements(GL_TRIANGLES, ringIndexCount, GL_UNSIGNED_INT, 0); 
                glBindVertexArray(0);
                glDisable(GL_BLEND);
            }
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }   


    // --- cleanup ---
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glDeleteVertexArrays(1,&sunVAO); glDeleteBuffers(1,&sunVBO); glDeleteBuffers(1,&sunEBO);
    glDeleteVertexArrays(1,&planetVAO); glDeleteBuffers(1,&planetVBO); glDeleteBuffers(1,&planetEBO);
    if (ringVAO) { glDeleteVertexArrays(1,&ringVAO); glDeleteBuffers(1,&ringVBO); glDeleteBuffers(1,&ringEBO);}
    for(auto vao:orbitVAOs) glDeleteVertexArrays(1,&vao);
    for(auto vbo:orbitVBOs) glDeleteBuffers(1,&vbo);
    glDeleteVertexArrays(1,&skyVAO); glDeleteBuffers(1,&skyVBO);
    glDeleteProgram(planetProg); glDeleteProgram(skyProg);
    GLuint texs[]={sunTex,texMercury,texVenus,texEarth,texMars,texJupiter,texSaturn,texUranus,texNeptune,texSaturnRing,cubemap,texMoon};
    for(auto t:texs) if(t) glDeleteTextures(1,&t);
    glfwTerminate();
    return 0;
}

