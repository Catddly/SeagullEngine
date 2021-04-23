#version 450 core

layout (set = 0, binding = 0) uniform texture2D skyboxCubeMap[6];
layout (set = 0, binding = 1) uniform sampler Sampler;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(1.0, 0.5, 0.0, 1.0);
}