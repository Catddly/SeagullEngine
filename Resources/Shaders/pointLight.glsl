//#version 450 core

#define MAX_POINT_LIGHTS 2

layout(SG_UPDATE_FREQ_PER_FRAME, binding = 2) uniform PointLight
{
    vec3  position;
    float intensity;
    vec3  color;
    float range;
} light[MAX_POINT_LIGHTS];

vec3 GetAmbient(int lightIndex)
{
	float ambientStrength = 0.1;
    vec3  ambient = ambientStrength * light[lightIndex].color;
	return ambient;
}

vec3 GetDiffuse(int lightIndex, vec3 positionW, vec3 norm)
{
    vec3 lightDir = normalize(light[lightIndex].position - positionW);
    float diffuseAlbedo = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diffuseAlbedo * light[lightIndex].color;
    return diffuse;
}

vec3 GetSpecular(int lightIndex, vec3 positionW, vec3 cameraPosW, vec3 norm)
{
    float specularStrength = 0.5;
    vec3 viewDir = normalize(cameraPosW - positionW);
    vec3 lightDir = normalize(light[lightIndex].position - positionW);

    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    vec3 specular = specularStrength * spec * light[lightIndex].color;
    return specular;
}

float GetAttenuation(int lightIndex, vec3 positionW)
{
    float distance2 = pow(length(light[lightIndex].position - positionW), 2);
    float attenuation = pow(max(1.0 - pow(distance2 * light[lightIndex].range, 2), 0), 2);
    return attenuation;
}

vec3 GetLighting(vec3 fragPosW, vec3 cameraPosW, vec3 norm)
{
    vec3 result = vec3(0, 0, 0);
    vec3 modelColor = vec3(1.0, 1.0, 1.0);

    for (int i = 0; i < MAX_POINT_LIGHTS; i++)
    {
        vec3 ambient = GetAmbient(i);
        vec3 diffuse = GetDiffuse(i, fragPosW, norm);
        vec3 specular = GetSpecular(i, fragPosW, cameraPosW, norm);
        float attenuation = GetAttenuation(i, fragPosW);

        result += (ambient + diffuse + specular) * modelColor * attenuation * light[i].intensity;
    }
    return result;
}