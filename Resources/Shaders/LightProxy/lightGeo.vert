#version 450 core

#define MAX_LIGHT_COUNT 2

layout(SG_UPDATE_FREQ_PER_FRAME, binding = 0) uniform UniformBuffer
{
    mat4 model[MAX_LIGHT_COUNT];
	mat4 view;
	mat4 proj;
    vec4 lightColor[MAX_LIGHT_COUNT];
} ubo;
    
layout(location = 0) in vec3 Position;

layout(location = 0) out vec4 outLightColor;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model[gl_InstanceIndex] * vec4(Position, 1.0);
    outLightColor = ubo.lightColor[gl_InstanceIndex];
}