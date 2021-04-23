#version 450 core

//layout(SG_UPDATE_FREQ_NONE, binding = 2) uniform texture2D skyboxCubeMapL;
//layout(SG_UPDATE_FREQ_NONE, binding = 3) uniform texture2D skyboxCubeMapU;
//layout(SG_UPDATE_FREQ_NONE, binding = 4) uniform texture2D skyboxCubeMapD;
//layout(SG_UPDATE_FREQ_NONE, binding = 5) uniform texture2D skyboxCubeMapF;
//layout(SG_UPDATE_FREQ_NONE, binding = 6) uniform texture2D skyboxCubeMapB;
layout(set = 0, binding = 1) uniform texture2D skyboxCubeMap;
layout(set = 0, binding = 2) uniform sampler Sampler;

layout(location = 0) in vec3 outTexCoord;
layout(location = 1) in vec3 outNormalW;

layout(location = 0) out vec4 outColor;

void main()
{
	vec2 texCoord;
	if (outNormalW.y > 0)
	{
		texCoord = -(outTexCoord.xz + vec2(1, 1)) / 2;
		outColor = texture(sampler2D(skyboxCubeMap, Sampler), texCoord);
	}
	else
	{
		outColor = vec4(outNormalW, 1.0);
	}
}