#version 450 core

layout(SG_UPDATE_FREQ_PER_FRAME, binding = 0) uniform UniformBuffer
{
    mat4 model;
	mat4 view;
	mat4 proj;
    vec3 cameraPos;
} ubo;
    
layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outFragPos;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(Position, 1.0);
    outNormal = mat3(ubo.model) * Normal;
    outTexCoord = TexCoord;
    outFragPos = vec3(ubo.model * vec4(Position, 1.0));
}