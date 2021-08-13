#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 2) in vec2 texCoords;


layout(set = 0, binding = 1) uniform sampler2D shadow_map;

layout(set = 1, binding = 1) uniform sampler2D shadow_mapmain;

void main() {
    float depth = texture(shadow_mapmain, vec2(texCoords.x, texCoords.y)).r;
	outColor = vec4(depth,depth,depth,1);
	//outColor = vec4(0,0,1,1);
}
