#version 450 core

//vec3 skyboxVert[8] = {
//	vec3(-1, -1, -1),
//	vec3(-1, -1, 1),
//	vec3(-1, 1, -1),
//	vec3(1, -1, -1),
//	vec3(-1, 1, 1),
//	vec3(1, 1, -1),
//	vec3(1, 1, 1),
//	vec3(1, -1, 1)
//};

layout(location = 0) in vec3 Position;

void main()
{
	gl_Position = vec4(Position, 0.0);
}