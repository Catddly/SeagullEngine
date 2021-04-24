#version 450 core

layout (SG_UPDATE_FREQ_NONE, binding = 0) uniform samplerCube skyboxCubeMap; // combind image sampler

layout(location = 0) in vec3 inTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 color = texture(skyboxCubeMap, inTexCoord).rgb;
	outColor = vec4(color, 1.0);
}