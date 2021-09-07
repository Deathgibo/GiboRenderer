#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 1) out vec2 texuv;


void main(){
	gl_Position = vec4(inPosition, 1.0);

	texuv = inUV;
}
