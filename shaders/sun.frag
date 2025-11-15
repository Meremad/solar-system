#version 330 core
in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform sampler2D sunTex;
uniform vec3 lightPos;   
uniform vec3 viewPos;

const float EMISSION_BOOST = 1.6;
const float SPECULAR_INTENSITY = 0.6;
const float AMBIENT_FACTOR = 0.12;

void main()
{
    vec3 albedo = texture(sunTex, TexCoords).rgb;
    vec3 N = normalize(Normal);
    
    vec3 L = normalize(lightPos - FragPos);
    float diff = max(dot(N, L), 0.0);

    vec3 V = normalize(viewPos - FragPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 ambient = AMBIENT_FACTOR * albedo;
    vec3 diffuse = diff * albedo;
    vec3 specular = SPECULAR_INTENSITY * spec * vec3(1.0);

    float lum = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
    vec3 emission = albedo * pow(lum, 1.6) * EMISSION_BOOST;

    vec3 color = ambient + diffuse + specular + emission;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}
