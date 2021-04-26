#version 450 core

layout (SG_UPDATE_FREQ_NONE, binding = 0) uniform samplerCube skyboxCubeMap; // combind image sampler

layout(location = 0) in vec3 inTexCoord;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main()
{
	vec3 color = texture(skyboxCubeMap, inTexCoord).rgb;

	// tone mapping
	color = Uncharted2Tonemap(color * 4.5);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	// gamma correction
	color = pow(color, vec3(1.0f / 2.2));

	outColor = vec4(color, 1.0);
}