#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inT;
layout(location = 4) in vec3 inB;

layout(set = 0, binding = 1) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
} pv;

//pancake all values that would get clipped behind near plane so all shadow casters are included. only works with sun directional light
void main()
{
	gl_Position = pv.proj * pv.view * vec4(vec3(inPosition.x, inPosition.y, inPosition.z), 1.0);
	gl_Position.y = -gl_Position.y;
	gl_PointSize = 15;
}
