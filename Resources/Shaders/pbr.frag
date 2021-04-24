#version 450 core

#define MAX_POINT_LIGHT 2
#define MIN_REFLECTIVITY 0.04
#define PI 3.1415926535897932384626433832795

struct BRDF
{
	vec3 diffuse;
	vec3 specular;
	float roughness; // 1.0 - smoothness
};

layout(set = 1, binding = 2) uniform LightData
{
    vec3  position;
    float intensity;
    vec3  color;
    float range;
} light[MAX_POINT_LIGHT];

layout(set = 1, binding = 3) uniform MaterialData
{
    vec3  color;
    float metallic;
    float smoothness;
} mat;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormalW;
layout(location = 2) in vec3 inPosW;
layout(location = 3) in vec3 inCameraPosW;

layout(location = 0) out vec4 outColor;

float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2) / (PI * denom * denom); 
}

vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); // ?? should be dotVH?
}

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

float CalcAtten(vec3 lightPos, vec3 posW, float range)
{
	float distance2 = pow(length(lightPos - posW), 2);
	float attenuation = pow(max(1.0 - pow(distance2 * range, 2), 0), 2);
	return attenuation;
}

vec3 specularBRDF(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	vec3 color = vec3(0.0);
	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, F0);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001); // specular brdf
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic); // kd for lambert diffuse section, F is ks
		color += (kD * mat.color / PI + spec);
	}

	return color;
}

void main()
{
    vec3 N = normalize(inNormalW);
    vec3 V = normalize(inCameraPosW - inPosW);
    vec3 R = reflect(-V, N);

    float metallic = mat.metallic;
	float roughness = 1.0 - mat.smoothness;

    vec3 F0 = vec3(0.04); 
	F0 = mix(F0, mat.color, metallic);
    
    // render equation
    vec3 Lo = vec3(0.0);
	{
		// light 1 begin
		vec3 L = normalize(light[0].position - inPosW);
		// light attenuation
		float attenuation = CalcAtten(light[0].position, inPosW, light[0].range);
		vec3 lightRadiance = light[0].color * attenuation * light[0].intensity;
		// precalculate vectors and dot products	
		vec3 H = normalize (V + L);
		float dotNH = clamp(dot(N, H), 0.0, 1.0);
		float dotNV = clamp(dot(N, V), 0.0, 1.0);
		float dotNL = clamp(dot(N, L), 0.0, 1.0);

		Lo += specularBRDF(L, V, N, F0, metallic, roughness) * lightRadiance * dotNL;
		// light 1 end
	}

	{
		// light 2 begin
		vec3 L = normalize(light[1].position - inPosW);
		// light attenuation
		float attenuation = CalcAtten(light[1].position, inPosW, light[1].range);
		vec3 lightRadiance = light[1].color * attenuation * light[1].intensity;
		// precalculate vectors and dot products	
		vec3 H = normalize (V + L);
		float dotNH = clamp(dot(N, H), 0.0, 1.0);
		float dotNV = clamp(dot(N, V), 0.0, 1.0);
		float dotNL = clamp(dot(N, L), 0.0, 1.0);

		Lo += specularBRDF(L, V, N, F0, metallic, roughness) * lightRadiance * dotNL;
		// light 2 end
	}

	// fixed ambient
	vec3 ambient = vec3(0.05, 0.05, 0.05);
    vec3 color = Lo + ambient;

	outColor = vec4(color, 1.0);
}