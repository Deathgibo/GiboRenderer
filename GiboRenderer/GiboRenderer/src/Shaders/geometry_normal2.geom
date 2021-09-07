#version 450

layout(triangles) in;
layout(line_strip, max_vertices = 18) out;


layout(location = 5) in VS_OUT {
    mat4 model;
		mat4 view;
		mat4 proj;
} gs_in[];

layout(location = 1) in vec3 outnormals[];
layout(location = 2) in vec3 outtangents[];
layout(location = 3) in vec3 outbitangents[];

layout(location = 4) out vec3 thecolor;

void MakeLine(int index)
{
	//NORMAL
	thecolor = vec3(1,0,0);
	gl_Position = gs_in[index].proj * gs_in[index].view * gs_in[index].model * gl_in[index].gl_Position;
	gl_Position.y = -gl_Position.y;
	EmitVertex();

	vec3 norm = normalize(mat3(transpose(inverse(gs_in[index].view * gs_in[index].model))) * outnormals[index]);
	vec4 normal = gs_in[index].view * gs_in[index].model * gl_in[index].gl_Position;// +vec4(norm, 0.0f)*.3f;
	gl_Position = gs_in[index].proj * (normal + .1f*vec4(norm,0.0f));
	gl_Position.y = -gl_Position.y;
	thecolor = vec3(1,0,0);
	EmitVertex();

	//TANGENT
	gl_Position = gs_in[index].proj * gs_in[index].view * gs_in[index].model * gl_in[index].gl_Position;
	gl_Position.y = -gl_Position.y;
	thecolor = vec3(0,1,0);
	EmitVertex();

	vec3 tangent = normalize(mat3(transpose(inverse(gs_in[index].view * gs_in[index].model))) * outtangents[index]);
	gl_Position = gs_in[index].proj * (normal + .1f*vec4(tangent,0.0f));
	gl_Position.y = -gl_Position.y;
	thecolor = vec3(0,1,0);
	EmitVertex();

	//BITANGENT
	gl_Position = gs_in[index].proj * gs_in[index].view * gs_in[index].model * gl_in[index].gl_Position;
	gl_Position.y = -gl_Position.y;
	thecolor = vec3(0,0,1);
	EmitVertex();

	vec3 bitangent = normalize(mat3(transpose(inverse(gs_in[index].view * gs_in[index].model))) * outbitangents[index]);
	gl_Position = gs_in[index].proj * (normal + .1f*vec4(bitangent,0.0f));
	gl_Position.y = -gl_Position.y;
	thecolor = vec3(0,0,1);
	EmitVertex();


	EndPrimitive();
}

void main(void)
{
	MakeLine(0);
	MakeLine(1);
    MakeLine(2);
}
