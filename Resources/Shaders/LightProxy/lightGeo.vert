#version 450 core

#define MAX_LIGHT_COUNT 2

layout(set = 1, binding = 0) uniform CameraUbo
{
    mat4 view;
    mat4 proj;
    vec3 positionW;
} camera;

layout(set = 1, binding = 1) uniform LightUbo
{
    mat4 model[MAX_LIGHT_COUNT];
    vec4 lightColor[MAX_LIGHT_COUNT];
} lightUbo;
    
layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;

layout(location = 0) out vec4 outLightColor;

void main()
{
    gl_Position = camera.proj * camera.view * lightUbo.model[gl_InstanceIndex] * vec4(Position, 1.0);
    outLightColor = lightUbo.lightColor[gl_InstanceIndex];
}