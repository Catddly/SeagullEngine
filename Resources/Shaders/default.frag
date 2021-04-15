#version 450 core

layout(SG_UPDATE_FREQ_PER_FRAME, binding = 1) uniform Light
{
    vec3  position;
    float intensity;
    vec3  color;
} light;

layout(location = 0) in vec2 outTexCoord;
layout(location = 1) in vec3 outNormal;
layout(location = 2) in vec3 outFragPos;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 ambient = vec3(0.1, 0.1, 0.1);
    vec3 lightDir = normalize(light.position - outFragPos);
    vec3 diffuse = max(dot(outNormal, lightDir), 0.0) * light.color * light.intensity;

    outColor = vec4((diffuse + ambient) * vec3(0.5, 0.5, 0.5), 1.0);
}
