#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // В мировые координаты
    FragPos = vec3(model * vec4(aPos, 1.0));
    // Пересчитываем нормаль правильно с учётом поворотов/масштаба!
    Normal  = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTex;

    // Корректная позиция вершины
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
