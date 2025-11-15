#version 330 core
in vec2 TexCoords;
in vec3 FragPos;  // позиция фрагмента в МИРОВЫХ координатах
in vec3 Normal;   // нормаль в МИРОВЫХ координатах
out vec4 FragColor;

uniform sampler2D planetTex;   // текстура планеты
uniform vec3 lightPos;         // позиция солнца в МИРОВЫХ координатах (0,0,0)
uniform float ambientK;        // например 0.10

void main()
{
    vec3 albedo = texture(planetTex, TexCoords).rgb;

    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);
    float diff = max(dot(N, L), 0.0);

    vec3 ambient = ambientK * albedo;
    vec3 diffuse = diff * albedo;

    vec3 color = ambient + diffuse;
    FragColor = vec4(color, 1.0);
}
