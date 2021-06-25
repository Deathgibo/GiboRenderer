#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout (set=0, binding=4) uniform sampler sampler_border;
layout (set=0, binding=5) uniform sampler sampler_repeat;

layout (set=0, binding=6) uniform texture2D texture1;

layout (set=0, binding=7, rgba8) uniform image2D image1;

layout (set=0, binding=8) uniform samplerBuffer texelbuffer1;

layout (set=0, binding=9, rgba8) uniform imageBuffer storagetexel1;

layout(std140,set = 0, binding = 10) buffer storagebuffer
{
	mat4 matrix;
} storage1;

layout(set = 0, binding = 11) uniform dynamicbuffer
{
  float x;
} d1;

layout(	push_constant ) uniform pushuniform 
{
	float x;
} push1;

layout(set = 0, binding = 12) uniform arraybuffer
{
	float x[12]; //data in c++ side has to still be aligned by wahtever amount minimum is
} array1;

layout(input_attachment_index=0, set=0, binding=12) uniform subpassInput inputattach1;

layout(location = 2) in vec2 texCoords;

void main() 
{
	//imageLoad(input_tex, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y));
	//imageStore(output_tex, ivec2(gl_GlobalInvocationID.xy), pix);

	vec4 texColor = texture(sampler2D(texture1, sampler_border), vec2(texCoords.x, -texCoords.y));
	
	imageStore(image1, ivec2(texCoords.x, -texCoords.y), vec4(1,0,0,1));
	vec4 storageColor = imageLoad(image1, ivec2(texCoords.x, -texCoords.y));
   
   //vec4 texelFetch(samplerBuffer sampler, int P);
   
   vec4 texelColor = texelFetch(texelbuffer1, 0);
   

   imageStore(storagetexel1, int(0), vec4(1,1,1,1));
   vec4 storagetexelColor = imageLoad(storagetexel1, int(0));


   storage1.matrix = mat4(1.0);
   storage1.matrix[0][0] = 5.5f;
   vec4 storagebufferColor = vec4(storage1.matrix[0][0], 0,0,1);
	
   vec4 dynamicColor = vec4(d1.x,d1.x,d1.x,1);

   vec4 pushColor = vec4(push1.x, push1.x, push1.x, 1);

   vec4 arrayColor = vec4(array1.x[0], array1.x[1], array1.x[2], 1);
   
   outColor = arrayColor;
}
