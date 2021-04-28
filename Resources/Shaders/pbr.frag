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

layout(std140, push_constant) uniform PushConsts 
{
    layout (offset = 0) mat4 model;
} pushConsts;

layout (set = 0, binding = 0) uniform samplerCube samplerIrradiance;
layout (set = 0, binding = 1) uniform samplerCube samplerPrefilter;

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

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
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

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0;
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(samplerPrefilter, R, lodf).rgb;
	vec3 b = textureLod(samplerPrefilter, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

vec3 EnvDFGLazarov_BRDF(vec3 F0, float gloss, float ndotv)
{
    vec4 p0 = vec4( 0.5745, 1.548, -0.02397, 1.301 );
    vec4 p1 = vec4( 0.5753, -0.2511, -0.02066, 0.4755 );
    vec4 t = gloss * p0 + p1;
    float bias = clamp(t.x * min(t.y, exp2(-7.672 * ndotv)) + t.z, 0.0, 1.0);
    float delta = clamp(t.w, 0.0, 1.0);
    float scale = delta - bias;
    bias *= clamp(50.0 * F0.y, 0.0, 1.0);
    return F0 * scale + bias;
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

	// IBL
	R = mat3(pushConsts.model) * R;
	vec3 reflection = prefilteredReflection(R, roughness).rgb;
	vec3 samplerN = mat3(pushConsts.model) * N;
	vec3 irradiance = texture(samplerIrradiance, samplerN).rgb;

	vec3 brdf = EnvDFGLazarov_BRDF(F0, mat.smoothness, max(dot(N, V), 0.0));
	vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

	vec3 diffuse = irradiance * mat.color;
	vec3 specular = brdf * reflection;

	// ambient part and specular part
	vec3 kd = 1.0 - F;
	kd *= 1.0 - metallic;
	vec3 ambient = (kd * diffuse + specular);

    vec3 color = Lo + ambient;

	// tone mapping
	color = Uncharted2Tonemap(color * 4.5);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	// gamma correction
	color = pow(color, vec3(1.0f / 2.2));

	outColor = vec4(color, 1.0);
}