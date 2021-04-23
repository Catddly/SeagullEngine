#version 450 core

layout(set = 1, binding = 0) uniform UniformBuffer
{
    mat4 model;
} ubo;

layout(set = 1, binding = 1) uniform CameraUbo
{
    mat4 view;
    mat4 proj;
    vec3 positionW;
} camera;

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;

layout(location = 0) out vec3 outTexCoord;
layout(location = 1) out vec3 outNormalW;

void main()
{
    vec3 pos = Position * 2;
	gl_Position = camera.proj * camera.view * ubo.model * vec4(pos, 1.0);
	outTexCoord = Position;
    outNormalW = Normal;
}