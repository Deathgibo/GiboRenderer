#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 2) in vec2 texCoords;

layout(set = 1, binding = 0) uniform ColorBuffer
{
	vec3 color;
} object_color;

layout(set = 0, binding = 3) uniform sampler2D uvtexture;

void main() 
{
	vec3 color = texture(uvtexture, vec2(texCoords.x, -texCoords.y)).rgb;
	outColor = vec4(color, 1);
	
	
	//outColor = vec4(object_color.color, 1);
}
