#version 450 core

layout(set = 1, binding = 0) uniform CameraUbo
{
    mat4 view;
    mat4 proj;
    vec3 positionW;
} camera;
    

layout(set = 1, binding = 1) uniform UniformBuffer
{
    mat4 model;
} ubo;
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormalW;
layout(location = 2) out vec3 outPosW;
layout(location = 3) out vec3 outCameraPosW;

void main()
{
    gl_Position = camera.proj * camera.view * ubo.model * vec4(Position, 1.0);
    outNormalW = mat3(ubo.model) * Normal;
    outTexCoord = TexCoord;
    outPosW = vec3(ubo.model * vec4(Position, 1.0));
    outCameraPosW = camera.positionW;
}