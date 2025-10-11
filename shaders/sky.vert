#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal; // unused
layout(location = 2) in vec2 aTex;    // unused

out vec3 worldPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // put sky sphere around origin in world coords, but we will center it at camera in main.cpp if needed
    vec4 pos = model * vec4(aPos, 1.0);
    worldPos = pos.xyz;
    // render sky with position transformed by view/projection but keep it always 'behind'
    // Note: we intentionally don't apply model->view translation so stars appear infinitely far
    gl_Position = projection * (view * vec4(aPos, 1.0));
}
