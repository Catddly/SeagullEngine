#version 450 core

layout(set = 1, binding = 0) uniform CameraUbo
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
    mat3 v = mat3(camera.view);
    mat4 view = mat4(v);
	gl_Position = camera.proj * view * vec4(pos, 1.0);
	outTexCoord = pos;
    outNormalW = Normal;
}