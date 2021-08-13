#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inT;
layout(location = 4) in vec3 inB;

layout(	push_constant ) uniform VertexMatricesBuffer
{
	mat4 model;
} ubo;

layout(set = 1,binding = 2) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
} pv;

layout(set = 1,binding = 3) uniform nearBuffer{
	float near_plane;
} nn;

//pancake all values that would get clipped behind near plane so all shadow casters are included. only works with sun directional light
void main()
{
	vec4 light_space = pv.view * ubo.model * vec4(vec3(inPosition.x, inPosition.y, inPosition.z), 1.0);
	gl_Position = pv.proj * vec4(light_space.x, light_space.y, min(light_space.z, -nn.near_plane-.01), light_space.w); 
	gl_Position.y = -gl_Position.y;
}
