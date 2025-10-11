#version 330 core
in vec3 TexCoords;
out vec4 FragColor;

uniform samplerCube skybox;

void main()
{
    vec3 col = texture(skybox, TexCoords).rgb;
    FragColor = vec4(col, 1.0);
}
