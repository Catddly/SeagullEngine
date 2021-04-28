#version 450 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;

layout(std140, push_constant) uniform PushConsts 
{
    layout (offset = 0) mat4 mvp;
    layout (offset = 64) float deltaPhi;
	layout (offset = 68) float deltaTheta;
} pushConsts;

layout (location = 0) out vec3  outTexCoord;
layout (location = 1) out float outDeltaPhi;
layout (location = 2) out float outDeltaTheta;

void main()
{
    outTexCoord = Position;
    outDeltaPhi = pushConsts.deltaPhi;
    outDeltaTheta = pushConsts.deltaTheta;

    vec3 pos = Position;
    pos.y = pos.y * -1;
    gl_Position = pushConsts.mvp * vec4(pos, 1.0);
}