#version 450 core

layout(location = 0) in vec2 outTexCoord;
layout(location = 1) in vec3 outNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
