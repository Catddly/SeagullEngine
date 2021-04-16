#version 450 core

layout(SG_UPDATE_FREQ_PER_FRAME, binding = 0) uniform UniformBuffer
{
    mat4 model;
	mat4 view;
	mat4 proj;
    vec3 cameraPos;
} ubo;

layout(SG_UPDATE_FREQ_PER_FRAME, binding = 1) uniform Light
{
    vec3  position;
    float intensity;
    vec3  color;
    float range;
} light[2];

layout(location = 0) in vec2 outTexCoord;
layout(location = 1) in vec3 outNormal;
layout(location = 2) in vec3 outFragPos;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 norm = normalize(outNormal);

    // ambient1
    float ambientStrength1 = 0.1;
    vec3 ambient1 = ambientStrength1 * light[0].color;

    // diffuse1
    vec3 lightDir1 = normalize(light[0].position - outFragPos);
    float diffuseAlbedo1 = max(dot(norm, lightDir1), 0.0);
    vec3 diffuse1 = diffuseAlbedo1 * light[0].color;

    // specular1
    float specularStrength1 = 0.5;
    vec3 viewDir1 = normalize(ubo.cameraPos - outFragPos);
    vec3 reflectDir1 = reflect(-lightDir1, norm);
    float spec1 = pow(max(dot(viewDir1, reflectDir1), 0.0), 64);
    vec3 specular1 = specularStrength1 * spec1 * light[0].color;

    // attenuation1
    float distance2_1 = pow(length(light[0].position - outFragPos), 2);
    float attenuation1 = pow(max(1.0 - pow(distance2_1 * light[0].range, 2), 0), 2);

    vec3 result1 = (diffuse1 + ambient1 + specular1) * vec3(0.74, 0.67, 0.18) * attenuation1;

    // ambient2
    float ambientStrength2 = 0.1;
    vec3 ambient2 = ambientStrength2 * light[1].color;

    // diffuse2
    vec3 lightDir2 = normalize(light[1].position - outFragPos);
    float diffuseAlbedo2 = max(dot(norm, lightDir2), 0.0);
    vec3 diffuse2 = diffuseAlbedo2 * light[1].color;

    // specular2
    float specularStrength2 = 0.5;
    vec3 viewDir2 = normalize(ubo.cameraPos - outFragPos);
    vec3 reflectDir2 = reflect(-lightDir2, norm);
    float spec2 = pow(max(dot(viewDir2, reflectDir2), 0.0), 64);
    vec3 specular2 = specularStrength2 * spec2 * light[1].color;

    // attenuation2
    float distance2_2 = pow(length(light[1].position - outFragPos), 2);
    float attenuation2 = pow(max(1.0 - pow(distance2_2 * light[1].range, 2), 0), 2);

    vec3 result2 = (diffuse2 + ambient2 + specular2) * vec3(0.74, 0.67, 0.18) * attenuation2;

    outColor = vec4(result1 + result2, 1.0);
}
