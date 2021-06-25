#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec3 inT;
layout(location = 5) in vec3 inB;

layout(location = 2) out vec2 texCoords;
layout(location = 3) out vec3 fragNormal;


layout(set = 0,binding = 2) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
	mat4 lightmat;
} pv;

layout(set = 0, binding = 3) uniform CloudBuffer{
	vec4 campos; 
	vec4 camdirection;
	vec4 lightdir; 
	vec4 farplane;
} info;

void main(){
	gl_Position = vec4(inPosition, 1.0).xyww; //just render quad over 
	//gl_Position.y = -gl_Position.y;
	
	texCoords = inUV;

	// calculate farplane normals
	// position (-1,1) (1,1) (-1,-1) (1,-1)
	// camera matrix is its model matrix transform
	// frustum is in camera space we want it in world space. So multiply by inverse of camera matrix to convert to world space
	
	vec3 frustum_corner = (inverse(pv.view) * vec4(info.farplane.x * -inPosition.x, info.farplane.y * inPosition.y, info.farplane.z,1)).xyz;
	vec3 frustrum_normal = frustum_corner;
	fragNormal = frustrum_normal - info.campos.xyz;
	fragNormal = -fragNormal;
}
