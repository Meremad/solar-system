// src/main.cpp
// Textured Sun + cubemap skybox + orbit+fly camera (F switches mode)
// Requires: GLFW, GLEW, GLM, stb_image.h (in include/)
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h" // place stb_image.h in include/

const unsigned int SCR_W = 1280;
const unsigned int SCR_H = 720;

// -------------------- file helpers --------------------
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

// -------------------- shader utils --------------------
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

// -------------------- sphere generator --------------------
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
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            glm::vec3 n = glm::normalize(glm::vec3(x,y,z));
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

// -------------------- texture loaders --------------------
GLuint loadTextureTry(const std::string &relPath) {
    std::string path = tryPrefixes(relPath);
    int w,h,comp;
    stbi_set_flip_vertically_on_load(true);
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
    std::cout << "Loaded texture: " << path << " ("<<w<<"x"<<h<<")\n";
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

// -------------------- skybox VAO --------------------
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

// -------------------- camera (orbit + fly) --------------------
bool flyMode = false;               // global mode flag
bool keysArr[1024] = { false };     // keyboard state

struct OrbitCam {
    glm::vec3 target = glm::vec3(0.0f); // in orbit mode, target is center; in fly mode, target used as fly position
    float distance = 6.0f;
    float yaw = glm::radians(90.0f);
    float pitch = 0.0f;
    float minD = 0.5f, maxD = 200.0f;

    // fly-specific position (world space)
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
        // move relative to camera orientation
        float speed = 40.0f; // units per second
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
} cam;

// ------------- input state -------------
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
    // In orbit: only rotate while leftDown
    // In fly: rotate whenever mouse moves while leftDown OR even if not pressed (we will allow rotation when flyMode and leftDown)
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
        // in fly mode, move forward/back with scroll
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
            // enter fly: set flyPos to current camera pos
            cam.flyPos = cam.pos();
            // hide cursor for fly nicer control? (optional)
            // glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            // exit fly: leave cursor normal
            // glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        std::cout << "flyMode: " << (flyMode ? "ON" : "OFF") << std::endl;
    }
}

// -------------------- main --------------------
int main() {
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

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // load shaders
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

    // create sun mesh
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

    // skybox VAO
    GLuint skyVAO, skyVBO; createSkyboxVAO(skyVAO, skyVBO);

    // textures
    GLuint sunTex = loadTextureTry("assets/sun.jpg");
    if (sunTex == 0) std::cerr << "Put assets/sun.jpg\n";
    std::vector<std::string> faces = {
        "assets/skybox/starfield_rt.tga",
        "assets/skybox/starfield_lf.tga",
        "assets/skybox/starfield_up.tga",
        "assets/skybox/starfield_dn.tga",
        "assets/skybox/starfield_ft.tga",
        "assets/skybox/starfield_bk.tga"
    };
    GLuint cubemap = loadCubemapFaces(faces);

    // uniforms (samplers)
    glUseProgram(sunProg);
    GLint sunTexLoc = glGetUniformLocation(sunProg, "sunTex");
    if (sunTexLoc >= 0) glUniform1i(sunTexLoc, 0);
    glUseProgram(skyProg);
    GLint skyLoc = glGetUniformLocation(skyProg, "skybox");
    if (skyLoc >= 0) glUniform1i(skyLoc, 0);

    // initial camera
    cam.distance = 6.0f;
    cam.yaw = glm::radians(90.0f);
    cam.pitch = 0.0f;
    cam.flyPos = glm::vec3(0.0f, 0.0f, 6.0f);

    // timing for moveFly
    double lastTime = glfwGetTime();

    // main loop
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        // update fly movement if active
        if (flyMode) cam.moveFly(dt);

        // poll events (callbacks will update mouse/key state)
        glfwPollEvents();

        // clear
        glClearColor(0.0f,0.0f,0.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float time = (float)now;
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)SCR_W/(float)SCR_H, 0.1f, 200.0f);
        glm::mat4 view = cam.view();

        // DRAW SKYBOX FIRST (don't write depth)
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

        // DRAW SUN
        glUseProgram(sunProg);
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(12.0f), glm::vec3(0.0f,1.0f,0.0f));
        model = glm::scale(model, glm::vec3(1.4f));
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"projection"),1,GL_FALSE,glm::value_ptr(proj));
        glm::vec3 lightDir = glm::normalize(glm::vec3(-0.3f,-1.0f,-0.4f));
        glUniform3f(glGetUniformLocation(sunProg,"lightDir"), lightDir.x, lightDir.y, lightDir.z);
        glm::vec3 camPos = cam.pos();
        glUniform3f(glGetUniformLocation(sunProg,"viewPos"), camPos.x, camPos.y, camPos.z);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTex);
        glBindVertexArray(sunVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)inds.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // swap
        glfwSwapBuffers(window);
    }

    // cleanup
    glDeleteVertexArrays(1, &sunVAO); glDeleteBuffers(1, &sunVBO); glDeleteBuffers(1, &sunEBO);
    glDeleteVertexArrays(1, &skyVAO); glDeleteBuffers(1, &skyVBO);
    glDeleteProgram(sunProg); glDeleteProgram(skyProg);
    glDeleteTextures(1, &sunTex); glDeleteTextures(1, &cubemap);

    glfwTerminate();
    return 0;
}
