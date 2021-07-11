#include "../pch.h"
#include "RenderManager.h"
#include "vkcore/VulkanHelpers.h"

#include "../ThirdParty/ImGui/imgui.h"
#include "../ThirdParty/ImGui/imgui_impl_glfw.h"
#include "../ThirdParty/ImGui/imgui_impl_vulkan.h"
#include "RenderObject.h"

namespace Gibo {

	//GLFW CALLBACK FUNCTIONS. Make sure glfwSetWindowUserPointer(WindowManager.Getwindow(), reinterpret_cast<void*>(this)) is called after creating window.
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		RenderManager * handler = reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window));
		handler->Recreateswapchain();
	}
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window))->GetInputManager().key_callback(window, key, scancode, action, mods);
	}
	static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
	{
		reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window))->GetInputManager().cursor_position_callback(window, xpos, ypos);
	}
	static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
	{
		reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window))->GetInputManager().mouse_button_callback(window, button, action, mods);
	}
	static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
	{
		reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window))->GetInputManager().scroll_callback(window, xoffset, yoffset);
	}


	RenderManager::RenderManager()
	{
	}

	RenderManager::~RenderManager()
	{
	
	}

	void RenderManager::ShutDownRenderer()
	{
		vkDeviceWaitIdle(Device.GetDevice());

		atmosphere->CleanUp();
		delete atmosphere;

		CleanUpPBR();
		CleanUpCompute();
		CleanUpGeneral();
		CleanUpQuad();
		if (enable_imgui)
		{
			closeImGui();
		}

		meshCache->CleanUp();
		delete meshCache;

		textureCache->CleanUp();
		delete textureCache;

		lightmanager->CleanUp();
		delete lightmanager;

		objectmanager->CleanUp();
		delete objectmanager;

		Device.DestroyDevice();

		WindowManager.Terminate();
	}

	void RenderManager::CleanUpPBR()
	{
		PBRdeleteimagedata();
		
		program_pbr.CleanUp();

		for (int i = 0; i < cmdbuffer_pbr.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS), 1, &cmdbuffer_pbr[i]);
		}
	}

	void RenderManager::CleanUpCompute()
	{
		program_compute.CleanUp();

		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_compute.layout, nullptr);
		vkDestroyPipeline(Device.GetDevice(), pipeline_compute.pipeline, nullptr);

		for (int i = 0; i < cmdbuffer_compute.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE), 1, &cmdbuffer_compute[i]);
		}
	}

	void RenderManager::CleanUpGeneral()
	{
		for (int i = 0; i < pv_uniform.size(); i++)
		{
			Device.DestroyBuffer(pv_uniform[i]);
		}

		for (int i = 0; i < inFlightFences.size(); i++)
		{
			vkDestroyFence(Device.GetDevice(), inFlightFences[i], nullptr);
		}
		//swapimageFences are not actually created so don't delete them

		for (int i = 0; i < semaphore_colorpass.size(); i++)
		{
			vkDestroySemaphore(Device.GetDevice(), semaphore_imagefetch[i], nullptr);
			vkDestroySemaphore(Device.GetDevice(), semaphore_colorpass[i], nullptr);
			vkDestroySemaphore(Device.GetDevice(), semaphore_computepass[i], nullptr);
			vkDestroySemaphore(Device.GetDevice(), semaphore_quad[i], nullptr);
			if (enable_imgui)
			{
				vkDestroySemaphore(Device.GetDevice(), semaphore_imgui[i], nullptr);
			}
		}
	}

	void RenderManager::SetMultisampling(SAMPLE_COUNT count)
	{
		vkDeviceWaitIdle(Device.GetDevice());

		VkPhysicalDeviceLimits limits = PhysicalDeviceQuery::GetDeviceLimits(Device.GetPhysicalDevice());
		if (limits.framebufferColorSampleCounts < ConvertSampleCount(count) || limits.framebufferDepthSampleCounts < ConvertSampleCount(count))
		{
			Logger::LogWarning("physical device does not support that sample count\n");
			return;
		}
		Logger::LogError(ConvertSampleCount(count));
		multisampling_count = ConvertSampleCount(count);

		Recreateswapchain();
	}

	void RenderManager::SetResolution(RESOLUTION res)
	{
		int width, height;
		ConvertResolution(res, width, height);
		Resolution.width = width;
		Resolution.height = height;

		Recreateswapchain();
	}

	/*
	If you just change window size, you don't have to recreate everything, just everything relating to the size of the final image your displaying

	However if you want to change resolution then you have to change everything dealing with image attachment size. This is:
		rendertarget images and views
		framebuffer
		pipeline
		command buffer
		any data/buffers that rely on screen width and height (projection matrix buffer, atmosphere shader, etc
	*/
	void RenderManager::Recreateswapchain()
	{ 
		vkDeviceWaitIdle(Device.GetDevice());
		
		int width = 0;
		int height = 0;
		WindowManager.getwindowframebuffersize(width, height);

		while (width == 0 || height == 0) {
			WindowManager.getwindowframebuffersize(width, height);
			glfwWaitEvents();
		}

		//make sure to recreate swapchain first to get swapchains extent
		Device.CleanSwapChain();
		Device.CreateSwapChain(window_extent, FRAMES_IN_FLIGHT);

		if (Resolution_Fitted)
		{
			Resolution = window_extent;
		}

		PBRdeleteimagedata();
		PBRcreateimagedata();

		Quaddeleteimagedata();
		Quadcreateimagedata();

		atmosphere->SwapChainRecreate(Resolution, window_extent, FOV, renderpass_pbr, multisampling_count);

		if (enable_imgui)
		{
			for (int i = 0; i < framebuffer_imgui.size(); i++)
			{
				vkDestroyFramebuffer(Device.GetDevice(), framebuffer_imgui[i], nullptr);
			}
			for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
			{
				VkImageView views = { Device.GetswapchainView(i) };
				framebuffer_imgui[i] = CreateFrameBuffer(Device.GetDevice(), window_extent.width, window_extent.height, renderpass_imgui, &views, 1);
			}
			ImGui_ImplVulkanH_Window info = {};
			//ImGui_ImplVulkanH_CreateOrResizeWindow(Device.GetInstace(), Device.GetPhysicalDevice(), Device.GetDevice(), &g_MainWindowData, PhysicalDeviceQuery::GetQueueFamily(Device.GetPhysicalDevice(), VK_QUEUE_GRAPHICS_BIT),
				//nullptr, window_extent.width, window_extent.height, FRAMES_IN_FLIGHT);
		}

		SetProjectionMatrix();
	}

	void RenderManager::PBRcreateimagedata()
	{
		//renderpasss
		VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		RenderPassCache& rpcache = Device.GetRenderPassCache();
		if (multisampling_count == VK_SAMPLE_COUNT_1_BIT)
		{
			RenderPassAttachment colorattachment(0, color_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::COLOR);
			RenderPassAttachment depthattachment(1, findDepthFormat(Device.GetPhysicalDevice()), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				RENDERPASSTYPE::DEPTH);
			std::vector<RenderPassAttachment> attachments = { colorattachment, depthattachment };
			renderpass_pbr = rpcache.GetRenderPass(attachments.data(), attachments.size(), VK_PIPELINE_BIND_POINT_GRAPHICS);
		}
		else
		{
			RenderPassAttachment colorattachment(0, color_format, multisampling_count, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::COLOR);
			RenderPassAttachment depthattachment(1, findDepthFormat(Device.GetPhysicalDevice()), multisampling_count, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				RENDERPASSTYPE::DEPTH);
			RenderPassAttachment resolveattachment(2, color_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::RESOLVE);
			std::vector<RenderPassAttachment> attachments = { colorattachment, depthattachment, resolveattachment };
			renderpass_pbr = rpcache.GetRenderPass(attachments.data(), attachments.size(), VK_PIPELINE_BIND_POINT_GRAPHICS);
		}

		//image attachments
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), color_format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
		{
			Logger::LogError("format not supported for main color attachment\n");
		}
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), color_format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		{
			Logger::LogError("storage format not supported for main color attachment\n");
		}

		color_attachment.resize(FRAMES_IN_FLIGHT);
		color_attachmentview.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			VkImageUsageFlags color_usage = (multisampling_count == VK_SAMPLE_COUNT_1_BIT) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			Device.CreateImage(VK_IMAGE_TYPE_2D, color_format, color_usage, multisampling_count, Resolution.width, Resolution.height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, color_attachment[i]);

			color_attachmentview[i] = CreateImageView(Device.GetDevice(), color_attachment[i].image, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		}

		depth_attachment.resize(FRAMES_IN_FLIGHT);
		depth_view.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateImage(VK_IMAGE_TYPE_2D, findDepthFormat(Device.GetPhysicalDevice()), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, multisampling_count, Resolution.width,
				Resolution.height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, depth_attachment[i]);
			depth_view[i] = CreateImageView(Device.GetDevice(), depth_attachment[i].image, findDepthFormat(Device.GetPhysicalDevice()), VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		}

		resolve_attachment.resize(FRAMES_IN_FLIGHT);
		resolve_attachmentview.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			VkImageUsageFlags resolve_usage = VK_IMAGE_USAGE_SAMPLED_BIT; // VK_IMAGE_USAGE_STORAGE_BIT
			Device.CreateImage(VK_IMAGE_TYPE_2D, color_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | resolve_usage, VK_SAMPLE_COUNT_1_BIT, Resolution.width, Resolution.height, 1, 1, 1,
				VMA_MEMORY_USAGE_GPU_ONLY, 0, resolve_attachment[i]);

			resolve_attachmentview[i] = CreateImageView(Device.GetDevice(), resolve_attachment[i].image, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		}

		//framebuffer
		framebuffer_pbr.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			if (multisampling_count == VK_SAMPLE_COUNT_1_BIT)
			{
				std::vector<VkImageView> imageview = { color_attachmentview[i], depth_view[i] };
				framebuffer_pbr[i] = CreateFrameBuffer(Device.GetDevice(), Resolution.width, Resolution.height, renderpass_pbr, imageview.data(), imageview.size());
			}
			else
			{
				std::vector<VkImageView> imageview = { color_attachmentview[i], depth_view[i], resolve_attachmentview[i] };
				framebuffer_pbr[i] = CreateFrameBuffer(Device.GetDevice(), Resolution.width, Resolution.height, renderpass_pbr, imageview.data(), imageview.size());
			}
		}

		//pipeline
		PipelineCache& pipecache = Device.GetPipelineCache();
		PipelineData pipelinedata(Resolution.width, Resolution.height);

		pipelinedata.ColorBlendstate.blendenable = VK_TRUE;
		pipelinedata.ColorBlendstate.colorblendop = VK_BLEND_OP_ADD;
		pipelinedata.ColorBlendstate.dstblendfactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		pipelinedata.ColorBlendstate.srcblendfactor = VK_BLEND_FACTOR_SRC_ALPHA;
		pipelinedata.ColorBlendstate.alphablendop = VK_BLEND_OP_ADD;
		pipelinedata.ColorBlendstate.dstalphablendfactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		pipelinedata.ColorBlendstate.srcalphablendfactor = VK_BLEND_FACTOR_SRC_ALPHA;

		pipelinedata.Rasterizationstate.cullmode = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT
		pipelinedata.Rasterizationstate.frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelinedata.Rasterizationstate.polygonmode = VK_POLYGON_MODE_FILL;

		pipelinedata.DepthStencilstate.depthtestenable = VK_TRUE;
		pipelinedata.DepthStencilstate.depthcompareop = VK_COMPARE_OP_LESS;
		pipelinedata.DepthStencilstate.depthwriteenable = VK_TRUE;

		pipelinedata.Multisamplingstate.samplecount = multisampling_count;
		pipelinedata.Multisamplingstate.sampleshadingenable = VK_TRUE;
		pipelinedata.Multisamplingstate.minsampleshading = .5;

		pipelinedata.Inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		std::vector<VkDescriptorSetLayout> layoutsz = { program_pbr.GetGlobalLayout(), program_pbr.GetLocalLayout() };

		pipeline_pbr = pipecache.GetGraphicsPipeline(pipelinedata, Device.GetPhysicalDevice(), renderpass_pbr, program_pbr.GetShaderStageInfo(), program_pbr.GetPushRanges(), layoutsz.data(), layoutsz.size());
	}

	void RenderManager::PBRdeleteimagedata()
	{
		vkDestroyRenderPass(Device.GetDevice(), renderpass_pbr, nullptr);

		for (int i = 0; i < color_attachment.size(); i++)
		{
			Device.DestroyImage(color_attachment[i]);
			vkDestroyImageView(Device.GetDevice(), color_attachmentview[i], nullptr);
		}
		color_attachment.clear();
		for (int i = 0; i < depth_attachment.size(); i++)
		{
			Device.DestroyImage(depth_attachment[i]);
			vkDestroyImageView(Device.GetDevice(), depth_view[i], nullptr);
		}
		depth_attachment.clear();
		for (int i = 0; i < framebuffer_pbr.size(); i++)
		{
			vkDestroyFramebuffer(Device.GetDevice(), framebuffer_pbr[i], nullptr);
		}
		for (int i = 0; i < resolve_attachment.size(); i++)
		{
			Device.DestroyImage(resolve_attachment[i]);
			vkDestroyImageView(Device.GetDevice(), resolve_attachmentview[i], nullptr);
		}
		resolve_attachment.clear();

		framebuffer_pbr.clear();
		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_pbr.layout, nullptr);
		vkDestroyPipeline(Device.GetDevice(), pipeline_pbr.pipeline, nullptr);
	}

	void RenderManager::CreateQuad()
	{
		mesh_quad = meshCache->GetQuadMesh();

		RenderPassCache& rpcache = Device.GetRenderPassCache();
		RenderPassAttachment colorattachment(0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::COLOR);
		
		std::vector<RenderPassAttachment> attachments = { colorattachment };
		renderpass_quad = rpcache.GetRenderPass(attachments.data(), attachments.size(), VK_PIPELINE_BIND_POINT_GRAPHICS);


		//shader program/ uniform buffer
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/vertquad.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/fragquad.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
			{"tex", 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo1 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants1 = {
		};

		if (!program_quad.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(),
			pushconstants1.data(), pushconstants1.size(), 5))
		{
			Logger::LogError("failed to create quad shaderprogram\n");
		}

		SamplerKey key(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			false, 5.0, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0, 0, 0);
		sampler_quad = Device.GetSamplerCache().GetSampler(key);

		Quadcreateimagedata();

		//commandbuffer
		cmdbuffer_quad.resize(FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo cmd_info = {};
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS);
		cmd_info.commandBufferCount = cmdbuffer_quad.size();
		vkAllocateCommandBuffers(Device.GetDevice(), &cmd_info, cmdbuffer_quad.data());
	}

	void RenderManager::Quadcreateimagedata()
	{
		//this should be identical to main color attachment except for resolution
		color_attachmentquad.resize(FRAMES_IN_FLIGHT);
		color_attachmentviewquad.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateImage(VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, window_extent.width, window_extent.height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, color_attachmentquad[i]);
			color_attachmentviewquad[i] = CreateImageView(Device.GetDevice(), color_attachmentquad[i].image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		}

		//framebuffer
		framebuffers_quad.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			VkImageView views = { color_attachmentviewquad[i] };
			framebuffers_quad[i] = CreateFrameBuffer(Device.GetDevice(), window_extent.width, window_extent.height, renderpass_quad, &views, 1);
		}

		PipelineCache& pipecache = Device.GetPipelineCache();
		PipelineData pipelinedata(window_extent.width, window_extent.height);

		pipelinedata.ColorBlendstate.blendenable = VK_FALSE;
		pipelinedata.Rasterizationstate.cullmode = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT
		pipelinedata.Rasterizationstate.frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelinedata.Rasterizationstate.polygonmode = VK_POLYGON_MODE_FILL;

		pipelinedata.DepthStencilstate.depthtestenable = VK_FALSE;
		pipelinedata.DepthStencilstate.depthwriteenable = VK_FALSE;
		pipelinedata.Inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		std::vector<VkDescriptorSetLayout> layoutsz = { program_quad.GetGlobalLayout(), program_quad.GetLocalLayout() };

		pipeline_quad = pipecache.GetGraphicsPipeline(pipelinedata, Device.GetPhysicalDevice(), renderpass_quad, program_quad.GetShaderStageInfo(), program_quad.GetPushRanges(), layoutsz.data(), layoutsz.size());

		//set global descriptor
		DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			global_descriptors.imageviews[i].push_back((multisampling_count == VK_SAMPLE_COUNT_1_BIT) ? color_attachmentview[i] : resolve_attachmentview[i]);
			global_descriptors.samplers[i].push_back(sampler_quad);
		}
		program_quad.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);
	}

	void RenderManager::Quaddeleteimagedata()
	{
		for (int i = 0; i < color_attachmentquad.size(); i++)
		{
			Device.DestroyImage(color_attachmentquad[i]);
			vkDestroyImageView(Device.GetDevice(), color_attachmentviewquad[i], nullptr);
		}

		for (int i = 0; i < framebuffers_quad.size(); i++)
		{
			vkDestroyFramebuffer(Device.GetDevice(), framebuffers_quad[i], nullptr);
		}

		vkDestroyPipeline(Device.GetDevice(), pipeline_quad.pipeline, nullptr);
		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_quad.layout, nullptr);
	}

	void RenderManager::RecordQuadCmd(int current_frame, int current_imageindex)
	{
		vkResetCommandBuffer(cmdbuffer_quad[current_frame], 0);

		//commandbuffer
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdbuffer_quad[current_frame], &begin_info);
		VkRenderPassBeginInfo begin_rp = {};
		begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.renderPass = renderpass_quad;
		begin_rp.framebuffer = framebuffers_quad[current_frame];
		begin_rp.renderArea.offset = { 0,0 };
		begin_rp.renderArea.extent = { window_extent.width, window_extent.height };
		std::array<VkClearValue, 1> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		begin_rp.pClearValues = clearValues.data();
		begin_rp.clearValueCount = clearValues.size();

		//reset both ones
		Device.GetQueryManager().ResetQueries(cmdbuffer_quad[current_frame], current_frame, QueryManager::QUERY_NAME::QUAD);

		//set timer here at top of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_quad[current_frame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::QUAD, true);

		vkCmdBeginRenderPass(cmdbuffer_quad[current_frame], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmdbuffer_quad[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_quad.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_quad[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_quad.layout, 0, 1, &program_quad.GetGlobalDescriptor(current_frame), 0, nullptr);

		VkDeviceSize sizes[] = { 0 };
		vkCmdBindVertexBuffers(cmdbuffer_quad[current_frame], 0, 1, &mesh_quad.vbo, sizes);
		vkCmdBindIndexBuffer(cmdbuffer_quad[current_frame], mesh_quad.ibo, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmdbuffer_quad[current_frame], mesh_quad.index_size, 1, 0, 0, 0);

		vkCmdEndRenderPass(cmdbuffer_quad[current_frame]);

		//set timer here at bottom of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_quad[current_frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::QUAD, false);

		vkEndCommandBuffer(cmdbuffer_quad[current_frame]);
	}

	void RenderManager::CleanUpQuad()
	{
		vkDestroyRenderPass(Device.GetDevice(), renderpass_quad, nullptr);

		program_quad.CleanUp();

		Quaddeleteimagedata();

		for (int i = 0; i < cmdbuffer_quad.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS), 1, &cmdbuffer_quad[i]);
		}
	}

	void RenderManager::startImGui()
	{
		//descriptor pool
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		VULKAN_CHECK(vkCreateDescriptorPool(Device.GetDevice(), &pool_info, nullptr, &imguipool), "creating imgui descriptor pool");

		//renderpass
		RenderPassAttachment attachment(0, Device.GetswapchainFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::COLOR);
		renderpass_imgui = Device.GetRenderPassCache().GetRenderPass(&attachment, 1);

		//framebuffers
		framebuffer_imgui.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			VkImageView views = { Device.GetswapchainView(i) };
			framebuffer_imgui[i] = CreateFrameBuffer(Device.GetDevice(), window_extent.width, window_extent.height, renderpass_imgui, &views, 1);
		}

		//command buffers
		cmdbuffer_imgui.resize(FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS);
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = cmdbuffer_imgui.size();
		VULKAN_CHECK(vkAllocateCommandBuffers(Device.GetDevice(), &allocInfo, cmdbuffer_imgui.data()), "allocating imgui cmdbuffers");


		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		ImGui::StyleColorsClassic();

		//ImGui_ImplVulkan_SetMinImageCount()
		//ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
		//ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForVulkan(WindowManager.Getwindow(), true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = Device.GetInstace();
		init_info.PhysicalDevice = Device.GetPhysicalDevice();
		init_info.Device = Device.GetDevice();
		init_info.QueueFamily = PhysicalDeviceQuery::GetQueueFamily(Device.GetPhysicalDevice(), VK_QUEUE_GRAPHICS_BIT);
		init_info.Queue = Device.GetQueue(POOL_FAMILY::GRAPHICS);
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = imguipool;
		init_info.Allocator = nullptr;
		init_info.MinImageCount = FRAMES_IN_FLIGHT;
		init_info.ImageCount = FRAMES_IN_FLIGHT;
		init_info.CheckVkResultFn = nullptr;
		ImGui_ImplVulkan_Init(&init_info, renderpass_imgui);

		VkCommandBuffer font_cmd = Device.beginSingleTimeCommands(POOL_FAMILY::GRAPHICS);
		ImGui_ImplVulkan_CreateFontsTexture(font_cmd);
		Device.submitSingleTimeCommands(font_cmd, POOL_FAMILY::GRAPHICS);
	}

	void RenderManager::updateImGui(int current_frame, int imageindex)
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		//ImGui::ShowDemoWindow();

		ImGui::Begin("timers");
		
		//ImGui::Checkbox
		//ImGui::ColorEdit3
		//ImGui::PlotHistogram
		//ImGui::Text
		
		char overlay[32]; 
		sprintf_s(overlay, "%f milliseconds", time_mainpass[time_counter]);
		ImGui::PlotLines("main color", time_mainpass.data(), time_mainpass.size(), 0, overlay, 0.0f, 32.0f, ImVec2(0, 80.0f));
		
		sprintf_s(overlay, "%f milliseconds", time_quad[time_counter]);
		ImGui::PlotLines("quad", time_quad.data(), time_quad.size(), 0, overlay, 0.0f, 32.0f, ImVec2(0, 80.0f));

		sprintf_s(overlay, "%f milliseconds", time_pp[time_counter]);
		ImGui::PlotLines("post process", time_pp.data(), time_pp.size(), 0, overlay, 0.0f, 32.0f, ImVec2(0, 80.0f));
		
		sprintf_s(overlay, "%f milliseconds", time_cpu[time_counter]);
		ImGui::PlotLines("render cpu", time_cpu.data(), time_cpu.size(), 0, overlay, 0.0f, 32.0f, ImVec2(0, 80.0f));
		//ImGui::PlotLines("main pass", values, 8);
		//ImGui::DragFloat
		//ImGui::ImageButton
		//ImGui::SliderFloat
		int prev = imguimulti_count;
		ImGui::InputInt("Multi_Sampling (1,2,4,8)", &imguimulti_count, 1, 6, 0); ImGuiInputTextFlags;
		if (prev != imguimulti_count)
		{
			multi_changed = true;
		}

		bool previousfull_screen = imguifullscreen;
		ImGui::Checkbox("full_screen", &imguifullscreen);
		if (previousfull_screen != imguifullscreen)
		{
			full_screenchanged = true;
		}

		char overlay2[32];
		sprintf_s(overlay2, "%f ", imguifov);
		ImGui::SliderFloat("FOV", &imguifov, 0, 180, overlay2, 1);

		const char* items[] = { "Fit_Window", "High", "Medium", "Low"};
		int previousres = imguiresolution;
		ImGui::Combo("Resolution", &imguiresolution, items, 4);
		if (previousres != imguiresolution)
		{
			resolution_changed = true;
		}

		ImGui::End();
		
		ImGui::Render();

		vkResetCommandBuffer(cmdbuffer_imgui[current_frame], 0);

		//commandbuffer
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdbuffer_imgui[current_frame], &begin_info);
		VkRenderPassBeginInfo begin_rp = {};
		begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.renderPass = renderpass_imgui;
		begin_rp.framebuffer = framebuffer_imgui[imageindex];
		begin_rp.renderArea.offset = { 0,0 };
		begin_rp.renderArea.extent = { window_extent.width, window_extent.height };
		std::array<VkClearValue, 1> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		begin_rp.pClearValues = clearValues.data();
		begin_rp.clearValueCount = clearValues.size();

		vkCmdBeginRenderPass(cmdbuffer_imgui[current_frame], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

		//draw call
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuffer_imgui[current_frame]);
		
		vkCmdEndRenderPass(cmdbuffer_imgui[current_frame]);
		vkEndCommandBuffer(cmdbuffer_imgui[current_frame]);
	}

	void RenderManager::closeImGui()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		vkDestroyDescriptorPool(Device.GetDevice(), imguipool, nullptr);
		vkDestroyRenderPass(Device.GetDevice(), renderpass_imgui, nullptr);

		for (int i = 0; i < framebuffer_imgui.size(); i++)
		{
			vkDestroyFramebuffer(Device.GetDevice(), framebuffer_imgui[i], nullptr);
		}

		for (int i = 0; i < cmdbuffer_compute.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS), 1, &cmdbuffer_imgui[i]);
		}
	}

	bool RenderManager::InitializeRenderer()
	{
		bool valid = true;

		valid = valid && WindowManager.InitializeVulkan(window_extent.width, window_extent.height, &framebuffer_size_callback, key_callback, cursor_position_callback, mouse_button_callback, scroll_callback);
		//set the callbacks to come back to this instance of rendermanager
		glfwSetWindowUserPointer(WindowManager.Getwindow(), reinterpret_cast<void*>(this));

		valid = valid && Device.CreateDevice("GiboRenderer", WindowManager.Getwindow(), window_extent, Resolution, FRAMES_IN_FLIGHT);
		if (Resolution_Fitted)
		{
			Resolution = window_extent;
		}

		Logger::LogInfo("Compiling Shaders-------------------\n");
		std::system("cd Shaders && compile.bat");
		Logger::LogInfo("Shaders Finished-------------------\n");
		
		meshCache = new MeshCache(Device);
		textureCache = new TextureCache(Device);
		lightmanager = new LightManager(Device, FRAMES_IN_FLIGHT);
		objectmanager = new RenderObjectManager(Device, FRAMES_IN_FLIGHT);

		CreatePBR();
		CreateAtmosphere();
		atmosphere->UpdateCamPosition(glm::vec4(0, 0, 0, 1));
		createPBRfinal();
		CreateCompute();
		CreateQuad();

		if (enable_imgui) 
		{
			startImGui();

			semaphore_imgui.resize(FRAMES_IN_FLIGHT);
			for (int i = 0; i < semaphore_imgui.size(); i++)
			{
				VkSemaphoreCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
				info.flags = 0;
				vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_imgui[i]);
			}
		}

		//fences and semaphores
		semaphore_imagefetch.resize(FRAMES_IN_FLIGHT);
		semaphore_colorpass.resize(FRAMES_IN_FLIGHT);
		semaphore_computepass.resize(FRAMES_IN_FLIGHT);
		semaphore_quad.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < semaphore_imagefetch.size(); i++)
		{
			VkSemaphoreCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			info.flags = 0;

			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_imagefetch[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_colorpass[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_computepass[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_quad[i]);
		}

		inFlightFences.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < inFlightFences.size(); i++)
		{
			VkFenceCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			vkCreateFence(Device.GetDevice(), &info, nullptr, &inFlightFences[i]);
		}
		swapimageFences.resize(Device.GetSwapChainImageCount());

		return valid;
	}

	void RenderManager::CreateAtmosphere()
	{
		atmosphere = new Atmosphere(Device, textureCache->Get2DTexture("Images/missingtexture.png", 0), FRAMES_IN_FLIGHT);
		atmosphere->Create(window_extent, Resolution, FOV, renderpass_pbr, *meshCache, FRAMES_IN_FLIGHT, pv_uniform, multisampling_count);
		atmosphere->FillLUT();
	}

	void RenderManager::CreateCompute()
	{
		program_compute;
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/comp.spv", VK_SHADER_STAGE_COMPUTE_BIT},
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
			{"input", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"output", 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo1 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants1 = {
		};
		if (!program_compute.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(), pushconstants1.data(), pushconstants1.size(), 1))
		{
			Logger::LogError("failed to create pp shaderprogram\n");
		}

		pipeline_compute = Device.GetPipelineCache().GetComputePipeline(program_compute.GetShaderStageInfo()[0], &program_compute.GetGlobalLayout(), 1);

		//create cmdbuffers
		cmdbuffer_compute.resize(FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE);
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = cmdbuffer_compute.size();
		VULKAN_CHECK(vkAllocateCommandBuffers(Device.GetDevice(), &allocInfo, cmdbuffer_compute.data()), "allocating compute cmdbuffers");
	}

	void RenderManager::RecordComputeCmd(int current_frame, int current_imageindex)
	{
		//we have to set the current_frames global descriptorset to point to the specific swapchain image we have this frame in flight. 
		DescriptorHelper global_descriptors(1);
		global_descriptors.imageviews[0].push_back(color_attachmentviewquad[current_frame]);
		global_descriptors.samplers[0].push_back(atmosphere->GetSampler());
		global_descriptors.imageviews[0].push_back(Device.GetswapchainView(current_imageindex));
		global_descriptors.samplers[0].push_back(atmosphere->GetSampler());
		program_compute.SetSpecificGlobalDescriptor(current_frame, global_descriptors.uniformbuffers[0], global_descriptors.buffersizes[0], global_descriptors.imageviews[0], global_descriptors.samplers[0], global_descriptors.bufferviews[0]);
		
		//now just reset command buffer and record using the new swapchainimage and current_frame.
		vkResetCommandBuffer(cmdbuffer_compute[current_frame], 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_compute[current_frame], &beginInfo), "begin post proccess cmdbuffer");

		//reset both ones
		Device.GetQueryManager().ResetQueries(cmdbuffer_compute[current_frame], current_frame, QueryManager::QUERY_NAME::POST_PROCESS);

		//set timer here at top of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_compute[current_frame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::POST_PROCESS, true);

		TransitionImageLayout(Device, Device.GetswapchainImage(current_imageindex), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_HOST_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, cmdbuffer_compute[current_frame]);

		float WORKGROUP_SIZE = 32.0;
		vkCmdBindPipeline(cmdbuffer_compute[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_compute[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute.layout, 0, 1, &program_compute.GetGlobalDescriptor(current_frame), 0, nullptr);
		vkCmdDispatch(cmdbuffer_compute[current_frame], window_extent.width / WORKGROUP_SIZE, window_extent.height / WORKGROUP_SIZE, 1);

		//IMGUI - if imgui is one we need to transition it from general to colorattachment, else put it to present
		VkImageLayout transition_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		if (enable_imgui)
		{
			transition_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		TransitionImageLayout(Device, Device.GetswapchainImage(current_imageindex), VK_IMAGE_LAYOUT_GENERAL, transition_layout, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, cmdbuffer_compute[current_frame]);
		
		//set timer here at bottom of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_compute[current_frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::POST_PROCESS, false);

		VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_compute[current_frame]), "end cmdbuffer");
	}

	void RenderManager::CreatePBR()
	{
		//pv buffer
		SetProjectionMatrix();

		pv_uniform.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateBuffer(sizeof(glm::mat4) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, pv_uniform[i]);
		}

		//shader program/ uniform buffer
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/pbrvert.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/pbrfrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
			{"ViewProjBuffer", 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
			{"AmbientLut", 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"AtmosphereBuffer", 7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
			{"light_struct", 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
			{"lightcount_struct", 9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
			{"TransmittanceLUT", 10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo1 = {
				{"MaterialBuffer", 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
				{"Albedo_Map", 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
				{"Specular_Map", 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
				{"Metal_Map", 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
				{"Normal_Map", 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants1 = {
			{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)}
		};

		if (!program_pbr.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(), 
			 pushconstants1.data(), pushconstants1.size(), 500))
		{
			Logger::LogError("failed to create pbr shaderprogram\n");
		}
	

		PBRcreateimagedata();
	}

	//projection matrix will have same aspect ratio as window. Could also set it to a default value if you want to see the same thing no matter what window size is.
	void RenderManager::SetProjectionMatrix()
	{
		proj_matrix = glm::perspective(glm::radians(FOV), ((float)window_extent.width / (float)window_extent.height), near_plane, far_plane);
	}

	void RenderManager::SetCameraMatrix()
	{
		cam_matrix = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0, 0, 3) + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
	}

	void RenderManager::createPBRfinal()
	{
		//descriptors global
		DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			global_descriptors.buffersizes[i].push_back(sizeof(glm::mat4) * 2);
			global_descriptors.uniformbuffers[i].push_back(pv_uniform[i]);

			global_descriptors.buffersizes[i].push_back(sizeof(Atmosphere::atmosphere_shader));
			global_descriptors.uniformbuffers[i].push_back(atmosphere->Getshaderinfobuffer(i));

			global_descriptors.buffersizes[i].push_back(sizeof(Light::lightparams) * LightManager::MAX_LIGHTS);
			global_descriptors.uniformbuffers[i].push_back(lightmanager->GetLightBuffer(i));

			global_descriptors.buffersizes[i].push_back(sizeof(int));
			global_descriptors.uniformbuffers[i].push_back(lightmanager->GetLightCountBuffer(i));

			global_descriptors.imageviews[i].push_back(atmosphere->GetAmbientView());
			global_descriptors.samplers[i].push_back(atmosphere->GetSampler());

			global_descriptors.imageviews[i].push_back(atmosphere->GetTransmittanceView());
			global_descriptors.samplers[i].push_back(atmosphere->GetSampler());
		}
		program_pbr.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);
		
		//commandbuffer
		cmdbuffer_pbr.resize(FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo cmd_info = {};
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS);
		cmd_info.commandBufferCount = cmdbuffer_pbr.size();
		vkAllocateCommandBuffers(Device.GetDevice(), &cmd_info, cmdbuffer_pbr.data());
	}

	void RenderManager::RecordPBRCmd(int current_frame)
	{
		vkResetCommandBuffer(cmdbuffer_pbr[current_frame], 0);

		//commandbuffer
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdbuffer_pbr[current_frame], &begin_info);
		VkRenderPassBeginInfo begin_rp = {};
		begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.renderPass = renderpass_pbr;
		begin_rp.framebuffer = framebuffer_pbr[current_frame];
		begin_rp.renderArea.offset = { 0,0 };
		begin_rp.renderArea.extent = { Resolution.width, Resolution.height };
		std::array<VkClearValue, 3> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		clearValues[2].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		begin_rp.pClearValues = clearValues.data();
		begin_rp.clearValueCount = clearValues.size();
		
		//reset both ones
		Device.GetQueryManager().ResetQueries(cmdbuffer_pbr[current_frame], current_frame, QueryManager::QUERY_NAME::MAIN_PASS);

		//set timer here at top of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_pbr[current_frame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::MAIN_PASS, true);

		vkCmdBeginRenderPass(cmdbuffer_pbr[current_frame], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmdbuffer_pbr[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_pbr[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 0, 1, &program_pbr.GetGlobalDescriptor(current_frame), 0, nullptr);

		//render all opaque objects first
		auto bin = objectmanager->GetBin()[RenderObjectManager::BIN_TYPE::REGULAR];
		for (int j = 0; j < bin.size(); j++)
		{
			vkCmdBindDescriptorSets(cmdbuffer_pbr[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 1, 1, &program_pbr.GetLocalDescriptor(bin[j]->GetId(), current_frame), 0, nullptr);
			vkCmdPushConstants(cmdbuffer_pbr[current_frame], pipeline_pbr.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &bin[j]->GetMatrix(current_frame));

			VkDeviceSize sizes[] = { 0 };
			vkCmdBindVertexBuffers(cmdbuffer_pbr[current_frame], 0, 1, &bin[j]->GetMesh().vbo, sizes);
			vkCmdBindIndexBuffer(cmdbuffer_pbr[current_frame], bin[j]->GetMesh().ibo, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdbuffer_pbr[current_frame], bin[j]->GetMesh().index_size, 1, 0, 0, 0);
		}

		atmosphere->Draw(cmdbuffer_pbr[current_frame], current_frame);

		vkCmdBindPipeline(cmdbuffer_pbr[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_pbr[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 0, 1, &program_pbr.GetGlobalDescriptor(current_frame), 0, nullptr);

		//render all blendable objects back to front
		bin = objectmanager->GetBin()[RenderObjectManager::BIN_TYPE::BLENDABLE];
		for (int j = 0; j < bin.size(); j++)
		{
			vkCmdBindDescriptorSets(cmdbuffer_pbr[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 1, 1, &program_pbr.GetLocalDescriptor(bin[j]->GetId(), current_frame), 0, nullptr);
			vkCmdPushConstants(cmdbuffer_pbr[current_frame], pipeline_pbr.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &bin[j]->GetMatrix(current_frame));

			VkDeviceSize sizes[] = { 0 };
			vkCmdBindVertexBuffers(cmdbuffer_pbr[current_frame], 0, 1, &bin[j]->GetMesh().vbo, sizes);
			vkCmdBindIndexBuffer(cmdbuffer_pbr[current_frame], bin[j]->GetMesh().ibo, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdbuffer_pbr[current_frame], bin[j]->GetMesh().index_size, 1, 0, 0, 0);
		}
		
		vkCmdEndRenderPass(cmdbuffer_pbr[current_frame]);

		//set timer here at bottom of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_pbr[current_frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::MAIN_PASS, false);

		vkEndCommandBuffer(cmdbuffer_pbr[current_frame]);
	}

	//we want it so that if a is closer to screen it is true, if be is closerorequal to screen it is false
	bool blendsort(RenderObject* a, RenderObject* b, glm::mat4 cam_matrix)
	{
		//convert center of every object from model space to camera space then compare z value
		glm::mat4 camera_matrix = cam_matrix;// = vkcoreShaderDescriptors::GetInstance()->GetMatrix().view;
		glm::vec4 pos(0, 0, 0, 1);
		glm::mat4 amatrix = a->GetSyncedMatrix();
		glm::mat4 bmatrix = b->GetSyncedMatrix();
		glm::vec4 apos = camera_matrix * amatrix * pos;
		glm::vec4 bpos = camera_matrix * bmatrix * pos;
		//compare z
		if (apos.z > bpos.z)
		{
			return true;
		}

		return false;
	}

	void RenderManager::SortBlendedObjects()
	{
		//sort blendable objects front to back. vector size will never be that big (<100) and they will be mostly sorted so an insertion sort it perfect.
		auto& bin = objectmanager->GetBin()[RenderObjectManager::BIN_TYPE::BLENDABLE];
		int n = bin.size();
		int i, j;
		RenderObject* key;
		for (i = 1; i < n; i++)
		{
			key = bin[i];
			j = i - 1;

			/* Move elements of arr[0..i-1], that are
			greater than key, to one position ahead
			of their current position */
			while (j >= 0 && blendsort(bin[j], key, cam_matrix))
			{
				bin[j + 1] = bin[j];
				j = j - 1;
			}
			bin[j + 1] = key;
		}
	}

	void RenderManager::Render()
	{
//#ifdef _DEBUG
		double cpu_time = 0;
		double cpu_tmp = 0;
		Timer cpu_timer("cpu renderer 1");
//#endif
		//CPU_ONLY: update cpu only data that has no gpu dependencies or if we add an extra frame so it doesn't matter
		objectmanager->Update(); //gpu dependency deleting renderobject but added extra frame
		UpdateDescriptorGraveYard(); //gpu dependency deleting descriptor set but added extra frame

		SortBlendedObjects(); //cpu only memory just rearring data structure
//#ifdef _DEBUG
		cpu_timer.Stop(cpu_time);
//#endif
		//wait for resource key. we have a resource for every frame in flight.
		vkWaitForFences(Device.GetDevice(), 1, &inFlightFences[current_frame_in_flight], VK_TRUE, UINT32_MAX);

		//fetch image index were going to use. semaphore tells us when we actually acquired it. acquire image time depends on presentation mode immediate its like 0 seconds it waits.
		uint32_t imageIndex;
		VULKAN_CHECK(vkAcquireNextImageKHR(Device.GetDevice(), Device.GetSwapChain(), UINT64_MAX, semaphore_imagefetch[current_frame_in_flight], VK_NULL_HANDLE, &imageIndex), "acquiring swapchain image");
		//if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			//recreateSwapChain();
		//}

		//#ifdef _DEBUG
		Timer cpu_timer2("cpu renderer 2");
		//#endif

		//check querytimes
		time_mainpass[time_counter] = Device.GetQueryManager().GetNanoseconds(current_frame_in_flight, QueryManager::QUERY_NAME::MAIN_PASS) / 1000000.0;
		time_pp[time_counter] = Device.GetQueryManager().GetNanoseconds(current_frame_in_flight, QueryManager::QUERY_NAME::POST_PROCESS) / 1000000.0;
		time_quad[time_counter] = Device.GetQueryManager().GetNanoseconds(current_frame_in_flight, QueryManager::QUERY_NAME::QUAD) / 1000000.0;
		time_counter = (time_counter + 1) % time_mainpass.size();
		
		//GPU_DEPENDENT: gpu data can now be updated because we have the resource key
		if (enable_imgui)
		{
			updateImGui(current_frame_in_flight, imageIndex);
		}

		for (int i = 0; i < objectmanager->GetVector().size(); i++) //gpu dependency/ updating model matrix data
		{
			objectmanager->GetVector()[i]->Update(current_frame_in_flight);
		}

		//proj/view matrix gpu dependency
		std::vector<glm::mat4> pv_matrix = { cam_matrix, proj_matrix };
		Device.BindData(pv_uniform[current_frame_in_flight].allocation, pv_matrix.data(), sizeof(glm::mat4) * 2);

		//we have resource key so here is where we update gpu dependencies for current frame
		lightmanager->Update(current_frame_in_flight); //gpu dependency, its changing current frames light buffer
		atmosphere->Update(current_frame_in_flight); //gpu dependency, its changing current frames shaderinfo buffer

		//resubmit commandbuffers that need to be updated every frame
		RecordPBRCmd(current_frame_in_flight);
		RecordQuadCmd(current_frame_in_flight, imageIndex);
		RecordComputeCmd(current_frame_in_flight, imageIndex);

//#ifdef _DEBUG
		cpu_timer2.Stop(cpu_tmp);
		cpu_time += cpu_tmp;
//#endif
		//wait for image key. the image could be in use now if we have more frames in flight than swapchain images. Also we have to assume we can't start the pipeline
		//until we have the image key though we only need it for compute stage, but were free to write to cpu data.
		if (swapimageFences[imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(Device.GetDevice(), 1, &swapimageFences[imageIndex], VK_TRUE, UINT32_MAX);
		}
		//image key gets freed when this flight fence gets freed. Its a deep copy.
		swapimageFences[imageIndex] = inFlightFences[current_frame_in_flight];
		//set fence to unsignaled. will set to signaled when we are finished with the last command buffer submission.
		vkResetFences(Device.GetDevice(), 1, &inFlightFences[current_frame_in_flight]);
//#ifdef _DEBUG
		Timer cpu_timer3("cpu renderer 3");
//#endif

		//submit main color pass
		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmdbuffer_pbr[current_frame_in_flight];

		VkPipelineStageFlags stages_colorpass[] = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit_info.pWaitDstStageMask = stages_colorpass;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &semaphore_imagefetch[current_frame_in_flight];

		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &semaphore_colorpass[current_frame_in_flight];
		
		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::GRAPHICS), 1, &submit_info, VK_NULL_HANDLE);

		//submit quad pass
		VkSubmitInfo submit_info_quad = {};
		submit_info_quad.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info_quad.commandBufferCount = 1;
		submit_info_quad.pCommandBuffers = &cmdbuffer_quad[current_frame_in_flight];

		VkPipelineStageFlags stages_quad[] = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit_info_quad.pWaitDstStageMask = stages_quad;
		submit_info_quad.waitSemaphoreCount = 1;
		submit_info_quad.pWaitSemaphores = &semaphore_colorpass[current_frame_in_flight];

		submit_info_quad.signalSemaphoreCount = 1;
		submit_info_quad.pSignalSemaphores = &semaphore_quad[current_frame_in_flight];

		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::GRAPHICS), 1, &submit_info_quad, VK_NULL_HANDLE);

		//submit Post-Process pass
		VkSubmitInfo submit_info_compute = {};
		submit_info_compute.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info_compute.commandBufferCount = 1;
		submit_info_compute.pCommandBuffers = &cmdbuffer_compute[current_frame_in_flight];

		VkPipelineStageFlags stages_compute[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
		submit_info_compute.pWaitDstStageMask = stages_colorpass;
		submit_info_compute.waitSemaphoreCount = 1;
		submit_info_compute.pWaitSemaphores = &semaphore_quad[current_frame_in_flight];

		submit_info_compute.signalSemaphoreCount = 1;
		submit_info_compute.pSignalSemaphores = &semaphore_computepass[current_frame_in_flight];

		VkFence submit_fence = inFlightFences[current_frame_in_flight];
		if (enable_imgui)
		{
			submit_fence = VK_NULL_HANDLE;
		}
		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::COMPUTE), 1, &submit_info_compute, submit_fence);

		if (enable_imgui)
		{
			//submit imgui pass
			VkSubmitInfo submit_info_imgui = {};
			submit_info_imgui.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit_info_imgui.commandBufferCount = 1;
			submit_info_imgui.pCommandBuffers = &cmdbuffer_imgui[current_frame_in_flight];

			VkPipelineStageFlags stages_imgui[] = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
			submit_info_imgui.pWaitDstStageMask = stages_imgui;
			submit_info_imgui.waitSemaphoreCount = 1;
			submit_info_imgui.pWaitSemaphores = &semaphore_computepass[current_frame_in_flight];

			submit_info_imgui.signalSemaphoreCount = 1;
			submit_info_imgui.pSignalSemaphores = &semaphore_imgui[current_frame_in_flight];

			vkQueueSubmit(Device.GetQueue(POOL_FAMILY::COMPUTE), 1, &submit_info_imgui, inFlightFences[current_frame_in_flight]);
		}

		//submit presentation
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		VkSwapchainKHR swapChains[] = { Device.GetSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		
		presentInfo.waitSemaphoreCount = 1;
		VkSemaphore sem = semaphore_computepass[current_frame_in_flight];
		if (enable_imgui)
		{
			sem = semaphore_imgui[current_frame_in_flight];
		}
		presentInfo.pWaitSemaphores = &sem;

		VULKAN_CHECK(vkQueuePresentKHR(Device.GetQueue(POOL_FAMILY::PRESENT), &presentInfo), "presenting image");


		current_frame_in_flight = (current_frame_in_flight + 1) % FRAMES_IN_FLIGHT;
//#ifdef _DEBUG
		cpu_timer3.Stop(cpu_tmp);
		cpu_time += cpu_tmp;
		time_cpu[time_counter] = cpu_time;
//#endif
	}

	void RenderManager::Update()
	{
		glfwPollEvents();
		
		//handle imgui variables
		if (enable_imgui)
		{
			FOV = imguifov;
			SetProjectionMatrix();
			atmosphere->UpdateFarPlane(window_extent, FOV);
			if (multi_changed)
			{
				if (imguimulti_count > -1 && imguimulti_count < 6)
					SetMultisampling((SAMPLE_COUNT)imguimulti_count);
				multi_changed = false;
			}
			if (full_screenchanged)
			{
				if (imguifullscreen)
				{
					WindowManager.SetFullScreen(1920, 1080);
				}
				else
				{
					WindowManager.SetWindowedMode(0, 0, 1920, 1080);
				}
				full_screenchanged = false;
			}
			if (resolution_changed == true)
			{
				//{ "Fit_Window", "High", "Medium", "Low"};
				switch (imguiresolution)
				{
				case 0: SetResolutionFitted(true); Recreateswapchain(); break;
				case 1: SetResolutionFitted(false); SetResolution(RESOLUTION::TENEIGHTYP); break;
				case 2: SetResolutionFitted(false); SetResolution(RESOLUTION::SEVENTWENTYP); break;
				case 3: SetResolutionFitted(false); SetResolution(RESOLUTION::FOUREIGHTYP); break;
				}
				resolution_changed = false;
			}
		}

		Render();
	}

	bool RenderManager::IsWindowOpen() const
	{
		return glfwWindowShouldClose(WindowManager.Getwindow());
	}

	void RenderManager::SetWindowTitle(std::string title) const
	{
		WindowManager.SetWindowTitle(title);
	}

	void RenderManager::AddRenderObject(RenderObject* object, RenderObjectManager::BIN_TYPE type)
	{
		//first make sure to add it to objectmanager. This sets the objects id, and pushes it to all data-structures
		objectmanager->AddRenderObject(object, type);

		//now add local descriptor set for every shader that needs this object
	
		//PBR
		DescriptorHelper local_descriptors(FRAMES_IN_FLIGHT, 1, 4);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{

			local_descriptors.buffersizes[i].emplace_back(sizeof(Material::materialinfo));
			local_descriptors.uniformbuffers[i].emplace_back(object->GetMaterial().GetBuffer());

			local_descriptors.imageviews[i].emplace_back(object->GetMaterial().GetAlbedoMap().view);
			local_descriptors.samplers[i].emplace_back(object->GetMaterial().GetAlbedoMap().sampler);

			local_descriptors.imageviews[i].emplace_back(object->GetMaterial().GetSpecularMap().view);
			local_descriptors.samplers[i].emplace_back(object->GetMaterial().GetSpecularMap().sampler);

			local_descriptors.imageviews[i].emplace_back(object->GetMaterial().GetMetalMap().view);
			local_descriptors.samplers[i].emplace_back(object->GetMaterial().GetMetalMap().sampler);

			local_descriptors.imageviews[i].emplace_back(object->GetMaterial().GetNormalMap().view);
			local_descriptors.samplers[i].emplace_back(object->GetMaterial().GetNormalMap().sampler);

		}
		program_pbr.AddLocalDescriptor(object->GetId(), local_descriptors.uniformbuffers, local_descriptors.buffersizes, local_descriptors.imageviews, local_descriptors.samplers, local_descriptors.bufferviews);

		//
	}

	/*
		Some notes on this - obviously when we delete objects we have to handle frames in flight. Pretty much its not that hard as long as everything
		that has to deal with renderobjects follows same rules:
		Make sure none of the future frames reference anything related to this object like descriptors/memory/etc
		Make sure we don't remove any memory being used in the frames using it
		after x amount of frames have passed make sure to just remove the memory and order shouldn't matter because it shouldn't be used by anything
	*/
	void RenderManager::RemoveRenderObject(RenderObject* object, RenderObjectManager::BIN_TYPE type)
	{
		objectmanager->RemoveRenderObject(object, type);

		//add objects id to the descriptorgraveyard.
		descriptorgraveyard.push_back(descriptorgraveinfo(object->GetId(), 0));
	}

	//wait for frame key before calling 
	void RenderManager::UpdateDescriptorGraveYard()
	{
		//we assume every object goes to every one of these shaders for now.
		for (int i = 0; i < descriptorgraveyard.size(); i++)
		{
			descriptorgraveyard[i].frame_count++;

			if (descriptorgraveyard[i].frame_count > FRAMES_IN_FLIGHT)
			{
				//remove descriptor set for all frames for this renderobject because all use of the resources are done and the renderobject is being destroyed this frame as well
				
				//PBR
				program_pbr.RemoveLocalDescriptor(descriptorgraveyard[i].id);
				
				//


				descriptorgraveyard.erase(descriptorgraveyard.begin() + i);
			}
		}
	}
}