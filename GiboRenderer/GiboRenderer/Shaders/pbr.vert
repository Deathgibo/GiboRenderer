#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec3 inT;
layout(location = 5) in vec3 inB;

layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 texCoords;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out vec3 WorldPos;
layout(location = 5) out vec4 fragLightPos;
layout(location = 6) out vec4 fragV;
layout(location = 7) out vec3 fragTangent;
layout(location = 8) out vec3 fragBiTangent;
layout(location = 9) out mat3 TBN;

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
	fragTangent = mat3(transpose(inverse(ubo.model))) * inT;
	fragBiTangent = mat3(transpose(inverse(ubo.model))) * inB;

	fragLightPos = pv.lightmat * ubo.model * vec4(inPosition, 1.0);
	fragLightPos.y = -fragLightPos.y;
	fragV = pv.view * ubo.model * vec4(vec3(inPosition.x, inPosition.y, inPosition.z), 1.0);
	fragV.y = -fragV.y;

    //Gram-Schmidt process
    vec3 T = normalize(vec3(ubo.model * vec4(inT, 0.0)));
	//vec3 B = normalize(vec3(ubo.model * vec4(inB, 0.0)));
    vec3 N = normalize(vec3(ubo.model * vec4(inNormal, 0.0)));
    T = normalize(T - dot(T,N) * N);
    vec3 B = cross(N, T);

    TBN = mat3(T, B, N);
}
