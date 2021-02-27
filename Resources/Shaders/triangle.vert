#version 450 core

layout(SG_UPDATE_FREQ_PER_FRAME, binding = 0) uniform UniformBuffer
{
    mat4 model;
	mat4 view;
	mat4 proj;
} ubo;
    
layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 TexCoord;

layout(location = 0) out vec2 outTexCoord;

void main()
{
    //vec4 null =  ubo.proj * ubo.view * ubo.model * vec4(Position, 0.1, 1.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(Position, 0.1, 1.0);
    outTexCoord = TexCoord;
}
