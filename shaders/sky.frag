#version 330 core
out vec4 FragColor;
in vec3 worldPos;
uniform float time;

// simple procedural starfield (cheap, no textures)
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vec3 dir = normalize(worldPos);
    float u = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
    float v = asin(dir.y) / 3.14159265 + 0.5;
    vec2 uv = vec2(u * 512.0, v * 256.0);

    // base dark sky
    vec3 base = vec3(0.01, 0.02, 0.06);

    // generate bright tiny stars via a threshold
    float s = rand(floor(uv));
    float star = step(0.9994, s); // tune threshold -> density
    // twinkle
    float tw = 0.5 + 0.5 * sin(time * 2.0 + uv.x * 0.01);
    star *= tw;

    // soft glow tiny
    float glow = pow(smoothstep(0.995, 1.0, s) , 3.0) * 2.5;

    vec3 col = base + vec3(1.0, 0.95, 0.8) * (star + glow);
    FragColor = vec4(col, 1.0);
}
