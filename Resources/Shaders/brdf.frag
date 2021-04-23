#version 450 core

#define MAX_POINT_LIGHT 2
#define MIN_REFLECTIVITY 0.04

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

layout(location = 0) in vec2 outTexCoord;
layout(location = 1) in vec3 outNormalW;
layout(location = 2) in vec3 outPosW;
layout(location = 3) in vec3 outCameraPosW;

layout(location = 0) out vec4 outColor;

void main()
{
    float diffuseAttenuation = (1.0 - MIN_REFLECTIVITY) - mat.metallic * (1.0 - MIN_REFLECTIVITY); // clamp 0 - 1 to 0 - 0.96
    
    BRDF brdf;
    brdf.diffuse = mat.color * diffuseAttenuation;
    brdf.specular = mix(vec3(MIN_REFLECTIVITY), mat.color, mat.metallic);
    brdf.roughness = 1.0 - mat.smoothness;

	vec3 norm = normalize(outNormalW);
    vec3 viewDir = normalize(outCameraPosW - outPosW);
    vec3 lightRes = vec3(0.0);

    {
        vec3 lightDir = normalize(light[0].position - outPosW);

        float lightAlbedo = max(dot(norm, lightDir), 0.0);
        vec3  lighting = lightAlbedo * mat.color * light[0].intensity * light[0].color;

        // brdf specular
        float specularStrength;
        vec3 h = normalize(lightDir + viewDir); // half vector
        float nh2 = pow(max(dot(norm, h), 0.0), 2);
        float lh2 = pow(max(dot(lightDir, h), 0.0), 2);
        float r2 = pow(brdf.roughness, 2);
        float d2 = pow(nh2 * (r2 - 1.0) + 1.00001, 2);
        float normalization = brdf.roughness * 4.0 + 2.0;
        specularStrength = r2 / (d2 * max(0.1, lh2) * normalization);

        vec3 brdfRes = specularStrength * brdf.specular + brdf.diffuse;

        // attenuation
        float distance2 = pow(length(light[0].position - outPosW), 2);
        float attenuation = pow(max(1.0 - pow(distance2 * light[0].range, 2), 0), 2);

        lightRes += (lighting * brdfRes) * attenuation;
    }

    {
        vec3 lightDir = normalize(light[1].position - outPosW);

        float lightAlbedo = max(dot(norm, lightDir), 0.0);
        vec3  lighting = lightAlbedo * mat.color * light[1].intensity * light[1].color;

        // brdf specular
        float specularStrength;
        vec3 h = normalize(lightDir + viewDir); // half vector
        float nh2 = pow(max(dot(norm, h), 0.0), 2);
        float lh2 = pow(max(dot(lightDir, h), 0.0), 2);
        float r2 = pow(brdf.roughness, 2);
        float d2 = pow(nh2 * (r2 - 1.0) + 1.00001, 2);
        float normalization = brdf.roughness * 4.0 + 2.0;
        specularStrength = r2 / (d2 * max(0.1, lh2) * normalization);

        vec3 brdfRes = specularStrength * brdf.specular + brdf.diffuse;

        // attenuation
        float distance2 = pow(length(light[1].position - outPosW), 2);
        float attenuation = pow(max(1.0 - pow(distance2 * light[1].range, 2), 0), 2);

        lightRes += (lighting * brdfRes) * attenuation;
    }

	outColor = vec4(lightRes, 1.0);
	//outColor = vec4(vec3(mat.color), 1.0);
}