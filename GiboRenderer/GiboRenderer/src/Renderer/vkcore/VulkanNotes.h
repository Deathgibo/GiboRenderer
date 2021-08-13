#pragma once

/*

--RESOLUTION/WindowSize/Etc--
	Okay so in terms of all of this it all starts with your render attachment and swapchain resolutions. Then these go in a framebuffer which can have a different resolution. Then
	the pipeline viewport specifies the transformation of the attachments to the framebuffer, and the scissor specifies what pixels won't get discared in the framebuffer. For instance you
	can have a color attachment 800x800 and framebuffer 400x400. if you set the viewport as 200x200 that 800x800 image will get squashes and show in half the framebuffer. Then finally
	when you begin a renderpass you specify the area where the attachments will be effected (it should be same as attachments for best performance) not anything to do with framebuffer.
	Then on top of this you have the actual window but I dont vulkan has any idea about a window, so I think its update to the operating system for that.

	Projection Matrix: the projection matrix takes in a aspect ratio, this represents the aspect ratio of our virtual camera and its just a ratio not any width or height. For the most part
	this doesn't matter except we really do want the aspect ratio to match the ratio of our monitor so they align and you don't waste rendering space or have stretching

	Window Size: Window size is just the window size so not much here, but its important to note most of the time the swapchain image size will be the size of the window. However
	if you change the window you have to change the swapchain

	Resolution: This is the resolution of our render targets, this will matters a lot of course because its the number of pixels that get drawn on, the more you have the more precise it is. This 
	doesn't have to match window size or anything, but of course you need to present the swap chain image. So if it differs you have to get that information over to the swapchain probably by
	drawing over a quad.

--SHADER MEMORY--
	GPU memory is important and I think it will be useful to test different methods. Like trying out different descriptor types, like push constants, uniform buffer, dynamic buffer.
	As well as is it better to map and unmap them on cpu, keep them mapped on cpu, or create staging buffer and copy cpu to gpu every frame?

--USAGE/LAYOUT/FORMAT--
	Its important to know the usage and layouts and formats of your buffers and images when creating and using them throughout vulkan. Usage is simple if you look at flags.
	Layouts are as well just make sure you transition them. 
	Formats are important and its still confusing but I think I need to stick to non SRGB formats for it to be in linear space for pbr and hdr rendering. Also make sure formats
	are higher precision if needed. As for the swapchain format I don't know maybe it can be srb, will have to test
	*There are some mandorty formats vulkan requires so can stick to those as defaults

--FRAMES PER FLIGHT--
	For frames in flight you have multiple gpu datas like buffers and images including render targets. So you need multiple descriptosets for these which luckily don't change because memory is binded.
	Pipelines luckily don't change based off new gpu data. Commandbuffers have to change though. So really if you change gpu data your only going to have to re-create command buffers. Also
	your render attachments need to be unique as well so you don't overwrite them. Luckily its just new image/imageviews and framebuffer. Besides this you just re-create the commandbuffers.
	So really if you make a change you should just alert all command buffers to be recreated when they are not currently in flight. maybe have a fence connected too each.

--QUEUES--
	Queue are just something you submit command buffers to that actually put the commands on a queue for the gpu to run
	Queues can handle different operations on different hardware of the gpu so they have different types
	Families are just queues that have the same exact property. Really theres different queues for multi-threading purposes
	Graphics: For creating graphics pipelines and drawing (vkCmdDraw)
	Compute: For creating compute pipelines and dispatching compute shaders (vkCmdDispatch)
	Transfer: Used for very fast memory-copying operations (vkCmdCopy)
	Sparse: Allows for additional memory management features (vkQueueBindSparse)
	Present: Used for presenting Command

--TRANSFER--
	In vulkan whenever you want copy buffer or images you do it through the transfer operation. There is a pipeling stage for transfer as well as you have to make sure
	your memory has the correct transfer usage, transfer layout, and even transfer memory access. usually when you transfer you need memory barriers. One use is to copy a staging
	buffer to a device_local buffer.

--COMMAND BUFFER / POOL NOTES--
	When one or many command buffers are submitted for execution the API user has to guarantee to not free the command buffers,
	or any of the resources referenced in the command buffers, before they have been fully consumed by the GPU.

	Recreate the main command buffer every frame. other command buffers that are static can just create at the start.
	maybe research second command buffers, multithreading, and ways to recreate command buffers for efficiently

	recording->executing->done
	its best to minimize command buffers and have larger ones so gpu doesn't stall

	VK_COMMAND_POOL_CREATE_TRANSIENT_BIT specifies that command buffers allocated from the pool will be short-lived, meaning that they will be reset or freed in a relatively short timeframe.
	VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows any command buffer allocated from a pool to be individually reset to the initial state
	VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  puts it in invalid state start submission, so you have to make sure to reset it first before rerecording


--DESCRIPTORS--
	VK_DESCRIPTOR_TYPE_SAMPLER
	used for the shader to sample an image, deals with min/max filtering, dealing with out of boundaries, anisotropy, and mip mapping.
	*This doesn't actually have an image just can be used on images like a sampled_image.
		layout (set=m, binding=n) uniform sampler <variable name>

	VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
	It seems this is just an image you can pass to shaders, but to even access it you need to pass in a sampler. Doesn't seem that useful except maybe for optimiziation?
	You can pass in multiple textures but one sampler or vice versa. *You can read the same texture with different samplers.
	When creating make sure you use VK_IMAGE_USAGE_SAMPLED_BIT and format supports sampled_bit (function to check in vkcoreImageObject). Also make sure image layout is in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
		layout (set=m, binding=n) uniform texture2D <variable name>
		vec4 texture2D(sampler2D sampler, vec2 coord, float bias (can skip))

	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	This is the most common one to use and its just a sampler and sampled_image in one. Mostly have better performance than using them seperately.
		layout (set=m, binding=n) uniform sampler2D <variable name>
		imageLoad(), imageStore(), atomics

	VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	So this is an image, and most importantly it allows us to write to it in the shader.
	When creating make sure you use VK_IMAGE_USAGE_STORAGE_BIT and format supports storage_bit(function to check in vkcoreImageObject). Also make sure image layout is in VK_IMAGE_LAYOUT_GENERAL.
		layout (set=m, binding=n) uniform image2D <variable name>

	VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
	Can read data from a BUFFER, but its formated as a texel with one,two,three,four components.
	Good becuase you can access data that is much larger than the data provided from usual images.
	Make sure when you create the buffer your using VK_IMAGE_USAGE_UNIFORM_TEXEL_BUFFER_BIT also make sure format supports it.
	*Also you need to create a buffer view
		layout (set=m, binding=n) uniform samplerBuffer <variable name>

	VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
	just like the texel buffer except you can load larger images, write to them, and do atomic operations.
	Also needs to be created with a bufferview.
	Make sure when you create the buffer your using VK_IMAGE_USAGE_STORAGE_TEXEL_BUFFER_BIT also make sure format supports it.
		layout (set=m, binding=n) uniform imageBuffer <variable name>

	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	Basic storage for things that probably don't change too often. Use VK_BUFFER_USAGE_UNIFORM_BUFFER bit.
	*Have to make sure they have good alignment.
		layout (set=m, binding=n) uniform <variable name>
		{
			member1
			member2
		};

	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
	dynamic uniform buffers. Just like uniform buffers except you can change the offset in the commandbuffer call to change value. Pretty much
	you make a big buffer making sure it aligns with vulkanDevice->properties.limits.minUniformBufferOffsetAlignment. Then when creating command
	buffer you call vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 1, &dynamicOffset);
	changing dynamicOffset for each object or whatever.

	*Dynamic notes: For dynamic make a buffer with dynamic_usage. Then make sure you follow minUniformBufferOffsetAlignment theres a function to find this in buffer.cpp.
	Make sure buffer is that size * objects you want, also make sure the data your creating is aligned with that as well. Then the range for the set is the size of one of the variables.
	If you have a big buffer of matrices range isnt that whole thing but one matrix. Then you create uint32_t dynamicOffset = static_cast<uint32_t>(32), and that should work.

	STORAGE BUFFERS / DYNAMIC
	storage buffers are like uniform buffers but you can write to them and they are also used to store really large data, at lower performance.
	Pretty much its the same setup as uniform buffer. Make sure it has alignment.

		layout(std140,set = 1, binding = 0) readonly/writeonly buffer <variable name>
		{
			ObjectData objects[];
		};

	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
	lets use read data inside fragment shaders from images used as render pass attachments. Like reading in depth buffer from previous subpass.
	Could be faster instead of binding image, restarting renderpass, and reading.
	make sure you create them with format for color or depth attachment.

		layout(input_attachment_index=i, set=m, binding=n) uniform subpassInput <variable name>


	PUSH_CONSTANTS
	Great for sending small data that changes a lot, better performance than uniform buffers. at least 128 bytes.
	The pipelinelayout takes x amount of VkPushConstantRange, which holds the shaderstage, offset, and size (both must be multiple of 4).
	Then you can call vkCmdPushConstants(command_buffer, pipeline_layout, pipeline_stages, offset, size, data)

		layout(	push_constant ) uniform <block name>
		{
			variables
		} <instance name>
		<instance name>.<variable name>

--VULKAN MEMORY--
	So in Vulkan theres a lot of memory managent. One is just the cpu side vulkan handle memory management, and you just let vulkan allocate them however you want or they give you an option to 
	plug in your own allocator. Then for command buffers and descriptors they actually give you pools which is nice. 
	Besides this theres the actual gpu memory, which is really the vram and some host side dram that you have to handle. Really you need to allocate buffers or images and the problem is that
	maybe the memory gets fragmented, you run out of memory, you want specific memory on the gpu because its very quick, etc. For my solution I just use VMA to handle the memory and allocations.
	Of course another tip is you mainly want to preallocate all the memory and not do it during runtime.

	In vulkan the physical device has heaps and memory types. The heaps are the actual physical memory which is usually the VRAM on your gpu, the dram on your computer. On amd they have a
	small heap where you can put host_visible and device_visible memory. Of course yu want most memory on the VRAM because its quickest, especially your images and render targets. However you can't
	see this cpu side so usually you have staging buffers or put it on that amd heap to make it fast. Sometimes you can just store it on cpu and keep it mapped. Also if its not host_coherent
	you have to flush memory everytime you map and change it.
	Its important to reivew the memory types, pretty much its gpu_visible, cpu_visible, maybe a mix, and then cache_coherent options for cpu_visible ones. Each has their own perks.

	Then there are memory types which just point to heaps and these are what you interface with when allocating memory on a memorytype.

	So to recap pretty much you want to preallocate as much memory and pools as you can to reduce runtime allocations. You want to allocate memory for the best use case like reading from gpu, 
	writing on gpu reading on cpu, writing on cpu reading gpu, etc. Also of course managing memory and making sure you don't run out. If your more advanced you can look at fragmentation as well.

	ALLOCATOR NOTES (HOST MEMORY)
	VkAllocationCallbacks {
		void*                                   pUserData;
		PFN_vkAllocationFunction                pfnAllocation;
		PFN_vkReallocationFunction              pfnReallocation;
		PFN_vkFreeFunction                      pfnFree;
		PFN_vkInternalAllocationNotification    pfnInternalAllocation;
		PFN_vkInternalFreeNotification          pfnInternalFree;
	} VkAllocationCallbacks;

	when you create all vulkan objects you can pass in a VkAllocationCallbacks  object. You use this if you want to make your own allocator, else vulkan does its own.
	So pretty much this struct holds 5 functions that you override that the allocator must do like allocate, reallocate, free. So the ideas not hard.
	The actual allocation looks like low level stuff like using malloc or other memory calls.

	vkMapMemory
	vkFlushMappedMemoryRanges
	vkInvalidateMappedMemoryRanges
	VkMappedMemoryRange
	vkUnmapMemory


--SYNCHRONIZATION--
	I still don't understand synchronization that much for pipeline barriers. For the main synchronization of the command buffers I just use semaphores (gpu to gpu synchronization)
	and fences (gpu to cpu synchronization) which is usually good enough for my case. 
	However if you do copy operations and change memory usage you will need to usage memory barriers. There are global memory barriers, buffer barriers, and image barriers.
	Pretty much when you change the memory usage of these you need a barrier, specifying the previous vk_access and the new vk_access as well as the stage it was coming from and the stage
	you want the barrier to start at. Its still confusing and not clear.

	In terms of how you render scene you could synrhconize operations by different command buffers and semaphores. You could put them all in one command buffer and do pipeline barriers on the memory.
	You could do different subpasses with dependencies between them. Theres a lot of ways and its good to know your options.

--COMPUTE--
For compute shader you basically have xyz workgroups, then inside each of these you have xyz localworkgroups. Gpu specify the maximum number of workgroups and localworkgroups. The work groups
run in parralell and local work groups all run in parallel. The compute shader runs for every local work group. The important thing is work groups can share memory, and there are memory barriers
and execution barriers you can set up for local work groups. 
Also for limits maxComputeWorkGroupCount[3] is the max number of xyz work groups you can dispatch on a dispatch call.
maxComputeWorkGroupSize[3] is the max number of xyz local workgroup count specified in the shader you can have
maxComputeWorkGroupInvocations is the max number of x*y*z localworkgroups you can have in the shader

*/