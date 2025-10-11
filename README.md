# solar-system

# 🌞 Solar System — OpenGL (macOS + VS Code)

Учебный проект на OpenGL: вращающееся солнце (с текстурой и шейдером) + звёздный фон (skybox).  
Адаптирован под macOS и сборку через CMake. Работает с GLFW + GLEW + GLM + stb_image.


---

## Возможности

- Вращающееся по своей оси Солнце (текстура + шейдер).  
- Skybox (звёздное небо) из 6 файлов.  
- Комбинированная камера: `orbit` (мышь + колесо) и `fly` (WASD + мышь), переключение — `F`.  
- Генерация сферы в коде (не нужен внешний класс).  
- Поддержка пользовательских шейдеров (положи их в `shaders/`).

---

## Требования

- macOS (Intel / Apple Silicon)
- Homebrew (рекомендуется)
- Компилятор с поддержкой C++11 или выше (clang)
- Пакеты: `glfw`, `glew`, `glm`, `cmake`, `make`/`ninja`

---

## Установка зависимостей (macOS)

Открой терминал и выполни:

```bash
# Homebrew, если ещё не установлен:
# /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

brew update
brew install cmake glfw glew glm

---

## Cборка проекта
mkdir -p build
cd build
cmake ..
cmake --build .

## Запуск

./build/SolarSystem


## Структура проекта 

solar-system-opengl/
├── assets/
│   ├── sun.jpg
│   └── skybox/
│       ├── starfield_rt.tga
│       ├── starfield_lf.tga
│       ├── starfield_up.tga
│       ├── starfield_dn.tga
│       ├── starfield_ft.tga
│       └── starfield_bk.tga
├── include/
│   └── stb_image.h
├── shaders/
│   ├── sun.vert
│   ├── sun.frag
│   ├── skybox.vert
│   └── skybox.frag
├── src/
│   └── main.cpp
├── CMakeLists.txt
└── README.md
