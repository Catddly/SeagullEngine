#version 450 core

#define MAX_POINT_LIGHT 2

layout(set = 1, binding = 2) uniform LightData
{
    vec3  position;
    float intensity;
    vec3  color;
    float range;
} light[MAX_POINT_LIGHT];

layout(location = 0) in vec2 outTexCoord;
layout(location = 1) in vec3 outNormalW;
layout(location = 2) in vec3 outPosW;
layout(location = 3) in vec3 outCameraPosW;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 norm = normalize(outNormalW);
    vec3 modelColor = vec3(1.0, 1.0, 1.0);

    // ambient1
    float ambientStrength1 = 0.1;
    vec3 ambient1 = ambientStrength1 * light[0].color;

    // diffuse1
    vec3 lightDir1 = normalize(light[0].position - outPosW);
    float diffuseAlbedo1 = max(dot(norm, lightDir1), 0.0);
    vec3 diffuse1 = diffuseAlbedo1 * light[0].color;

    // specular1
    float specularStrength1 = 0.5;
    vec3 viewDir1 = normalize(outCameraPosW - outPosW);
    vec3 reflectDir1 = reflect(-lightDir1, norm);
    float spec1 = pow(max(dot(viewDir1, reflectDir1), 0.0), 64);
    vec3 specular1 = specularStrength1 * spec1 * light[0].color;

    // attenuation1
    float distance2_1 = pow(length(light[0].position - outPosW), 2);
    float attenuation1 = pow(max(1.0 - pow(distance2_1 * light[0].range, 2), 0), 2);

    // ambient2
    float ambientStrength2 = 0.1;
    vec3 ambient2 = ambientStrength2 * light[1].color;

    // diffuse2
    vec3 lightDir2 = normalize(light[1].position - outPosW);
    float diffuseAlbedo2 = max(dot(norm, lightDir2), 0.0);
    vec3 diffuse2 = diffuseAlbedo2 * light[1].color;

    // specular2
    float specularStrength2 = 0.5;
    vec3 viewDir2 = normalize(outCameraPosW - outPosW);
    vec3 reflectDir2 = reflect(-lightDir2, norm);
    float spec2 = pow(max(dot(viewDir2, reflectDir2), 0.0), 64);
    vec3 specular2 = specularStrength2 * spec2 * light[1].color;

    // attenuation2
    float distance2_2 = pow(length(light[1].position - outPosW), 2);
    float attenuation2 = pow(max(1.0 - pow(distance2_2 * light[1].range, 2), 0), 2);

    vec3 result1 = (diffuse1 + ambient1 + specular1) * modelColor * attenuation1 * light[0].intensity;
    vec3 result2 = (diffuse2 + ambient2 + specular2) * modelColor * attenuation2 * light[1].intensity;

    //vec3 light = GetLighting(outPosW, outCameraPosW, norm);
    vec3 light = result1 + result2;
    outColor = vec4(light, 1.0);
}
