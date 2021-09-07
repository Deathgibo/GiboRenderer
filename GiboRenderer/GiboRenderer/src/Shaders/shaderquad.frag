#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;
layout(location = 1) in vec2 texuv;

layout(binding = 1) uniform sampler2D tex;

void main() {
    //vec4 textureColor = imageLoad(texSampler, ivec2(400, 800));
    vec4 textureColor = texture(tex, vec2(texuv.x, texuv.y)); //maybe flip this?

    outColor = vec4(textureColor.xyz, 1);
	//outColor = vec4(1,0,1, 1);
}
