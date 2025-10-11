#version 330 core
layout(location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aPos;
    // удаляем трансляцию из матрицы вида, чтобы небо всегда стояло за камерой
    mat4 rotView = mat4(mat3(view));
    gl_Position = projection * rotView * vec4(aPos, 1.0);
}
