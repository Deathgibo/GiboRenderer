#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normals;
layout(location = 2) in vec3 vcolor;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec3 inT;
layout(location = 5) in vec3 inB;

layout(location = 1) out vec3 outnormals;
layout(location = 2) out vec3 outtangents;
layout(location = 3) out vec3 outbitangents;

layout(set = 1, binding = 0) uniform UniformBufferObject{
	mat4 model;
} ubo;

layout(set = 0,binding = 2) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
} pv;

layout(location = 5) out VS_OUT {
    mat4 model;
		mat4 view;
		mat4 proj;
} gs_out;

void main(void)
{
	gl_Position = vec4(position, 1.0);

	outnormals = normals;
	outtangents = inT;
	outbitangents = inB;

	gs_out.model = ubo.model;
	gs_out.view = pv.view;
	gs_out.proj = pv.proj;
}
