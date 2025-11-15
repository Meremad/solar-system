# 🌞 Solar System — OpenGL Visualization

A cross-platform (macOS/Windows/Linux) OpenGL project featuring an interactive, real‑time visualization of the Solar System. Includes the Sun, planets, moons, Saturn’s rings, skybox, dynamic camera controls, and ImGui UI tools.

Built with **C++17**, **OpenGL**, **GLFW**, **GLEW**, **GLM**, and **ImGui**. Easily extendable with custom shaders and textures.

---

## ✨ Features

* Realistic rotation and orbital motion of planets, moons, and Saturn’s rings.
* Textured models of all planets, moons, and the Sun (stored in `assets/`).
* Skybox support (`assets/skybox/`).
* Time simulation with adjustable speed.
* Switchable orbit/fly camera (mouse + keyboard, ImGui UI).
* Import and quickly switch between custom shaders.

---

## 📦 Requirements

**Compiler:** C++17 compatible (clang / gcc / MSVC)

**Build system:** CMake ≥ 3.11

**Libraries:**

* GLFW
* GLEW
* GLM
* ImGui (included in project folder)
* stb_image.h (included automatically)

### macOS Installation

```
brew install cmake glfw glew glm
```

### Linux Installation

```
sudo apt install cmake libglfw3-dev libglew-dev libglm-dev
```

---

## 🚀 Build & Run

```
mkdir build
cd build
cmake ..
make -j4
./SolarSystem   # or ./solar-system, ./main (depends on binary name)
```

> Important: Run the executable **from the project root** to ensure access to `assets/` and `shaders/`.

---

## 📁 Project Structure

```
solar-system/
├── assets/
│   ├── earth.jpg, mars.jpg, venus.jpg, ...
│   ├── moon.jpg, sun.jpg, saturn_ring.png
│   └── skybox/
│       ├── starfield_rt.tga ... starfield_bk.tga
├── shaders/
│   ├── planet.vert/frag
│   ├── sun.vert/frag
│   └── skybox.vert/frag
├── src/
│   └── main.cpp
├── include/
│   └── stb_image.h
├── imgui/
│   └── ... (ImGui source files)
├── CMakeLists.txt
├── .gitignore
├── README.md
└── ...
```

---

## 🎮 Controls

**Orbit Camera:**

* Hold LMB + move mouse — rotate
* Mouse wheel — zoom

**Fly Camera:**

* Press **F** to toggle
* `W A S D` — movement
* Space / Shift — up/down
* Mouse — look around

**ImGui UI:**

* Select focus (planet)
* Adjust time speed
* Reset simulation

---

## 🛠 Notes

* All textures and shaders are stored in `assets/` and `shaders/`.
* Add your own shaders and load them through the existing CMake/shader loader system.
* Build system automatically generates all required binaries.
* Double‑check `.gitignore` and `CMakeLists.txt` if adding new files.

---

Enjoy exploring your own virtual Solar System! 🚀🌍
