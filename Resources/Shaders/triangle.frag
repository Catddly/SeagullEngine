#version 450 core

layout(set = 0, binding = 0) uniform sampler Sampler;
layout(set = 0, binding = 1) uniform texture2D Texture;

layout(location = 0) in vec2 outTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(sampler2D(Texture, Sampler), outTexCoord);
    //outColor = vec4(outTexCoord, 0.68, 1.0);
}
