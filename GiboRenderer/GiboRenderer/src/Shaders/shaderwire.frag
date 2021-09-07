#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 texCoords;

layout(set = 1,binding = 1) uniform sampler2D textureImage;

void main() {
  	outColor = vec4(0.5,0.5,0.5,1);
}
