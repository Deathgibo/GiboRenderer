#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 texCoords;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out vec3 WorldPos;
layout(location = 5) out vec4 fragLightPos;

layout(set = 1, binding = 0) uniform UniformBufferObject{
	mat4 model;
} ubo;

layout(set = 0,binding = 2) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
	mat4 lightmat;
} pv;

void main(){
	gl_Position = pv.proj * pv.view * ubo.model * vec4(vec3(inPosition.x, inPosition.y, inPosition.z), 1.0);
	gl_Position.y = -gl_Position.y;

	WorldPos = vec3(ubo.model * vec4(inPosition, 1.0));
	outColor = inColor;
	texCoords = inUV;
	fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
	fragLightPos = pv.lightmat * ubo.model * vec4(inPosition, 1.0);
	fragLightPos.y = -fragLightPos.y;
}
