#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inT;
layout(location = 4) in vec3 inB;

layout(location = 2) out vec2 texCoords;

layout(	push_constant ) uniform VertexMatricesBuffer
{
	mat4 model;
} ubo;

void main()
{
	gl_Position = ubo.model * vec4(inPosition, 1.0);

	texCoords = inUV;
}
