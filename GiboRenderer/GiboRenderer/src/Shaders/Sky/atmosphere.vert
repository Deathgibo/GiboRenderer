#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inT;
layout(location = 4) in vec3 inB;

layout(location = 2) out vec2 texCoords;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out vec3 WorldPos;
layout(location = 5) out vec4 fragLightPos;

layout(set = 0, binding = 0) uniform UniformBufferObject{
	mat4 model;
} ubo;

layout(set = 0,binding = 2) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
	mat4 lightmat;
} pv;

layout(set = 0, binding = 3) uniform InfoBuffer{
	vec4 campos; //verified
	vec4 camdirection;
	vec4 earth_center; //verified
	vec4 lightdir; //verified
	vec4 farplane;
	float earth_radius; //verified
	float atmosphere_height; //verified

} info;

#define FRUSTRUMRENDER 1

void main(){
	gl_Position = pv.proj * pv.view * ubo.model * vec4(vec3(inPosition.x, inPosition.y, inPosition.z), 1.0);
	gl_Position.y = -gl_Position.y;
	if(FRUSTRUMRENDER == 1)
	{
		//gl_Position = vec4(inPosition, 1.0);//used this if blending. dont use
		gl_Position = vec4(inPosition, 1.0).xyww; //just render quad over 
	}

	WorldPos = vec3(ubo.model * vec4(inPosition, 1.0));
	texCoords = inUV;
	fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
	fragLightPos = pv.lightmat * ubo.model * vec4(inPosition, 1.0);
	fragLightPos.y = -fragLightPos.y;

	//calculate farplane normals
	//position (-1,1) (1,1) (-1,-1) (1,-1)
	//camera matrix is its model matrix transform
	//frustum is in camera space to do camera space to world space transform
	if(FRUSTRUMRENDER == 1)
	{
		vec3 corner_vector = normalize(vec3(info.farplane.x * inPosition.x, info.farplane.y * inPosition.y, info.farplane.z));
		vec3 frustum_corner = (inverse(pv.view) * vec4(info.farplane.x * -inPosition.x, info.farplane.y * inPosition.y, info.farplane.z,0)).xyz;
		vec3 frustrum_normal = frustum_corner;// - info.campos.xyz; //this is the correct way to do this but it breaks with atmosphere, perhaps because of bad computations with campos
		fragNormal = frustrum_normal;
		//fragNormal = mat3(transpose(inverse(pv.view))) * corner_vector;
		//fragNormal.y = -fragNormal.y;
		//fragNormal.x = -fragNormal.x;
		fragNormal = -fragNormal;
	}

}
