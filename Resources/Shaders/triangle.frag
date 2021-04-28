#version 450 core

layout(SG_UPDATE_FREQ_NONE, binding = 0) uniform sampler Sampler;
layout(SG_UPDATE_FREQ_NONE, binding = 1) uniform texture2D Texture;

layout(location = 0) in vec2 outTexCoord;
//layout(location = 1) in vec3 outN;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(sampler2D(Texture, Sampler), outTexCoord);
    //outColor = vec4(0.4, 0.4, 0.4, 1.0);
}
