#version 450 core

layout(set = 1, binding = 0) uniform CameraUbo
{
    mat4 view;
    mat4 proj;
    vec3 positionW;
} camera;

layout(std140, push_constant) uniform PushConsts 
{
    layout (offset = 0) mat4 model;
} pushConsts;

layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;

layout(location = 0) out vec3 outTexCoord;
layout(location = 1) out vec3 outNormal;

void main()
{
    mat3 v = mat3(camera.view);
    mat4 view = mat4(v);
	gl_Position = camera.proj * view * pushConsts.model * vec4(Position, 1.0);
	outTexCoord = Position;
    outNormal = Normal;
}