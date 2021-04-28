#version 450 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;

layout(std140, push_constant) uniform PushConsts 
{
    layout (offset = 0) mat4 mvp;
	layout (offset = 64) float roughness;
	layout (offset = 68) float numSamples;
} pushConsts;

layout (location = 0) out vec3  outTexCoord;
layout (location = 1) out float outRoughness;
layout (location = 2) out float outNumSamples;

void main()
{
    outTexCoord = Position;
    outRoughness = pushConsts.roughness;
    outNumSamples = pushConsts.numSamples;

    vec3 pos = Position;
    pos.y = pos.y * -1;
    gl_Position = pushConsts.mvp * vec4(pos, 1.0);
}