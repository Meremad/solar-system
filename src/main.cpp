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
#include "../include/stb_image.h" // put stb_image.h in include/

const unsigned int SCR_W = 1280;
const unsigned int SCR_H = 720;

// --- file helpers (unchanged) ---
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



// --- shader utils (unchanged) ---
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

// --- sphere generator (unchanged) ---
void createSphere(float radius, unsigned int sectorCount, unsigned int stackCount,
                  std::vector<float>& vertices, std::vector<unsigned int>& indices)
{
    vertices.clear(); indices.clear();
    const float PI = acos(-1.0f);
    for (unsigned int i = 0; i <= stackCount; ++i) {
        float stackAngle = PI/2 - (float)i * PI / stackCount;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);
        for (unsigned int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = (float)j * 2.0f * PI / sectorCount;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            vertices.push_back(x); 
            vertices.push_back(y); 
            vertices.push_back(z);

            glm::vec3 n = glm::normalize(glm::vec3(x,y,z));
            vertices.push_back(n.x); vertices.push_back(n.y); vertices.push_back(n.z);

            float s = (float)j / sectorCount;
            float t = (float)i / stackCount;

            vertices.push_back(s);         // U
            vertices.push_back(t);         // V
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

// --- texture loader (unchanged) ---
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

        // outer vertex (pos, normal, uv)
        verts.push_back(outerR * x); verts.push_back(0.0f); verts.push_back(outerR * z);
        verts.push_back(0.0f); verts.push_back(1.0f); verts.push_back(0.0f);
        verts.push_back((float)i / segments); verts.push_back(0.0f);

        // inner vertex
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
}

// --- camera (orbit+fly) unchanged except additional setter ---
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

// input states (unchanged)
static bool leftDown = false;
static double lastX = 0.0, lastY = 0.0;

void mouseBtnCB(GLFWwindow* w, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftDown = true;
            glfwGetCursorPos(w, &lastX, &lastY);
        } else leftDown = false;
    }
}

void cursorPosCB(GLFWwindow* w, double x, double y) {
    double dx = x - lastX;
    double dy = y - lastY;
    float sens = 0.0045f;
    if (leftDown || flyMode) {
        cam.yaw   -= (float)(dx * sens);
        cam.pitch -= (float)(dy * sens);
        float lim = glm::radians(89.0f);
        if (cam.pitch > lim) cam.pitch = lim;
        if (cam.pitch < -lim) cam.pitch = -lim;
    }
    lastX = x; lastY = y;
}

