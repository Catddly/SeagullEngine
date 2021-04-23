#version 450 core

layout(SG_UPDATE_FREQ_NONE, binding = 0) uniform sampler Sampler;
layout(SG_UPDATE_FREQ_NONE, binding = 1) uniform texture2D skyboxCubeMapL;
layout(SG_UPDATE_FREQ_NONE, binding = 2) uniform texture2D skyboxCubeMapD;
layout(SG_UPDATE_FREQ_NONE, binding = 3) uniform texture2D skyboxCubeMapR;
layout(SG_UPDATE_FREQ_NONE, binding = 4) uniform texture2D skyboxCubeMapF;
layout(SG_UPDATE_FREQ_NONE, binding = 5) uniform texture2D skyboxCubeMapU;
layout(SG_UPDATE_FREQ_NONE, binding = 6) uniform texture2D skyboxCubeMapB;

layout(location = 0) in vec3 outTexCoord;
layout(location = 1) in vec3 outNormalW;

layout(location = 0) out vec4 outColor;

void main()
{
	vec2 texCoord;
	if (outNormalW.y > 0)
	{
		texCoord = -(outTexCoord.xz + vec2(1, 1)) / 2;
		texCoord.x = texCoord.x - 1;
		if (texCoord.x != 0)
		{
			texCoord.x = texCoord.x * -1;
		}
		outColor = texture(sampler2D(skyboxCubeMapR, Sampler), texCoord);
	}
	else if (outNormalW.z > 0)
	{
		texCoord = (outTexCoord.yx + vec2(1, 1)) / 2;
		texCoord.x = texCoord.x - 1;
		if (texCoord.x < 0)
		{
			texCoord.x = texCoord.x * -1;
		}
		if (texCoord.x == 1 && texCoord.y == 0)
		{
			texCoord = vec2(0, 1);
		}
		if (texCoord.x == 0 && texCoord.y == 1)
		{
			texCoord = vec2(1, 0);
		}
		outColor = texture(sampler2D(skyboxCubeMapD, Sampler), texCoord);
	}
	else if (outNormalW.z < 0)
	{
		texCoord = (outTexCoord.yx + vec2(1, 1)) / 2;
		texCoord.y = texCoord.y - 1;
		if (texCoord.y < 0)
		{
			texCoord.y = texCoord.y * -1;
		}
		outColor = texture(sampler2D(skyboxCubeMapU, Sampler), texCoord);
	}
	else if (outNormalW.y < 0)
	{
		texCoord = -(outTexCoord.xz + vec2(1, 1)) / 2;
		outColor = texture(sampler2D(skyboxCubeMapL, Sampler), texCoord);
	}
	else if (outNormalW.x > 0)
	{
		texCoord = -(outTexCoord.yz + vec2(1, 1)) / 2;
		outColor = texture(sampler2D(skyboxCubeMapF, Sampler), texCoord);
	}
	else if (outNormalW.x < 0)
	{
		texCoord = -(outTexCoord.yz + vec2(1, 1)) / 2;
		texCoord.x = texCoord.x - 1;
		if (texCoord.x < 0)
		{
			texCoord.x = texCoord.x * -1;
		}
		outColor = texture(sampler2D(skyboxCubeMapB, Sampler), texCoord);
	}
}