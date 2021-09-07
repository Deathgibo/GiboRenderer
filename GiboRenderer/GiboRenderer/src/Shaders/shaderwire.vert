#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 texCoords;

layout(set = 1, binding = 0) uniform UniformBufferObject{
	mat4 model;
} ubo;

layout(set = 0,binding = 2) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
} pv;

void main(){
	gl_Position = pv.proj * pv.view * ubo.model * vec4(vec3(inPosition.x, inPosition.y, inPosition.z), 1.0);
	gl_Position.y = -gl_Position.y;	
	outColor = inColor;
	texCoords = inUV;
}