void scrollCB(GLFWwindow* w, double xoff, double yoff) {
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


// Utility: convert orbital period in days to degrees per second
auto degPerSec = [](float days) -> float {
    return 360.0f / (days * 86400.0f);
};


// Timing vars for simulation controls
bool simulationRunning = true;
float timeMultiplier = 1.0f;   // Simulation speed multiplier (times real speed)
float simulatedTimeDays = 0.0f;

// Planet data structure including axial tilt and rotation speed
struct Planet {
    std::string name;
    float orbitRadius;
    float orbitSpeed;   // degrees per second (orbiting around sun)
    float rotationSpeed; // degrees per second (self spin)
    float axialTilt;    // degrees tilt on own axis
    float size;
    GLuint texture;
    float orbitAngle;
    float rotationAngle;
    bool hasRing;
    GLuint ringTex;
};

// Main function
int main() {
    std::map<int, std::string> planetChoices = {
        {0, "Mercury"}, {1, "Venus"}, {2, "Earth"}, {3, "Mars"},
        {4, "Jupiter"}, {5, "Saturn"}, {6, "Uranus"}, {7, "Neptune"}
    };
    int selectedPlanetIndex = -1;
    std::cout << "Select a planet to focus camera on:\n";
    for (auto& [i, name] : planetChoices) {
        std::cout << i << ": " << name << "\n";
    }
    std::cout << "Enter number (-1 for none): ";
    std::cin >> selectedPlanetIndex;
    if (selectedPlanetIndex < -1 || selectedPlanetIndex > 7) selectedPlanetIndex = -1;

    // Initialize GLFW window context
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "SolarSystem (orbit + fly cam)", NULL, NULL);
    if (!window) { std::cerr << "Window create failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glfwSetMouseButtonCallback(window, mouseBtnCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetScrollCallback(window, scrollCB);
    glfwSetKeyCallback(window, keyCB);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed\n"; return -1; }

    // Setup ImGui context
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

    // Load shaders
    std::string sunVSs = readFile("shaders/sun.vert");
    std::string sunFSs = readFile("shaders/sun.frag");
    std::string skyVSs = readFile("shaders/skybox.vert");
    std::string skyFSs = readFile("shaders/skybox.frag");

    if (sunVSs.empty() || sunFSs.empty()) std::cerr << "Missing sun shaders\n";
    if (skyVSs.empty() || skyFSs.empty()) std::cerr << "Missing skybox shaders\n";

    GLuint sunV = compileShaderSrc(sunVSs.c_str(), GL_VERTEX_SHADER, "sun.vert");
    GLuint sunF = compileShaderSrc(sunFSs.c_str(), GL_FRAGMENT_SHADER, "sun.frag");
    GLuint sunProg = linkProgram(sunV, sunF);

    GLuint skyV = compileShaderSrc(skyVSs.c_str(), GL_VERTEX_SHADER, "skybox.vert");
    GLuint skyF = compileShaderSrc(skyFSs.c_str(), GL_FRAGMENT_SHADER, "skybox.frag");
    GLuint skyProg = linkProgram(skyV, skyF);

    // Create sphere mesh for planets sun
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

    // Skybox VAO
    GLuint skyVAO, skyVBO; createSkyboxVAO(skyVAO, skyVBO);

    // Textures
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

    std::vector<std::string> faces = {
        "assets/skybox/starfield_rt.tga",
        "assets/skybox/starfield_lf.tga",
        "assets/skybox/starfield_up.tga",
        "assets/skybox/starfield_dn.tga",
        "assets/skybox/starfield_ft.tga",
        "assets/skybox/starfield_bk.tga"
    };
    GLuint cubemap = loadCubemapFaces(faces);

    // Uniforms for samplers
    glUseProgram(sunProg);
    GLint sunTexLoc = glGetUniformLocation(sunProg, "sunTex");
    if (sunTexLoc >= 0) glUniform1i(sunTexLoc, 0);
    glUseProgram(skyProg);
    GLint skyLoc = glGetUniformLocation(skyProg, "skybox");
    if (skyLoc >= 0) glUniform1i(skyLoc, 0);

    // Orbital periods by planet (days)
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

    // Conversion helper: rotation speed in degrees per second from hours
    auto rotSpeedH = [](float hours) -> float {
        return 360.0f / (hours * 3600.0f);
    };

    // Create planet vector with orbitSpeed and rotationSpeed including axial tilt
    std::vector<Planet> planets = {
        {"Mercury", 2.0f, degPerSec(orbitalPeriods["Mercury"]), rotSpeedH(1407.6f), 0.01f, 0.09f, texMercury, 0.0f, 0.0f, false, 0},
        {"Venus",   3.0f, degPerSec(orbitalPeriods["Venus"]),   rotSpeedH(-5832.5f), 177.4f, 0.19f, texVenus, 60.0f, 0.0f, false, 0},
        {"Earth",   4.0f, degPerSec(orbitalPeriods["Earth"]),   rotSpeedH(23.93f), 23.44f, 0.205f, texEarth, 120.0f, 0.0f, false, 0},
        {"Mars",    5.0f, degPerSec(orbitalPeriods["Mars"]),    rotSpeedH(24.62f), 25.19f, 0.14f, texMars, 200.0f, 0.0f, false, 0},
        {"Jupiter", 7.0f, degPerSec(orbitalPeriods["Jupiter"]), rotSpeedH(9.93f), 3.13f, 0.48f, texJupiter, 20.0f, 0.0f, false, 0},
        {"Saturn",  9.0f, degPerSec(orbitalPeriods["Saturn"]),  rotSpeedH(10.56f), 26.73f, 0.42f, texSaturn, 300.0f, 0.0f, true,  texSaturnRing},
        {"Uranus",  11.5f, degPerSec(orbitalPeriods["Uranus"]),  rotSpeedH(-17.24f), 97.77f, 0.28f, texUranus, 340.0f, 0.0f, false, 0},
        {"Neptune", 14.0f, degPerSec(orbitalPeriods["Neptune"]), rotSpeedH(16.11f), 28.32f, 0.27f, texNeptune, 80.0f, 0.0f, false, 0}
    };

    // Ring mesh for Saturn
    GLuint ringVAO = 0, ringVBO = 0, ringEBO = 0; GLsizei ringIndexCount = 0;
    if (texSaturnRing) {
        createRingMesh(ringVAO, ringVBO, ringEBO, ringIndexCount, 0.85f, 1.1f, 256);
    }

    // Planet VAO reuse sphere mesh
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

    // Create orbit lines
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

    // Initial camera settings
    cam.distance = 12.0f;
    cam.yaw = glm::radians(90.0f);
    cam.pitch = glm::radians(-10.0f);
    cam.flyPos = glm::vec3(0.0f, 0.0f, 12.0f);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dtReal = (float)(now - lastTime);
        lastTime = now;

        if (flyMode) cam.moveFly(dtReal);
        glfwPollEvents();

        // ImGui frame start
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Simulation control UI
        ImGui::Begin("Simulation Controls");
        if (ImGui::Button(simulationRunning ? "Stop" : "Start"))
            simulationRunning = !simulationRunning;
        ImGui::SameLine();
        ImGui::SliderFloat("Speed Multiplier", &timeMultiplier, 0.1f, 315360000.0f, "%.0fx");

        // Calculate how much time advances this frame realistically scaled to speed
        float deltaSimulatedDaysThisFrame = dtReal * timeMultiplier / 86400.0f;
        simulatedTimeDays += simulationRunning ? deltaSimulatedDaysThisFrame : 0.0f;

        std::string simTimeStr = formatSimulatedTime(simulatedTimeDays);
        ImGui::Text("Simulated Time: %s (%.2f days/sec)", simTimeStr.c_str(), timeMultiplier);

        ImGui::End();


        // Update simulation time if running
        if (simulationRunning) {
            simulatedTimeDays += dtReal * timeMultiplier / 86400.0f;
        }
        float dtSim = dtReal * timeMultiplier;

        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)SCR_W/(float)SCR_H, 0.1f, 200.0f);
        glm::mat4 view = cam.view();
        glm::vec3 camPos = cam.pos();

        // Draw skybox
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

        // Draw sun at origin
        glm::mat4 sunModel = glm::rotate(glm::mat4(1.0f), (float)now * glm::radians(12.0f), glm::vec3(0.0f,1.0f,0.0f));
        sunModel = glm::scale(sunModel, glm::vec3(1.4f));
        glm::vec3 sunWorldPos = glm::vec3(sunModel * glm::vec4(0.0f,0.0f,0.0f,1.0f));

        glUseProgram(sunProg);
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"model"),1,GL_FALSE,glm::value_ptr(sunModel));
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"projection"),1,GL_FALSE,glm::value_ptr(proj));
        glm::vec3 sunToCam = glm::normalize(camPos - sunWorldPos);
        glUniform3f(glGetUniformLocation(sunProg,"lightDir"), sunToCam.x, sunToCam.y, sunToCam.z);
        glUniform3f(glGetUniformLocation(sunProg,"viewPos"), camPos.x, camPos.y, camPos.z);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTex);
        glBindVertexArray(sunVAO);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Draw orbit lines
        glUseProgram(sunProg);
        for (size_t i = 0; i < planets.size(); ++i) {
            glUniformMatrix4fv(glGetUniformLocation(sunProg,"model"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
            glUniformMatrix4fv(glGetUniformLocation(sunProg,"view"),1,GL_FALSE,glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(sunProg,"projection"),1,GL_FALSE,glm::value_ptr(proj));

            glm::vec3 whiteLight(1.0f,1.0f,1.0f);
            glUniform3f(glGetUniformLocation(sunProg,"lightDir"), whiteLight.x, whiteLight.y, whiteLight.z);
            glUniform3f(glGetUniformLocation(sunProg,"viewPos"), camPos.x, camPos.y, camPos.z);
            glBindVertexArray(orbitVAOs[i]);
            glDrawArrays(GL_LINE_STRIP, 0, orbitVertexCounts[i]);
            glBindVertexArray(0);
        }

        // Update planet positions, orbit and self rotation
        if (selectedPlanetIndex >= 0 && selectedPlanetIndex < (int)planets.size()) {
            Planet &focusP = planets[selectedPlanetIndex];
            cam.setTarget(glm::vec3(cos(glm::radians(focusP.orbitAngle)) * focusP.orbitRadius, 0.0f, sin(glm::radians(focusP.orbitAngle)) * focusP.orbitRadius));
            flyMode = false;
        } else {
            cam.setTarget(glm::vec3(0.0f));
        }

        for (auto &p : planets) {
            p.orbitAngle += dtSim * p.orbitSpeed;
            if (p.orbitAngle > 360.0f) p.orbitAngle -= 360.0f;

            p.rotationAngle += dtSim * p.rotationSpeed;
            if (p.rotationAngle > 360.0f) p.rotationAngle -= 360.0f;

            float angRad = glm::radians(p.orbitAngle);
            glm::vec3 planetPos = glm::vec3(cosf(angRad) * p.orbitRadius, 0.0f, sinf(angRad) * p.orbitRadius);

            glm::mat4 pModel = glm::translate(glm::mat4(1.0f), planetPos);

            // Apply axial tilt around X axis
            pModel = glm::rotate(pModel, glm::radians(p.axialTilt), glm::vec3(1.0f, 0.0f, 0.0f));

            // Then planet self-rotation about Y axis
            pModel = glm::rotate(pModel, glm::radians(p.rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));

            pModel = glm::scale(pModel, glm::vec3(p.size));

            glUniformMatrix4fv(glGetUniformLocation(sunProg,"model"), 1, GL_FALSE, glm::value_ptr(pModel));
            glUniformMatrix4fv(glGetUniformLocation(sunProg,"view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(sunProg,"projection"), 1, GL_FALSE, glm::value_ptr(proj));

            glm::vec3 lightDir = glm::normalize(sunWorldPos - planetPos);
            glUniform3f(glGetUniformLocation(sunProg,"lightDir"), lightDir.x, lightDir.y, lightDir.z);
            glUniform3f(glGetUniformLocation(sunProg,"viewPos"), camPos.x, camPos.y, camPos.z);

            glActiveTexture(GL_TEXTURE0);
            if (p.texture) glBindTexture(GL_TEXTURE_2D, p.texture);
            else glBindTexture(GL_TEXTURE_2D, sunTex);

            glBindVertexArray(planetVAO);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);

            // Draw Saturn's ring
            if (p.hasRing && ringVAO && p.ringTex) {
                glm::mat4 rModel = glm::translate(glm::mat4(1.0f), planetPos);
                rModel = glm::rotate(rModel, glm::radians(26.7f), glm::vec3(1.0f,0.0f,0.0f));
                rModel = glm::scale(rModel, glm::vec3(p.size * 1.3f));

                glUniformMatrix4fv(glGetUniformLocation(sunProg,"model"),1,GL_FALSE,glm::value_ptr(rModel));
                glUniformMatrix4fv(glGetUniformLocation(sunProg,"view"),1,GL_FALSE,glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(sunProg,"projection"),1,GL_FALSE,glm::value_ptr(proj));
                glUniform3f(glGetUniformLocation(sunProg,"lightDir"), lightDir.x, lightDir.y, lightDir.z);
                glUniform3f(glGetUniformLocation(sunProg,"viewPos"), camPos.x, camPos.y, camPos.z);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, p.ringTex);
                glBindVertexArray(ringVAO);
                glDrawElements(GL_TRIANGLES, ringIndexCount, GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
                glDisable(GL_BLEND);
            }
        }

        // Render ImGui overlay
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &sunVAO); glDeleteBuffers(1, &sunVBO); glDeleteBuffers(1, &sunEBO);
    glDeleteVertexArrays(1, &planetVAO); glDeleteBuffers(1, &planetVBO); glDeleteBuffers(1, &planetEBO);
    if (ringVAO) { glDeleteVertexArrays(1, &ringVAO); glDeleteBuffers(1, &ringVBO); glDeleteBuffers(1, &ringEBO); }
    for (auto vao : orbitVAOs) glDeleteVertexArrays(1, &vao);
    for (auto vbo : orbitVBOs) glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &skyVAO); glDeleteBuffers(1, &skyVBO);
    glDeleteProgram(sunProg); glDeleteProgram(skyProg);
    GLuint texs[] = { sunTex, texMercury, texVenus, texEarth, texMars, texJupiter, texSaturn, texUranus, texNeptune, texSaturnRing, cubemap };
    for (auto t : texs) if (t) glDeleteTextures(1, &t);

    glfwTerminate();
    return 0;
}
