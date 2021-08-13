#include "../pch.h"
#include "RenderManager.h"
#include "vkcore/VulkanHelpers.h"

#include "../ThirdParty/ImGui/imgui.h"
#include "../ThirdParty/ImGui/imgui_impl_glfw.h"
#include "../ThirdParty/ImGui/imgui_impl_vulkan.h"
#include "RenderObject.h"
#include "ShadowAlgorithms.h"

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

		CleanUpDepth();
		CleanUpReduce();
		CleanUpShadow();
		CleanUpPBR();
		CleanUpCompute();
		CleanUpGui();
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
		
		for (int i = 0; i < cascade_buffer.size(); i++)
		{
			Device.DestroyBuffer(cascade_buffer[i]);
			Device.DestroyBuffer(point_pbrbuffer[i]);
			Device.DestroyBuffer(point_infobuffer[i]);
		}

		program_pbr.CleanUp();

		for (int i = 0; i < cmdbuffer_pbr.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS), 1, &cmdbuffer_pbr[i]);
		}
	}

	void RenderManager::CleanUpCompute()
	{
		program_compute.CleanUp();
		Computedeleteimagedata();

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
			vkDestroySemaphore(Device.GetDevice(), semaphore_depth[i], nullptr);
			vkDestroySemaphore(Device.GetDevice(), semaphore_reduce[i], nullptr);
			vkDestroySemaphore(Device.GetDevice(), semaphore_shadow[i], nullptr);
			vkDestroySemaphore(Device.GetDevice(), semaphore_colorpass[i], nullptr);
			vkDestroySemaphore(Device.GetDevice(), semaphore_gui[i], nullptr);
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
		//Logger::LogError(ConvertSampleCount(count));
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

		Depthdeleteimagedata();
		Depthcreateimagedata();

		Reducedeleteimagedata();
		Reducecreateimagedata();

		Shadowdeleteimagedata();
		Shadowcreateimagedata();

		PBRdeleteimagedata();
		PBRcreateimagedata();

		Computedeleteimagedata();
		Computecreateimagedata();

		Guideleteimagedata();
		Guicreateimagedata();

		Quaddeleteimagedata();
		Quadcreateimagedata();

		atmosphere->SwapChainRecreate(Resolution, window_extent, FOV, renderpass_pbr, multisampling_count);

		createReducefinal();
		createPBRfinal();

		if (enable_imgui)
		{
			for (int i = 0; i < framebuffer_imgui.size(); i++)
			{
				vkDestroyFramebuffer(Device.GetDevice(), framebuffer_imgui[i], nullptr);
			}
			for (int i = 0; i < Device.GetSwapChainImageCount(); i++)
			{
				VkImageView views = { Device.GetswapchainView(i) };
				framebuffer_imgui[i] = CreateFrameBuffer(Device.GetDevice(), window_extent.width, window_extent.height, renderpass_imgui, &views, 1);
			}
			ImGui_ImplVulkanH_Window info = {};
			//ImGui_ImplVulkanH_CreateOrResizeWindow(Device.GetInstace(), Device.GetPhysicalDevice(), Device.GetDevice(), &g_MainWindowData, PhysicalDeviceQuery::GetQueueFamily(Device.GetPhysicalDevice(), VK_QUEUE_GRAPHICS_BIT),
				//nullptr, window_extent.width, window_extent.height, FRAMES_IN_FLIGHT);
		}

		SetProjectionMatrix();
		vkDeviceWaitIdle(Device.GetDevice());
	}

	void RenderManager::PBRcreateimagedata()
	{
		//renderpasss
        main_color_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		RenderPassCache& rpcache = Device.GetRenderPassCache();
		if (multisampling_count == VK_SAMPLE_COUNT_1_BIT)
		{
			RenderPassAttachment colorattachment(0, main_color_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::COLOR);
			RenderPassAttachment depthattachment(1, findDepthFormat(Device.GetPhysicalDevice()), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				RENDERPASSTYPE::DEPTH);
			std::vector<RenderPassAttachment> attachments = { colorattachment, depthattachment };
			renderpass_pbr = rpcache.GetRenderPass(attachments.data(), attachments.size(), VK_PIPELINE_BIND_POINT_GRAPHICS);
		}
		else
		{
			RenderPassAttachment colorattachment(0, main_color_format, multisampling_count, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::COLOR);
			RenderPassAttachment depthattachment(1, findDepthFormat(Device.GetPhysicalDevice()), multisampling_count, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				RENDERPASSTYPE::DEPTH);
			RenderPassAttachment resolveattachment(2, main_color_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::RESOLVE);
			std::vector<RenderPassAttachment> attachments = { colorattachment, depthattachment, resolveattachment };
			renderpass_pbr = rpcache.GetRenderPass(attachments.data(), attachments.size(), VK_PIPELINE_BIND_POINT_GRAPHICS);
		}

		//image attachments
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), main_color_format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
		{
			Logger::LogError("format not supported for main color attachment\n");
		}
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), main_color_format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		{
			Logger::LogError("storage format not supported for storage image bit\n");
		}
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), main_color_format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
		{
			Logger::LogError("storage format not supported for sampled image bit\n");
		}

		color_attachment.resize(FRAMES_IN_FLIGHT);
		color_attachmentview.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			VkImageUsageFlags color_usage = (multisampling_count == VK_SAMPLE_COUNT_1_BIT) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			Device.CreateImage(VK_IMAGE_TYPE_2D, main_color_format, color_usage, multisampling_count, Resolution.width, Resolution.height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, color_attachment[i]);

			color_attachmentview[i] = CreateImageView(Device.GetDevice(), color_attachment[i].image, main_color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
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
			VkImageUsageFlags resolve_usage =  VK_IMAGE_USAGE_STORAGE_BIT; // VK_IMAGE_USAGE_STORAGE_BIT
			Device.CreateImage(VK_IMAGE_TYPE_2D, main_color_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | resolve_usage, VK_SAMPLE_COUNT_1_BIT, Resolution.width, Resolution.height, 1, 1, 1,
				VMA_MEMORY_USAGE_GPU_ONLY, 0, resolve_attachment[i]);

			resolve_attachmentview[i] = CreateImageView(Device.GetDevice(), resolve_attachment[i].image, main_color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		}

		//framebuffer
		framebuffer_pbr.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			if (multisampling_count == VK_SAMPLE_COUNT_1_BIT)
			{
				std::vector<VkImageView> imageview = { color_attachmentview[i], depthprepass_view[i] };
				framebuffer_pbr[i] = CreateFrameBuffer(Device.GetDevice(), Resolution.width, Resolution.height, renderpass_pbr, imageview.data(), imageview.size());
			}
			else
			{
				std::vector<VkImageView> imageview = { color_attachmentview[i], depthprepass_view[i], resolve_attachmentview[i] };
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

		pipelinedata.Rasterizationstate.cullmode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_BACK_BIT
		pipelinedata.Rasterizationstate.frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelinedata.Rasterizationstate.polygonmode = VK_POLYGON_MODE_FILL;

		pipelinedata.DepthStencilstate.depthtestenable = VK_TRUE;
		pipelinedata.DepthStencilstate.depthcompareop = VK_COMPARE_OP_LESS_OR_EQUAL;
		pipelinedata.DepthStencilstate.depthwriteenable = VK_TRUE; //we have a depth prepass so we don't need to update just test depth value

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

		//IMGUI - if imgui is one we need to transition it from general to colorattachment, else put it to present
		VkImageLayout transition_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		if (enable_imgui)
		{
			transition_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		RenderPassCache& rpcache = Device.GetRenderPassCache();
		RenderPassAttachment colorattachment(0, Device.GetswapchainFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, transition_layout,
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
		//framebuffer
		framebuffers_quad.resize(Device.GetSwapChainImageCount());
		for (int i = 0; i < Device.GetSwapChainImageCount(); i++)
		{
			VkImageView views = { Device.GetswapchainView(i) };
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
			global_descriptors.imageviews[i].push_back(ppcolor_attachmentview[i]);
			global_descriptors.samplers[i].push_back(sampler_quad);
		}
		program_quad.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);
	}

	void RenderManager::Quaddeleteimagedata()
	{
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
		begin_rp.framebuffer = framebuffers_quad[current_imageindex];
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
		framebuffer_imgui.resize(Device.GetSwapChainImageCount());
		for (int i = 0; i < Device.GetSwapChainImageCount(); i++)
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
		init_info.MinImageCount = Device.GetSwapChainImageCount();
		init_info.ImageCount = Device.GetSwapChainImageCount();
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
		ImGui::ShowDemoWindow();

		ImGui::Begin("timers");
		
		//ImGui::Checkbox
		//ImGui::ColorEdit3
		//ImGui::PlotHistogram
		//ImGui::Text
		
		char overlay[32]; 
		
		sprintf_s(overlay, "%f milliseconds", time_depth[time_counter]);
		ImGui::PlotLines("depth prepass", time_depth.data(), time_depth.size(), 0, overlay, 0.0f, 32.0f, ImVec2(0, 80.0f));

		sprintf_s(overlay, "%f milliseconds", time_reduce[time_counter]);
		ImGui::PlotLines("depth reduce", time_reduce.data(), time_reduce.size(), 0, overlay, 0.0f, 32.0f, ImVec2(0, 80.0f));

		sprintf_s(overlay, "%f milliseconds", time_shadow[time_counter]);
		ImGui::PlotLines("shadow pass", time_shadow.data(), time_shadow.size(), 0, overlay, 0.0f, 32.0f, ImVec2(0, 80.0f));

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

		//shadow map depth bias
		sprintf_s(overlay2, "%f ", bias_info.x);
		ImGui::SliderFloat("constant bias", &bias_info.x, 0, .1, overlay2, 1);
		sprintf_s(overlay2, "%f ", bias_info.y);
		ImGui::SliderFloat("normal bias", &bias_info.y, 0, .1, overlay2, 1);
		sprintf_s(overlay2, "%f ", bias_info.z);
		ImGui::SliderFloat("slope bias", &bias_info.z, 0, .1, overlay2, 1);

		const char* items2[] = { "NONE(0)", "NN(9)", "NN(16)", "NN(25)", "Bilinear(16)", "Bilinear(36)", "Bilinear(64)", "BiCubic(16)", "Gauss(9)", "Gauss(25)", "Gauss(49)", "Poisson(16)",
		"Witness(4)", "Witness(9)", "Witness(16)"};
		ImGui::Combo("PCF: ", &pcf_method, items2, 15);
		bias_info.w = pcf_method;

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

		CreateDepth();
		CreateReduce();
		CreateShadow();
		CreatePBR();
		CreateAtmosphere();
		atmosphere->UpdateCamPosition(glm::vec4(0, 0, 0, 1));
		CreateCompute();
		CreateGui();

		createDepthfinal();
		createReducefinal();
		createShadowFinal();
		createPBRfinal();
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
		semaphore_depth.resize(FRAMES_IN_FLIGHT);
		semaphore_reduce.resize(FRAMES_IN_FLIGHT);
		semaphore_shadow.resize(FRAMES_IN_FLIGHT);
		semaphore_colorpass.resize(FRAMES_IN_FLIGHT);
		semaphore_gui.resize(FRAMES_IN_FLIGHT);
		semaphore_computepass.resize(FRAMES_IN_FLIGHT);
		semaphore_quad.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < semaphore_imagefetch.size(); i++)
		{
			VkSemaphoreCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			info.flags = 0;

			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_imagefetch[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_depth[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_reduce[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_shadow[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_colorpass[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_gui[i]);
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
		Computecreateimagedata();

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

	void RenderManager::Computecreateimagedata()
	{
		ppcolor_attachment.resize(FRAMES_IN_FLIGHT);
		ppcolor_attachmentview.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateImage(VK_IMAGE_TYPE_2D, main_color_format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
				VK_SAMPLE_COUNT_1_BIT, Resolution.width, Resolution.height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, ppcolor_attachment[i]);

			ppcolor_attachmentview[i] = CreateImageView(Device.GetDevice(), ppcolor_attachment[i].image, main_color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		}
	}

	void RenderManager::Computedeleteimagedata()
	{
		for (int i = 0; i < ppcolor_attachment.size(); i++)
		{
			Device.DestroyImage(ppcolor_attachment[i]);
			vkDestroyImageView(Device.GetDevice(), ppcolor_attachmentview[i], nullptr);
		}
	}

	void RenderManager::RecordComputeCmd(int current_frame, int current_imageindex)
	{
		//we have to set the current_frames global descriptorset to point to the specific swapchain image we have this frame in flight.
		DescriptorHelper global_descriptors(1);
		global_descriptors.imageviews[0].push_back((multisampling_count == VK_SAMPLE_COUNT_1_BIT) ? color_attachmentview[current_frame] : resolve_attachmentview[current_frame]);
		global_descriptors.samplers[0].push_back(atmosphere->GetSampler());
		global_descriptors.imageviews[0].push_back(ppcolor_attachmentview[current_frame]);
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

		TransitionImageLayout(Device, ppcolor_attachment[current_frame].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_HOST_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, cmdbuffer_compute[current_frame]);

		float WORKGROUP_SIZE = 32.0;
		vkCmdBindPipeline(cmdbuffer_compute[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_compute[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute.layout, 0, 1, &program_compute.GetGlobalDescriptor(current_frame), 0, nullptr);
		vkCmdDispatch(cmdbuffer_compute[current_frame], std::ceil(Resolution.width / WORKGROUP_SIZE), std::ceil(Resolution.height / WORKGROUP_SIZE), 1);
	
		TransitionImageLayout(Device, ppcolor_attachment[current_frame].image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, cmdbuffer_compute[current_frame]);
		
		//set timer here at bottom of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_compute[current_frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::POST_PROCESS, false);

		VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_compute[current_frame]), "end compute cmdbuffer");
	}

	void RenderManager::CreateGui()
	{
		//mesh
		shadowmesh_gui = meshCache->GetQuadMesh();

		//renderpass
		RenderPassAttachment color_attachment(0, main_color_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			RENDERPASSTYPE::COLOR);
		renderpass_gui = Device.GetRenderPassCache().GetRenderPass(&color_attachment, 1, VK_PIPELINE_BIND_POINT_GRAPHICS);

		//program
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/guivert.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/guifrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
			{"shadow_map", 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo1 = {
			{"shadow_mapmain", 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants1 = {
			{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)}
		};
		if (!program_gui.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(), pushconstants1.data(), pushconstants1.size(), 
			8))
		{
			Logger::LogError("failed to create pp shaderprogram\n");
		}

		Guicreateimagedata();

		//commandbuffer
		cmdbuffer_gui.resize(FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo cmd_info = {};
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS);
		cmd_info.commandBufferCount = cmdbuffer_gui.size();
		vkAllocateCommandBuffers(Device.GetDevice(), &cmd_info, cmdbuffer_gui.data());
	}

	void RenderManager::CleanUpGui()
	{
		Guideleteimagedata();

		vkDestroyRenderPass(Device.GetDevice(), renderpass_gui, nullptr);
		
		program_gui.CleanUp();

		for (int i = 0; i < cmdbuffer_gui.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS), 1, &cmdbuffer_gui[i]);
		}
	}

	void RenderManager::Guicreateimagedata()
	{
		//framebuffers
		framebuffer_gui.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			VkImageView views = { ppcolor_attachmentview[i] };
			framebuffer_gui[i] = CreateFrameBuffer(Device.GetDevice(), Resolution.width, Resolution.height, renderpass_gui, &views, 1);
		}

		//descriptors global
		/*DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			global_descriptors.imageviews[i].push_back(shadowcascade_depthview[i][0]);
			global_descriptors.samplers[i].push_back(shadowmapsampler);
		}
		program_gui.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);
		*/
		//add 3 shadow maps

		//cascade atlas
		{
			DescriptorHelper local_descriptors(FRAMES_IN_FLIGHT);
			for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
			{
				//local_descriptors.imageviews[i].emplace_back(shadowcascade_depthview[i][c]);
				local_descriptors.imageviews[i].emplace_back(shadowcascade_atlasview[i]);
				local_descriptors.samplers[i].emplace_back(shadowmapsampler);
			}
			program_gui.AddLocalDescriptor(0, local_descriptors.uniformbuffers, local_descriptors.buffersizes, local_descriptors.imageviews, local_descriptors.samplers, local_descriptors.bufferviews);
		}
		
		//point/spot atlas
		DescriptorHelper local_descriptors(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			local_descriptors.imageviews[i].emplace_back(shadowpoint_atlasview[i]);
			local_descriptors.samplers[i].emplace_back(shadowmapsampler);
		}
		program_gui.AddLocalDescriptor(1, local_descriptors.uniformbuffers, local_descriptors.buffersizes, local_descriptors.imageviews, local_descriptors.samplers, local_descriptors.bufferviews);


		/*DescriptorHelper local_descriptors(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			local_descriptors.imageviews[i].emplace_back(shadowcascade_depthview[i][0]);
			local_descriptors.samplers[i].emplace_back(shadowmapsampler);
		}
		program_gui.AddLocalDescriptor(0, local_descriptors.uniformbuffers, local_descriptors.buffersizes, local_descriptors.imageviews, local_descriptors.samplers, local_descriptors.bufferviews);

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			local_descriptors.imageviews[i][0] = shadowcascade_depthview[i][1];
			local_descriptors.samplers[i][0] = shadowmapsampler;
		}
		program_gui.AddLocalDescriptor(1, local_descriptors.uniformbuffers, local_descriptors.buffersizes, local_descriptors.imageviews, local_descriptors.samplers, local_descriptors.bufferviews);

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			local_descriptors.imageviews[i][0] = shadowcascade_depthview[i][2];
			local_descriptors.samplers[i][0] = shadowmapsampler;
		}
		program_gui.AddLocalDescriptor(2, local_descriptors.uniformbuffers, local_descriptors.buffersizes, local_descriptors.imageviews, local_descriptors.samplers, local_descriptors.bufferviews);
		*/
		/*for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			local_descriptors.imageviews[i][0] = depthprepass_view[i];
			local_descriptors.samplers[i][0] = shadowmapsampler;
		}
		program_gui.AddLocalDescriptor(3, local_descriptors.uniformbuffers, local_descriptors.buffersizes, local_descriptors.imageviews, local_descriptors.samplers, local_descriptors.bufferviews);
		*/


		//pipeline
		PipelineCache& pipecache = Device.GetPipelineCache();
		PipelineData pipelinedata(Resolution.width, Resolution.height);

		pipelinedata.ColorBlendstate.blendenable = VK_FALSE;

		pipelinedata.Rasterizationstate.cullmode = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT
		pipelinedata.Rasterizationstate.frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelinedata.Rasterizationstate.polygonmode = VK_POLYGON_MODE_FILL;

		pipelinedata.DepthStencilstate.depthtestenable = VK_FALSE;
		pipelinedata.DepthStencilstate.depthwriteenable = VK_FALSE;
		pipelinedata.DepthStencilstate.stenciltestenable = VK_FALSE;

		pipelinedata.Multisamplingstate.samplecount = VK_SAMPLE_COUNT_1_BIT;
		pipelinedata.Multisamplingstate.sampleshadingenable = VK_FALSE;

		pipelinedata.Inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		std::vector<VkDescriptorSetLayout> layoutsz = { program_gui.GetGlobalLayout(), program_gui.GetLocalLayout() };

		pipeline_gui = pipecache.GetGraphicsPipeline(pipelinedata, Device.GetPhysicalDevice(), renderpass_gui, program_gui.GetShaderStageInfo(), program_gui.GetPushRanges(), layoutsz.data(), layoutsz.size());
	}

	void RenderManager::Guideleteimagedata()
	{
		for (int i = 0; i < framebuffer_gui.size(); i++)
		{
			vkDestroyFramebuffer(Device.GetDevice(), framebuffer_gui[i], nullptr);
		}

		program_gui.RemoveLocalDescriptor(0);
		program_gui.RemoveLocalDescriptor(1);

		vkDestroyPipeline(Device.GetDevice(), pipeline_gui.pipeline, nullptr);
		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_gui.layout, nullptr);
	}

	void RenderManager::RecordGuiCmd(int current_frame)
	{
		vkResetCommandBuffer(cmdbuffer_gui[current_frame], 0);

		//commandbuffer
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdbuffer_gui[current_frame], &begin_info);
		VkRenderPassBeginInfo begin_rp = {};
		begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.renderPass = renderpass_gui;
		begin_rp.framebuffer = framebuffer_gui[current_frame];
		begin_rp.renderArea.offset = { 0,0 };
		begin_rp.renderArea.extent = { Resolution.width, Resolution.height };
		std::array<VkClearValue, 1> clearValues = {};
		clearValues[0].color = { 0.0f, 1.0f, 0.0f, 1.0f };
		begin_rp.pClearValues = clearValues.data();
		begin_rp.clearValueCount = clearValues.size();

		//reset both ones
		//Device.GetQueryManager().ResetQueries(cmdbuffer_quad[current_frame], current_frame, QueryManager::QUERY_NAME::QUAD);

		//set timer here at top of pipeline
		//Device.GetQueryManager().WriteTimeStamp(cmdbuffer_quad[current_frame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::QUAD, true);

		vkCmdBeginRenderPass(cmdbuffer_gui[current_frame], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmdbuffer_gui[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_gui.pipeline);
		//vkCmdBindDescriptorSets(cmdbuffer_gui[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_gui.layout, 0, 1, &program_gui.GetGlobalDescriptor(current_frame), 0, nullptr);

		//draw cascaded shadow maps
		for (int c = 0; c < 1; c++)
		{
			vkCmdBindDescriptorSets(cmdbuffer_gui[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_gui.layout, 1, 1, &program_gui.GetLocalDescriptor(c, current_frame), 0, nullptr);
			glm::mat4 gui_matrix_scale = glm::scale(glm::mat4(1.0), glm::vec3(.1, .1, 1));
			glm::mat4 gui_matrix_translate = glm::translate(glm::mat4(1.0), glm::vec3(-0.9, -0.9 + .3*c, 0));
			gui_matrix_scale = gui_matrix_translate * gui_matrix_scale;
			vkCmdPushConstants(cmdbuffer_gui[current_frame], pipeline_gui.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &gui_matrix_scale);
			VkDeviceSize sizes[] = { 0 };
			vkCmdBindVertexBuffers(cmdbuffer_gui[current_frame], 0, 1, &mesh_quad.vbo, sizes);
			vkCmdBindIndexBuffer(cmdbuffer_gui[current_frame], mesh_quad.ibo, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdbuffer_gui[current_frame], mesh_quad.index_size, 1, 0, 0, 0);
		}

		vkCmdBindDescriptorSets(cmdbuffer_gui[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_gui.layout, 1, 1, &program_gui.GetLocalDescriptor(1, current_frame), 0, nullptr);
		glm::mat4 gui_matrix_scale = glm::scale(glm::mat4(1.0), glm::vec3(.3, .3, 3));
		glm::mat4 gui_matrix_translate = glm::translate(glm::mat4(1.0), glm::vec3(-0.7, -0.9 + .3*3, 0));
		gui_matrix_scale = gui_matrix_translate * gui_matrix_scale;
		vkCmdPushConstants(cmdbuffer_gui[current_frame], pipeline_gui.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &gui_matrix_scale);
		VkDeviceSize sizes[] = { 0 };
		vkCmdBindVertexBuffers(cmdbuffer_gui[current_frame], 0, 1, &mesh_quad.vbo, sizes);
		vkCmdBindIndexBuffer(cmdbuffer_gui[current_frame], mesh_quad.ibo, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmdbuffer_gui[current_frame], mesh_quad.index_size, 1, 0, 0, 0);

		//draw depth prepass
		/*vkCmdBindDescriptorSets(cmdbuffer_gui[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_gui.layout, 1, 1, &program_gui.GetLocalDescriptor(3, current_frame), 0, nullptr);
		gui_matrix_scale = glm::scale(glm::mat4(1.0), glm::vec3(.1, .1, 1));
		gui_matrix_translate = glm::translate(glm::mat4(1.0), glm::vec3(-0.9, 0.0, 0));
		gui_matrix_scale = gui_matrix_translate * gui_matrix_scale;
		vkCmdPushConstants(cmdbuffer_gui[current_frame], pipeline_gui.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &gui_matrix_scale);
		vkCmdBindVertexBuffers(cmdbuffer_gui[current_frame], 0, 1, &mesh_quad.vbo, sizes);
		vkCmdBindIndexBuffer(cmdbuffer_gui[current_frame], mesh_quad.ibo, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmdbuffer_gui[current_frame], mesh_quad.index_size, 1, 0, 0, 0);
		*/
		vkCmdEndRenderPass(cmdbuffer_gui[current_frame]);

		//set timer here at bottom of pipeline
		//Device.GetQueryManager().WriteTimeStamp(cmdbuffer_quad[current_frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::QUAD, false);

		vkEndCommandBuffer(cmdbuffer_gui[current_frame]);
	}

	void RenderManager::CreateShadow()
	{
		cascade_nears.resize(FRAMES_IN_FLIGHT);
		pointsize_current.resize(FRAMES_IN_FLIGHT);
		spotsize_current.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			pointsize_current[i] = 0;
			spotsize_current[i] = 0;
		}

		//shader program
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/shadowvert.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/shadowfrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo1 = {
			{"ProjVertexBuffer", 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
			{"nearBuffer", 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants1 = {
			{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)}
		};

		if (!program_shadow.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(),
			pushconstants1.data(), pushconstants1.size(), 10))
		{
			Logger::LogError("failed to create shadow shaderprogram\n");
		}

		//shader program
		std::vector<ShaderProgram::shadersinfo> info2 = {
			{"Shaders/spv/shadowpointvert.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/shadowpointfrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo2 = {
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo2 = {
			{"ProjVertexBuffer", 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants2 = {
			{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)}
		};

		if (!program_shadowpoint.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info2.data(), info2.size(), globalinfo2.data(), globalinfo2.size(), localinfo2.data(), localinfo2.size(),
			pushconstants2.data(), pushconstants2.size(), MAX_POINT_IMAGES + 6))
		{
			Logger::LogError("failed to create point shadow shaderprogram\n");
		}

		//commandbuffer
		cmdbuffer_shadow.resize(FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo cmd_info = {};
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS);
		cmd_info.commandBufferCount = cmdbuffer_shadow.size();
		vkAllocateCommandBuffers(Device.GetDevice(), &cmd_info, cmdbuffer_shadow.data());

		Shadowcreateimagedata();
	}

	void RenderManager::createShadowFinal()
	{
		//create and set descriptors
		shadowcascade_pv_buffers.resize(FRAMES_IN_FLIGHT);
		shadowcascade_nearplane_buffers.resize(FRAMES_IN_FLIGHT);
		shadowpoint_pv_buffers.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			shadowcascade_pv_buffers[i].resize(CASCADE_COUNT);
			shadowcascade_nearplane_buffers[i].resize(CASCADE_COUNT);
			for (int c = 0; c < CASCADE_COUNT; c++)
			{
				Device.CreateBuffer(sizeof(glm::mat4) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, shadowcascade_pv_buffers[i][c]);
				Device.CreateBuffer(sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, shadowcascade_nearplane_buffers[i][c]);
			}
			shadowpoint_pv_buffers[i].resize(MAX_POINT_IMAGES);
			for (int j = 0; j < MAX_POINT_IMAGES; j++)
			{
				Device.CreateBuffer(sizeof(glm::mat4) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, shadowpoint_pv_buffers[i][j]);
			}
		}

		cascade_depthbuffers.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateBuffer(sizeof(glm::vec4) * (MAX_CASCADES - 1), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, cascade_depthbuffers[i]);
		}

		//cascade descriptors
		for (int c = 0; c < CASCADE_COUNT; c++)
		{
			DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);
			for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
			{
				global_descriptors.buffersizes[i].push_back(sizeof(glm::mat4) * 2);
				global_descriptors.uniformbuffers[i].push_back(shadowcascade_pv_buffers[i][c]);

				global_descriptors.buffersizes[i].push_back(sizeof(float));
				global_descriptors.uniformbuffers[i].push_back(shadowcascade_nearplane_buffers[i][c]);
			}

			program_shadow.AddLocalDescriptor(c, global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers,
				global_descriptors.bufferviews);
		}	
	}

	void RenderManager::Shadowcreateimagedata()
	{

		RenderPassCache& rpcache = Device.GetRenderPassCache();
		//VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		RenderPassAttachment depthattachment(0, cascadedshadow_map_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::DEPTH);
		renderpass_shadow = rpcache.GetRenderPass(&depthattachment, 1, VK_PIPELINE_BIND_POINT_GRAPHICS);

		RenderPassAttachment depthattachmentpoint(0, pointspot_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::DEPTH);
		renderpass_shadowpoint = rpcache.GetRenderPass(&depthattachmentpoint, 1, VK_PIPELINE_BIND_POINT_GRAPHICS);

		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), cascadedshadow_map_format, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
		{
			Logger::LogError("shadow map format not supported for depth attachment\n");
		}
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), cascadedshadow_map_format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
		{
			Logger::LogError("shadow map format not supported for sampled image bit\n");
		}
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), pointspot_format, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
		{
			Logger::LogError("point shadow map format not supported for depth attachment\n");
		}
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), pointspot_format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
		{
			Logger::LogError("point shadow map format not supported for sampled image bit\n");
		}

		std::cout << "atlas_width: " << atlas_width << " atlas_height: " << atlas_height << std::endl;

		shadowcascade_atlasimage.resize(FRAMES_IN_FLIGHT);
		shadowcascade_atlasview.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateImage(VK_IMAGE_TYPE_2D, cascadedshadow_map_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT,
				atlas_width, atlas_height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, shadowcascade_atlasimage[i]);

			shadowcascade_atlasview[i] = CreateImageView(Device.GetDevice(), shadowcascade_atlasimage[i].image, cascadedshadow_map_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1,
				VK_IMAGE_VIEW_TYPE_2D);

			TransitionImageLayout(Device, shadowcascade_atlasimage[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_HOST_WRITE_BIT,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
		}

		shadowpoint_atlasimage.resize(FRAMES_IN_FLIGHT);
		shadowpoint_atlasview.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			//point/spot atlas
			Device.CreateImage(VK_IMAGE_TYPE_2D, pointspot_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT,
				atlaspoint_width, atlaspoint_height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, shadowpoint_atlasimage[i]);

			shadowpoint_atlasview[i] = CreateImageView(Device.GetDevice(), shadowpoint_atlasimage[i].image, pointspot_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1,
				VK_IMAGE_VIEW_TYPE_2D);

			TransitionImageLayout(Device, shadowpoint_atlasimage[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_HOST_WRITE_BIT,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
		}

		SamplerKey key(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			VK_FALSE, 1.0f, VK_BORDER_COLOR_INT_OPAQUE_WHITE, VK_SAMPLER_MIPMAP_MODE_NEAREST, 0.0, 1.0, 0.0);
		shadowmapsampler = Device.GetSamplerCache().GetSampler(key);

		SamplerKey key2(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			VK_FALSE, 1.0f, VK_BORDER_COLOR_INT_OPAQUE_WHITE, VK_SAMPLER_MIPMAP_MODE_NEAREST, 0.0, 1.0, 0.0);
		shadowcubemapsampler = Device.GetSamplerCache().GetSampler(key2);

		framebuffer_shadowcascadesatlas.resize(FRAMES_IN_FLIGHT);
		framebuffer_shadowpoints.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			framebuffer_shadowcascadesatlas[i] = CreateFrameBuffer(Device.GetDevice(), atlas_width, atlas_height, renderpass_shadow, &shadowcascade_atlasview[i], 1);
			//point/spot atlas
			framebuffer_shadowpoints[i] = CreateFrameBuffer(Device.GetDevice(), atlaspoint_width, atlaspoint_height, renderpass_shadowpoint, &shadowpoint_atlasview[i], 1, 1);
		}

		PipelineCache& pipecache = Device.GetPipelineCache();

		//cascade pipeline
		PipelineData pipelinedata(shadow_width, shadow_height);

		pipelinedata.ColorBlendstate.blendenable = VK_FALSE;

		pipelinedata.Rasterizationstate.cullmode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_BACK_BIT
		pipelinedata.Rasterizationstate.frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelinedata.Rasterizationstate.polygonmode = VK_POLYGON_MODE_FILL;
		pipelinedata.Rasterizationstate.depthBiasEnable = VK_FALSE;
		pipelinedata.Rasterizationstate.depthBiasConstantFactor = 0.003;
		pipelinedata.Rasterizationstate.depthBiasSlopeFactor = 0.003;
		pipelinedata.Rasterizationstate.depthBiasEnable = VK_FALSE;
		pipelinedata.Rasterizationstate.depthBiasClamp = 0;

		pipelinedata.DepthStencilstate.depthtestenable = VK_TRUE;
		pipelinedata.DepthStencilstate.depthcompareop = VK_COMPARE_OP_LESS;
		pipelinedata.DepthStencilstate.depthwriteenable = VK_TRUE;
		pipelinedata.DepthStencilstate.stenciltestenable = VK_FALSE;
		pipelinedata.DepthStencilstate.boundsenable = VK_FALSE;
		pipelinedata.DepthStencilstate.min_bounds = 0.0f;
		pipelinedata.DepthStencilstate.max_bounds = 1.0f;

		pipelinedata.Multisamplingstate.samplecount = VK_SAMPLE_COUNT_1_BIT;
		pipelinedata.Multisamplingstate.sampleshadingenable = VK_FALSE;

		pipelinedata.ViewPortstate.dynamicviewport = true;

		pipelinedata.Inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		std::vector<VkDescriptorSetLayout> layoutsz = { program_shadow.GetGlobalLayout(), program_shadow.GetLocalLayout() };

		pipeline_shadow = pipecache.GetGraphicsPipeline(pipelinedata, Device.GetPhysicalDevice(), renderpass_shadow, program_shadow.GetShaderStageInfo(), program_shadow.GetPushRanges(), layoutsz.data(), layoutsz.size());
		
		//point pipeline
		PipelineData pipelinedata2(shadowpoint_width, shadowpoint_height);

		pipelinedata2.ColorBlendstate.blendenable = VK_FALSE;

		pipelinedata2.Rasterizationstate.cullmode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_BACK_BIT
		pipelinedata2.Rasterizationstate.frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelinedata2.Rasterizationstate.polygonmode = VK_POLYGON_MODE_FILL;

		pipelinedata2.DepthStencilstate.depthtestenable = VK_TRUE;
		pipelinedata2.DepthStencilstate.depthcompareop = VK_COMPARE_OP_LESS;
		pipelinedata2.DepthStencilstate.depthwriteenable = VK_TRUE;
		pipelinedata2.DepthStencilstate.stenciltestenable = VK_FALSE;
		pipelinedata2.DepthStencilstate.boundsenable = VK_FALSE;
		pipelinedata2.DepthStencilstate.min_bounds = 0.0f;
		pipelinedata2.DepthStencilstate.max_bounds = 1.0f;
		//add more options

		pipelinedata2.Multisamplingstate.samplecount = VK_SAMPLE_COUNT_1_BIT;
		pipelinedata2.Multisamplingstate.sampleshadingenable = VK_FALSE;

		pipelinedata2.ViewPortstate.dynamicviewport = true;

		pipelinedata2.Inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		std::vector<VkDescriptorSetLayout> layoutsz2 = { program_shadowpoint.GetGlobalLayout(), program_shadowpoint.GetLocalLayout() };

		pipeline_shadowpoint = pipecache.GetGraphicsPipeline(pipelinedata2, Device.GetPhysicalDevice(), renderpass_shadowpoint, program_shadowpoint.GetShaderStageInfo(), program_shadowpoint.GetPushRanges(), layoutsz2.data(), layoutsz2.size());
	}

	void RenderManager::Shadowdeleteimagedata()
	{
		vkDestroyRenderPass(Device.GetDevice(), renderpass_shadow, nullptr);
		vkDestroyRenderPass(Device.GetDevice(), renderpass_shadowpoint, nullptr);

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.DestroyImage(shadowcascade_atlasimage[i]);
			vkDestroyImageView(Device.GetDevice(), shadowcascade_atlasview[i], nullptr);

			Device.DestroyImage(shadowpoint_atlasimage[i]);
			vkDestroyImageView(Device.GetDevice(), shadowpoint_atlasview[i], nullptr);
		}

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			vkDestroyFramebuffer(Device.GetDevice(), framebuffer_shadowcascadesatlas[i], nullptr);
			vkDestroyFramebuffer(Device.GetDevice(), framebuffer_shadowpoints[i], nullptr);
		}

		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_shadow.layout, nullptr);
		vkDestroyPipeline(Device.GetDevice(), pipeline_shadow.pipeline, nullptr);

		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_shadowpoint.layout, nullptr);
		vkDestroyPipeline(Device.GetDevice(), pipeline_shadowpoint.pipeline, nullptr);
	}

	void RenderManager::CleanUpShadow()
	{
		Shadowdeleteimagedata();

		for (int c = 0; c < CASCADE_COUNT; c++)
		{
			program_shadow.RemoveLocalDescriptor(c);
		}

		program_shadow.CleanUp();
		program_shadowpoint.CleanUp();

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.DestroyBuffer(cascade_depthbuffers[i]);

			for (int c = 0; c < CASCADE_COUNT; c++)
			{
				Device.DestroyBuffer(shadowcascade_pv_buffers[i][c]);

				Device.DestroyBuffer(shadowcascade_nearplane_buffers[i][c]);
			}
			for (int j = 0; j < MAX_POINT_IMAGES; j++)
			{
				Device.DestroyBuffer(shadowpoint_pv_buffers[i][j]);
			}
		}


		for (int i = 0; i < cmdbuffer_shadow.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS), 1, &cmdbuffer_shadow[i]);
		}
	}

	void RenderManager::CreateDepth()
	{
		//pv buffer
		SetProjectionMatrix();

		pv_uniform.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateBuffer(sizeof(glm::mat4) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, pv_uniform[i]);
		}

		//program
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/depthvert.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/depth_presspass.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
			{"ProjVertexBuffer", 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo1 = {
				//alpha
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants1 = {
			{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)}
		};

		if (!program_depth.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(),
			pushconstants1.data(), pushconstants1.size(), 500))
		{
			Logger::LogError("failed to create depth shaderprogram\n");
		}

		Depthcreateimagedata();

		//cmdbuffer
		cmdbuffer_depth.resize(FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo cmd_info = {};
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS);
		cmd_info.commandBufferCount = cmdbuffer_depth.size();
		vkAllocateCommandBuffers(Device.GetDevice(), &cmd_info, cmdbuffer_depth.data());
	}

	void RenderManager::CleanUpDepth()
	{
		Depthdeleteimagedata();
		
		program_depth.CleanUp();

		for (int i = 0; i < cmdbuffer_depth.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS), 1, &cmdbuffer_depth[i]);
		}
	}

	void RenderManager::Depthdeleteimagedata()
	{
		vkDestroyRenderPass(Device.GetDevice(), renderpass_depth, nullptr);

		for (int i = 0; i < depthprepass_attachment.size(); i++)
		{
			Device.DestroyImage(depthprepass_attachment[i]);
			vkDestroyImageView(Device.GetDevice(), depthprepass_view[i], nullptr);
		}

		Device.DestroyImage(dummyimageMS);
		vkDestroyImageView(Device.GetDevice(), dummyviewMS, nullptr);

		for (int i = 0; i < framebuffers_depth.size(); i++)
		{
			vkDestroyFramebuffer(Device.GetDevice(), framebuffers_depth[i], nullptr);
		}

		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_depth.layout, nullptr);
		vkDestroyPipeline(Device.GetDevice(), pipeline_depth.pipeline, nullptr);
	}

	void RenderManager::Depthcreateimagedata()
	{
		//renderpass
		RenderPassCache& rpcache = Device.GetRenderPassCache();
		if (multisampling_count == VK_SAMPLE_COUNT_1_BIT) //*VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		{
			RenderPassAttachment depthattachment(0, findDepthFormat(Device.GetPhysicalDevice()), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				RENDERPASSTYPE::DEPTH);
			std::vector<RenderPassAttachment> attachments = { depthattachment };
			renderpass_depth = rpcache.GetRenderPass(attachments.data(), attachments.size(), VK_PIPELINE_BIND_POINT_GRAPHICS);
		}
		else
		{
			RenderPassAttachment depthattachment(0, findDepthFormat(Device.GetPhysicalDevice()), multisampling_count, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				RENDERPASSTYPE::DEPTH);
			//RenderPassAttachment resolveattachment(1, main_color_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
				//VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::RESOLVE);
			std::vector<RenderPassAttachment> attachments = { depthattachment };
			renderpass_depth = rpcache.GetRenderPass(attachments.data(), attachments.size(), VK_PIPELINE_BIND_POINT_GRAPHICS);
		}

		//depth images/views
		depthprepass_attachment.resize(FRAMES_IN_FLIGHT);
		depthprepass_view.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateImage(VK_IMAGE_TYPE_2D, findDepthFormat(Device.GetPhysicalDevice()), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, multisampling_count,
				Resolution.width, Resolution.height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, depthprepass_attachment[i]);

			depthprepass_view[i] = CreateImageView(Device.GetDevice(), depthprepass_attachment[i].image, findDepthFormat(Device.GetPhysicalDevice()), VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1,
				                                   VK_IMAGE_VIEW_TYPE_2D);
		}

		//the reduce shader needs to read in the depth but of course it could be multisampled or not so just create a dummy image making it multisampled it its not multisampled and
		//vice versa, this should do for now
		Device.CreateImage(VK_IMAGE_TYPE_2D, findDepthFormat(Device.GetPhysicalDevice()), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			(multisampling_count == VK_SAMPLE_COUNT_1_BIT) ? VK_SAMPLE_COUNT_2_BIT : VK_SAMPLE_COUNT_1_BIT,
			Resolution.width, Resolution.height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, dummyimageMS);
		dummyviewMS = CreateImageView(Device.GetDevice(), dummyimageMS.image, findDepthFormat(Device.GetPhysicalDevice()), VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1,
			VK_IMAGE_VIEW_TYPE_2D);

		TransitionImageLayout(Device, dummyimageMS.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT);

		//framebuffers
		framebuffers_depth.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			framebuffers_depth[i] = CreateFrameBuffer(Device.GetDevice(), Resolution.width, Resolution.height, renderpass_depth, &depthprepass_view[i], 1);
		}

		//pipeline
		PipelineCache& pipecache = Device.GetPipelineCache();
		PipelineData pipelinedata(Resolution.width, Resolution.height);

		pipelinedata.ColorBlendstate.blendenable = VK_FALSE;

		pipelinedata.Rasterizationstate.cullmode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_BACK_BIT
		pipelinedata.Rasterizationstate.frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelinedata.Rasterizationstate.polygonmode = VK_POLYGON_MODE_FILL;

		pipelinedata.DepthStencilstate.depthtestenable = VK_TRUE;
		pipelinedata.DepthStencilstate.depthcompareop = VK_COMPARE_OP_LESS;
		pipelinedata.DepthStencilstate.depthwriteenable = VK_TRUE;

		pipelinedata.Multisamplingstate.samplecount = multisampling_count;
		pipelinedata.Multisamplingstate.sampleshadingenable = VK_FALSE;
		pipelinedata.Multisamplingstate.minsampleshading = .5;

		pipelinedata.Inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		std::vector<VkDescriptorSetLayout> layoutsz = { program_depth.GetGlobalLayout(), program_depth.GetLocalLayout() };

		pipeline_depth = pipecache.GetGraphicsPipeline(pipelinedata, Device.GetPhysicalDevice(), renderpass_depth, program_depth.GetShaderStageInfo(), program_depth.GetPushRanges(), layoutsz.data(), layoutsz.size());
	}

	void RenderManager::createDepthfinal()
	{
		DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			global_descriptors.buffersizes[i].push_back(sizeof(glm::mat4) * 2);
			global_descriptors.uniformbuffers[i].push_back(pv_uniform[i]);
		}
		program_depth.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);
	}

	void RenderManager::RecordDepthCmd(int current_frame)
	{
		vkResetCommandBuffer(cmdbuffer_depth[current_frame], 0);

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdbuffer_depth[current_frame], &begin_info);

		//reset both ones
		Device.GetQueryManager().ResetQueries(cmdbuffer_depth[current_frame], current_frame, QueryManager::QUERY_NAME::DEPTH);

		//set timer here at top of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_depth[current_frame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::DEPTH, true);

		VkRenderPassBeginInfo begin_rp = {};
		begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.renderPass = renderpass_depth;
		begin_rp.framebuffer = framebuffers_depth[current_frame];
		begin_rp.renderArea.offset = { 0,0 };
		begin_rp.renderArea.extent = { Resolution.width, Resolution.height };
		std::array<VkClearValue, 1> clearValues = {};
		clearValues[0].depthStencil = { 1.0f, 0 };
		begin_rp.pClearValues = clearValues.data();
		begin_rp.clearValueCount = clearValues.size();

		vkCmdBeginRenderPass(cmdbuffer_depth[current_frame], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmdbuffer_depth[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_depth.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_depth[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_depth.layout, 0, 1, &program_depth.GetGlobalDescriptor(current_frame), 0, nullptr);

		//render all opaque objects for early z test, transparent objects need overdraw. This doesn't have min/max values for transparent objects
		//transparent objects don't actually occlude things so its okay to not have them here for query calculations/etc
		auto bin = objectmanager->GetBin()[RenderObjectManager::BIN_TYPE::REGULAR];
		for (int j = 0; j < bin.size(); j++)
		{
			//vkCmdBindDescriptorSets(cmdbuffer_depth[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 1, 1, &program_pbr.GetLocalDescriptor(bin[j]->GetId(), current_frame), 0, nullptr);
			vkCmdPushConstants(cmdbuffer_depth[current_frame], pipeline_depth.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &bin[j]->GetMatrix(current_frame));

			VkDeviceSize sizes[] = { 0 };
			vkCmdBindVertexBuffers(cmdbuffer_depth[current_frame], 0, 1, &bin[j]->GetMesh().vbo, sizes);
			vkCmdBindIndexBuffer(cmdbuffer_depth[current_frame], bin[j]->GetMesh().ibo, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdbuffer_depth[current_frame], bin[j]->GetMesh().index_size, 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(cmdbuffer_depth[current_frame]);
		
		//set timer here at bottom of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_depth[current_frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::DEPTH, false);

		vkEndCommandBuffer(cmdbuffer_depth[current_frame]);
	}

	void RenderManager::CreateReduce()
	{
		//buffers
		reduce_data.proj = proj_matrix;
		reduce_data.n = near_plane;
		reduce_data.f = far_plane;
		reduce_data.a = 0.0;
		reduce_data.b = 0;

		Device.CreateBufferStaged(sizeof(reduce_struct), &reduce_data, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, reduce_buffer,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		Device.CreateBuffer(sizeof(reduce_struct), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, reduce_stagingbuffer);

		Device.CreateBuffer(sizeof(float) * 2, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, minmax_buffer);

		//program
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/depthreduction.spv", VK_SHADER_STAGE_COMPUTE_BIT},
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
			{"depth_prepass", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"output_tex", 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"ReduceBuffer", 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{"depth_prepassMS", 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo1 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants1 = {
		};
		if (!program_reduce.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(), pushconstants1.data(), pushconstants1.size(), 1))
		{
			Logger::LogError("failed to create reduce shaderprogram\n");
		}

		std::vector<ShaderProgram::shadersinfo> info2 = {
			{"Shaders/spv/depthreductionmulti.spv", VK_SHADER_STAGE_COMPUTE_BIT},
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo2 = {
			{"input_tex", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"output_tex", 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo2 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants2 = {
		};
		if (!program_reduce2.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info2.data(), info2.size(), globalinfo2.data(), globalinfo2.size(), localinfo2.data(), localinfo2.size(), 
			                         pushconstants2.data(), pushconstants2.size(), 1, 50))
		{
			Logger::LogError("failed to create reduce2 shaderprogram\n");
		}

		Reducecreateimagedata();

		//cmd buffers
		cmdbuffer_reduce.resize(FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE);
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = cmdbuffer_reduce.size();
		VULKAN_CHECK(vkAllocateCommandBuffers(Device.GetDevice(), &allocInfo, cmdbuffer_reduce.data()), "allocating reduce cmdbuffers");
	}

	void RenderManager::UpdateReduceProjMatrix()
	{
		reduce_data.proj = proj_matrix;
		reduce_data.n = near_plane;
		reduce_data.f = far_plane;
		reduce_data.a = (multisampling_count == VK_SAMPLE_COUNT_1_BIT) ? 0.0 : 1.0;
		reduce_data.b = 0;

		if (reduce_buffer.buffer != VK_NULL_HANDLE)
		{
			Device.BindData(reduce_stagingbuffer.allocation, &reduce_data, sizeof(reduce_struct));
			CopyBufferToBuffer(Device, reduce_stagingbuffer.buffer, reduce_buffer.buffer, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, sizeof(reduce_struct));
		}
	}

	void RenderManager::createReducefinal()
	{
		//1
		//depth prepass image, first reduce image, buffer with nearplane, farplane, projection matrix
		DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			global_descriptors.buffersizes[i].push_back(sizeof(reduce_struct));
			global_descriptors.uniformbuffers[i].push_back(reduce_buffer);

			global_descriptors.imageviews[i].push_back((multisampling_count == VK_SAMPLE_COUNT_1_BIT) ? depthprepass_view[i] : dummyviewMS);
			global_descriptors.samplers[i].push_back(shadowmapsampler);

			global_descriptors.imageviews[i].push_back(reduce_views[i][0]);
			global_descriptors.samplers[i].push_back(shadowmapsampler);

			global_descriptors.imageviews[i].push_back((multisampling_count != VK_SAMPLE_COUNT_1_BIT) ? depthprepass_view[i] : dummyviewMS);
			global_descriptors.samplers[i].push_back(shadowmapsampler);
		}
		program_reduce.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);
		//2
		//input reduce image, output reduce image
		DescriptorHelper global_descriptors2(FRAMES_IN_FLIGHT);
		reduce_descriptors.clear();
		reduce_descriptors.resize(FRAMES_IN_FLIGHT);

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			reduce_descriptors[i].resize(reduce_views[i].size() - 1);
			program_reduce2.AllocateSets(reduce_descriptors[i].data(), reduce_descriptors[i].size(), program_reduce2.GetGlobalPool(), program_reduce2.GetGlobalLayout());

			for (int j = 0; j < reduce_views[i].size() - 1; j++)
			{
				global_descriptors2.imageviews[i].clear();
				global_descriptors2.samplers[i].clear();

				global_descriptors2.imageviews[i].push_back(reduce_views[i][j]);
				global_descriptors2.samplers[i].push_back(shadowmapsampler);

				global_descriptors2.imageviews[i].push_back(reduce_views[i][j + 1]);
				global_descriptors2.samplers[i].push_back(shadowmapsampler);

				program_reduce2.UpdateDescriptorSet(reduce_descriptors[i][j], program_reduce2.GetGlobalDescriptorInfo(), global_descriptors2.uniformbuffers[i].data(),
					global_descriptors2.buffersizes[i].data(), global_descriptors2.imageviews[i].data(), global_descriptors2.samplers[i].data(), global_descriptors2.bufferviews[i].data());
			}
		}
	}

	void RenderManager::CleanUpReduce()
	{
		//buffers
		Device.DestroyBuffer(reduce_stagingbuffer);
		Device.DestroyBuffer(reduce_buffer);
		Device.DestroyBuffer(minmax_buffer);

		//programs
		program_reduce.CleanUp();
		program_reduce2.CleanUp();

		//cmdbuffers
		for (int i = 0; i < cmdbuffer_reduce.size(); i++)
		{
			vkFreeCommandBuffers(Device.GetDevice(), Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE), 1, &cmdbuffer_reduce[i]);
		}

		Reducedeleteimagedata();
	}

	void RenderManager::Reducecreateimagedata()
	{
		UpdateReduceProjMatrix();
		//reduce images
		reduce_images.clear();
		reduce_views.clear();
		reduce_images.resize(FRAMES_IN_FLIGHT);
		reduce_views.resize(FRAMES_IN_FLIGHT);
		
		const uint32_t local_group_size = 32;
		VkFormat format = VK_FORMAT_R32G32_SFLOAT;

		//check to see if it can be used as storage
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		{
			Logger::LogWarning("reduce format does not support storage_image_bit\n");
		}
		if (!PhysicalDeviceQuery::CheckImageOptimalFormat(Device.GetPhysicalDevice(), format, VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
		{
			Logger::LogWarning("reduce format does not support transfer_src_bit\n");
		}

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			uint32_t width = Resolution.width;
			uint32_t height = Resolution.height;

			reduce_images[i].clear();
			reduce_views[i].clear();

			reduce_extents.clear();
			while (width > 1 || height > 1)
			{
				uint32_t tmpwidth = width;
				tmpwidth = width / local_group_size;
				tmpwidth += width % local_group_size > 0 ? 1 : 0;
				width = tmpwidth;

				uint32_t tmpheight = height;
				tmpheight = height / local_group_size;
				tmpheight += height % local_group_size > 0 ? 1 : 0;
				height = tmpheight;

				VkExtent2D extent = { width, height };
				reduce_extents.push_back(extent);
			}

			reduce_images[i].resize(reduce_extents.size());
			reduce_views[i].resize(reduce_extents.size());
			for (int j = 0; j < reduce_extents.size(); j++)
			{
				//reduce_images[i].resize(reduce_images[i].size() + 1);
				//reduce_views[i].resize(reduce_views[i].size() + 1);
				//vkcoreImage image_tmp;
				//reduce_images[i].push_back(vkcoreImage());
				//reduce_views[i].push_back(VK_NULL_HANDLE);

				Device.CreateImage(VK_IMAGE_TYPE_2D, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SAMPLE_COUNT_1_BIT, reduce_extents[j].width, reduce_extents[j].height, 1, 1, 1,
					VMA_MEMORY_USAGE_GPU_TO_CPU, 0, reduce_images[i][j]);

				reduce_views[i][j] = CreateImageView(Device.GetDevice(), reduce_images[i][j].image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);

				TransitionImageLayout(Device, reduce_images[i][j].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}

		//pipeline
		pipeline_reduce = Device.GetPipelineCache().GetComputePipeline(program_reduce.GetShaderStageInfo()[0], &program_reduce.GetGlobalLayout(), 1);
		pipeline_reduce2 = Device.GetPipelineCache().GetComputePipeline(program_reduce2.GetShaderStageInfo()[0], &program_reduce2.GetGlobalLayout(), 1);
	}

	void RenderManager::Reducedeleteimagedata()
	{
		//images and views
		for (int i = 0; i < reduce_images.size(); i++)
		{
			for (int j = 0; j < reduce_images[i].size(); j++)
			{
				Device.DestroyImage(reduce_images[i][j]);
				vkDestroyImageView(Device.GetDevice(), reduce_views[i][j], nullptr);
			}
		}

		//pipelines
		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_reduce.layout, nullptr);
		vkDestroyPipeline(Device.GetDevice(), pipeline_reduce.pipeline, nullptr);

		vkDestroyPipelineLayout(Device.GetDevice(), pipeline_reduce2.layout, nullptr);
		vkDestroyPipeline(Device.GetDevice(), pipeline_reduce2.pipeline, nullptr);

		//descriptorsets
		for (int i = 0; i < reduce_descriptors.size(); i++)
		{
			vkFreeDescriptorSets(Device.GetDevice(), program_reduce2.GetGlobalPool(), reduce_descriptors[i].size(), reduce_descriptors[i].data());
		}
	}

	void RenderManager::RecordReduceCmd(int current_frame)
	{
		//now just reset command buffer and record using the new swapchainimage and current_frame.
		vkResetCommandBuffer(cmdbuffer_reduce[current_frame], 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_reduce[current_frame], &beginInfo), "begin reduce cmdbuffer");

		//reset both ones
		Device.GetQueryManager().ResetQueries(cmdbuffer_reduce[current_frame], current_frame, QueryManager::QUERY_NAME::REDUCE);

		//set timer here at top of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_reduce[current_frame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::REDUCE, true);

		TransitionImageLayout(Device, depthprepass_attachment[current_frame].image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT, cmdbuffer_reduce[current_frame]);

		vkCmdBindPipeline(cmdbuffer_reduce[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_reduce.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_reduce[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_reduce.layout, 0, 1, &program_reduce.GetGlobalDescriptor(current_frame), 0, nullptr);
		vkCmdDispatch(cmdbuffer_reduce[current_frame], reduce_extents[0].width, reduce_extents[0].height, 1);

		TransitionImageLayout(Device, depthprepass_attachment[current_frame].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT, cmdbuffer_reduce[current_frame]);

		vkCmdBindPipeline(cmdbuffer_reduce[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_reduce2.pipeline);
		for (int i = 0; i < reduce_images[current_frame].size() - 1; i++)
		{
			vkCmdBindDescriptorSets(cmdbuffer_reduce[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_reduce2.layout, 0, 1, &reduce_descriptors[current_frame][i], 0, nullptr);
			vkCmdDispatch(cmdbuffer_reduce[current_frame], reduce_extents[i+1].width, reduce_extents[i+1].height, 1);
		}

		//set timer here at bottom of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_reduce[current_frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::REDUCE, false);

		VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_reduce[current_frame]), "end compute cmdbuffer");
	}

	void RenderManager::CreatePBR()
	{
		cascade_buffer.resize(FRAMES_IN_FLIGHT);
		point_pbrbuffer.resize(FRAMES_IN_FLIGHT);
		point_infobuffer.resize(FRAMES_IN_FLIGHT);
		std::cout << "MAX POINT IMAGE SIZE: " << MAX_POINT_IMAGES << std::endl;
		//max cascade count in shader is 6
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateBuffer(sizeof(glm::mat4) * MAX_CASCADES * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, cascade_buffer[i]);
			Device.CreateBuffer(sizeof(glm::mat4) * MAX_POINT_IMAGES * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, point_pbrbuffer[i]);
			Device.CreateBuffer(sizeof(glm::vec4) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, point_infobuffer[i]);
		}
		std::cout << shadowpoint_width << std::endl;
		//shader program/ uniform buffer
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/pbrvert.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/pbrfrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
			{"cascade_splits", 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
			{"ViewProjBuffer", 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
			{"SunMatrix", 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
			{"sunShadowAtlas", 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"ShadowAtlas", 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"PointMatrix", 5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
			{"AmbientLut", 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"AtmosphereBuffer", 7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
			{"light_struct", 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
			{"lightcount_struct", 9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
			{"TransmittanceLUT", 10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"point_Buffer", 11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
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

		//commandbuffer
		cmdbuffer_pbr.resize(FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo cmd_info = {};
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::GRAPHICS);
		cmd_info.commandBufferCount = cmdbuffer_pbr.size();
		vkAllocateCommandBuffers(Device.GetDevice(), &cmd_info, cmdbuffer_pbr.data());
	}

	//projection matrix will have same aspect ratio as window. Could also set it to a default value if you want to see the same thing no matter what window size is.
	void RenderManager::SetProjectionMatrix()
	{
		proj_matrix = glm::perspective(glm::radians(FOV), ((float)window_extent.width / (float)window_extent.height), near_plane, far_plane);
		UpdateReduceProjMatrix();
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
			global_descriptors.buffersizes[i].push_back(sizeof(glm::vec4) * (MAX_CASCADES - 1));
			global_descriptors.uniformbuffers[i].push_back(cascade_depthbuffers[i]);

			global_descriptors.buffersizes[i].push_back(sizeof(glm::mat4) * 2);
			global_descriptors.uniformbuffers[i].push_back(pv_uniform[i]);

			global_descriptors.buffersizes[i].push_back(sizeof(glm::mat4) * MAX_CASCADES * 2);
			global_descriptors.uniformbuffers[i].push_back(cascade_buffer[i]);

			global_descriptors.buffersizes[i].push_back(sizeof(glm::mat4) * MAX_POINT_IMAGES * 2);
			global_descriptors.uniformbuffers[i].push_back(point_pbrbuffer[i]);

			global_descriptors.buffersizes[i].push_back(sizeof(Atmosphere::atmosphere_shader));
			global_descriptors.uniformbuffers[i].push_back(atmosphere->Getshaderinfobuffer(i));

			global_descriptors.buffersizes[i].push_back(sizeof(Light::lightparams) * LightManager::MAX_LIGHTS);
			global_descriptors.uniformbuffers[i].push_back(lightmanager->GetLightBuffer(i));

			global_descriptors.buffersizes[i].push_back(sizeof(int));
			global_descriptors.uniformbuffers[i].push_back(lightmanager->GetLightCountBuffer(i));

			global_descriptors.buffersizes[i].push_back(sizeof(glm::vec4) * 2);
			global_descriptors.uniformbuffers[i].push_back(point_infobuffer[i]);

			global_descriptors.imageviews[i].push_back(shadowcascade_atlasview[i]);
			global_descriptors.samplers[i].push_back(shadowmapsampler);

			global_descriptors.imageviews[i].push_back(shadowpoint_atlasview[i]);
			global_descriptors.samplers[i].push_back(shadowcubemapsampler);

			//global_descriptors.imageviews[i].push_back(shadowcascade_depthview[i][0]);
			//global_descriptors.samplers[i].push_back(shadowmapsampler);

			//global_descriptors.imageviews[i].push_back(shadowcascade_depthview[i][1]);
			//global_descriptors.samplers[i].push_back(shadowmapsampler);

			global_descriptors.imageviews[i].push_back(atmosphere->GetAmbientView());
			global_descriptors.samplers[i].push_back(atmosphere->GetSampler());

			global_descriptors.imageviews[i].push_back(atmosphere->GetTransmittanceView());
			global_descriptors.samplers[i].push_back(atmosphere->GetSampler());
		}
		program_pbr.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);
	}

	void RenderManager::RecordShadowCmd(int current_frame)
	{
		vkResetCommandBuffer(cmdbuffer_shadow[current_frame], 0);

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdbuffer_shadow[current_frame], &begin_info);

		//reset both ones
		Device.GetQueryManager().ResetQueries(cmdbuffer_shadow[current_frame], current_frame, QueryManager::QUERY_NAME::SHADOW);

		//set timer here at top of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_shadow[current_frame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::SHADOW, true);

		for (int c = 0; c < CASCADE_COUNT; c++)
		{
			int32_t offsetx = shadow_width * (c % 2);
			int32_t offsety = shadow_height * (std::floor(c / 2.0f));

			VkRenderPassBeginInfo begin_rp = {};
			begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin_rp.renderPass = renderpass_shadow;
			//begin_rp.framebuffer = framebuffer_shadowcascades[current_frame][c];
			begin_rp.framebuffer = framebuffer_shadowcascadesatlas[current_frame];
			//begin_rp.renderArea.offset = { 0,0 };
			//begin_rp.renderArea.extent = { shadow_width, shadow_height };
			begin_rp.renderArea.offset = { offsetx, offsety };
			begin_rp.renderArea.extent = { shadow_width, shadow_height };
			std::array<VkClearValue, 1> clearValues = {};
			clearValues[0].depthStencil = { 1.0f, 0 };
			begin_rp.pClearValues = clearValues.data();
			begin_rp.clearValueCount = clearValues.size();

			vkCmdBeginRenderPass(cmdbuffer_shadow[current_frame], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmdbuffer_shadow[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_shadow.pipeline);
			vkCmdBindDescriptorSets(cmdbuffer_shadow[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_shadow.layout, 1, 1, 
				                    &program_shadow.GetLocalDescriptor(c, current_frame_in_flight), 0, nullptr);
			VkViewport viewport;
			//upperleft corner
			viewport.x = offsetx;
			viewport.y = offsety;
			viewport.width = shadow_width;
			viewport.height = shadow_height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			//std::cout <<"offsetx: "<< offsetx << " offsety: " << offsety << std::endl;
			vkCmdSetViewport(cmdbuffer_shadow[current_frame], 0, 1, &viewport);
			VkRect2D scissor{};
			scissor.offset = { offsetx, offsety };
			scissor.extent = { shadow_width, shadow_height };
			vkCmdSetScissor(cmdbuffer_shadow[current_frame], 0, 1, &scissor);

			//render all shadow casting objects
			//transparent objects don't cast shadows
			auto bin = objectmanager->GetBin()[RenderObjectManager::BIN_TYPE::REGULAR];
			for (int j = 0; j < bin.size(); j++)
			{
				//vkCmdBindDescriptorSets(cmdbuffer_shadow[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 1, 1, &program_pbr.GetLocalDescriptor(bin[j]->GetId(), current_frame), 0, nullptr);
				vkCmdPushConstants(cmdbuffer_shadow[current_frame], pipeline_shadow.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &bin[j]->GetMatrix(current_frame));

				VkDeviceSize sizes[] = { 0 };
				vkCmdBindVertexBuffers(cmdbuffer_shadow[current_frame], 0, 1, &bin[j]->GetMesh().vbo, sizes);
				vkCmdBindIndexBuffer(cmdbuffer_shadow[current_frame], bin[j]->GetMesh().ibo, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmdbuffer_shadow[current_frame], bin[j]->GetMesh().index_size, 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(cmdbuffer_shadow[current_frame]);

			//copying too expensive like 8 ms
			//cascade image is in optimal_transfer_src layout
			//this copy converts atlas to the transfer_dst_optimal layout
			//copy cascade shadow map into atlas
			//CopyImageToImage(Device, shadowcascade_depthimage[current_frame][c].image, shadowcascade_atlasimage[current_frame].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			//VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			//shadow_width, shadow_height, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, VkOffset3D{ offsetx, offsety,0 }, VkOffset3D{ 0, 0, 0 }, cmdbuffer_shadow[current_frame]);
		}

		//clear atlas to black
		VkRenderPassBeginInfo begin_rp = {};
		begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.renderPass = renderpass_shadowpoint;
		begin_rp.framebuffer = framebuffer_shadowpoints[current_frame];
		begin_rp.renderArea.offset = { 0, 0 };
		begin_rp.renderArea.extent = { atlaspoint_width, atlaspoint_height };
		std::array<VkClearValue, 1> clearValues = {};
		clearValues[0].depthStencil = { 1.0f, 0 };
		begin_rp.pClearValues = clearValues.data();
		begin_rp.clearValueCount = clearValues.size();

		vkCmdBeginRenderPass(cmdbuffer_shadow[current_frame], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdEndRenderPass(cmdbuffer_shadow[current_frame]);

		//render pointlights first
		for (int i = 0; i < point_current; i++)
		{
			for (int j = 0; j < 6; j++)
			{
				//calculate offset
				int index = i * 6 + j;
				glm::ivec2 slots(atlaspoint_width / shadowpoint_width, atlaspoint_height / shadowpoint_height);
				int32_t offsetx = shadowpoint_width * (index % slots.x);
				int32_t offsety = shadowpoint_height * (std::floor(index / slots.y));

				RecordShadowHelper(offsetx, offsety, index, current_frame);
			}
		}

		for (int s = 0; s < spot_current; s++)
		{
			//calculate offset
			int index = point_current * 6 + s;
			glm::ivec2 slots(atlaspoint_width / shadowpoint_width, atlaspoint_height / shadowpoint_height);
			int32_t offsetx = shadowpoint_width * (index % slots.x);
			int32_t offsety = shadowpoint_height * (std::floor(index / slots.y));

			RecordShadowHelper(offsetx, offsety, index, current_frame);
		}

		//set timer here at bottom of pipeline
		Device.GetQueryManager().WriteTimeStamp(cmdbuffer_shadow[current_frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, current_frame, QueryManager::QUERY_NAME::SHADOW, false);

		vkEndCommandBuffer(cmdbuffer_shadow[current_frame]);
	}

	void RenderManager::RecordShadowHelper(int32_t offsetx, int32_t offsety, int index, int current_frame)
	{
		VkRenderPassBeginInfo begin_rp = {};
		begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.renderPass = renderpass_shadowpoint;
		begin_rp.framebuffer = framebuffer_shadowpoints[current_frame];
		begin_rp.renderArea.offset = { offsetx, offsety };
		begin_rp.renderArea.extent = { shadowpoint_width, shadowpoint_height };
		std::array<VkClearValue, 1> clearValues = {};
		clearValues[0].depthStencil = { 1.0f, 0 };
		begin_rp.pClearValues = clearValues.data();
		begin_rp.clearValueCount = clearValues.size();

		vkCmdBeginRenderPass(cmdbuffer_shadow[current_frame], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmdbuffer_shadow[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_shadowpoint.pipeline);

		vkCmdBindDescriptorSets(cmdbuffer_shadow[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_shadowpoint.layout, 1, 1,
			&program_shadowpoint.GetLocalDescriptor(index, current_frame_in_flight), 0, nullptr);

		VkViewport viewport;
		//upperleft corner
		viewport.x = offsetx;
		viewport.y = offsety;
		viewport.width = shadowpoint_width;
		viewport.height = shadowpoint_height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdbuffer_shadow[current_frame], 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset = { offsetx, offsety };
		scissor.extent = { shadowpoint_width, shadowpoint_height };
		vkCmdSetScissor(cmdbuffer_shadow[current_frame], 0, 1, &scissor);

		//render all shadow casting objects
		//transparent objects don't cast shadows
		auto bin = objectmanager->GetBin()[RenderObjectManager::BIN_TYPE::REGULAR];
		for (int j = 0; j < bin.size(); j++)
		{
			//vkCmdBindDescriptorSets(cmdbuffer_shadow[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 1, 1, &program_pbr.GetLocalDescriptor(bin[j]->GetId(), current_frame), 0, nullptr);
			vkCmdPushConstants(cmdbuffer_shadow[current_frame], pipeline_shadow.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &bin[j]->GetMatrix(current_frame));

			VkDeviceSize sizes[] = { 0 };
			vkCmdBindVertexBuffers(cmdbuffer_shadow[current_frame], 0, 1, &bin[j]->GetMesh().vbo, sizes);
			vkCmdBindIndexBuffer(cmdbuffer_shadow[current_frame], bin[j]->GetMesh().ibo, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdbuffer_shadow[current_frame], bin[j]->GetMesh().index_size, 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(cmdbuffer_shadow[current_frame]);
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
	//TODO - store this somehow and reuse it
	//store something in renderobject?
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
		time_depth[time_counter] = Device.GetQueryManager().GetNanoseconds(current_frame_in_flight, QueryManager::QUERY_NAME::DEPTH) / 1000000.0;
		time_reduce[time_counter] = Device.GetQueryManager().GetNanoseconds(current_frame_in_flight, QueryManager::QUERY_NAME::REDUCE) / 1000000.0;
		time_shadow[time_counter] = Device.GetQueryManager().GetNanoseconds(current_frame_in_flight, QueryManager::QUERY_NAME::SHADOW) / 1000000.0;
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

		UpdateShadowLights(current_frame_in_flight);
		point_info = glm::vec4((float)shadowpoint_width, (float)shadowpoint_height, point_current, -1);
		std::vector<glm::vec4> infos = { point_info, bias_info };
		Device.BindData(point_infobuffer[current_frame_in_flight].allocation, infos.data(), sizeof(glm::vec4) * 2);


		//min depth reduction: gpu is done calculating min/max values. transparent objects don't cast shadows.
		float sdsm_nearplane;
		float sdsm_farplane;
		if (SDSM_ENABLE)
		{
			CopyImageToBuffer(Device, reduce_images[current_frame_in_flight][reduce_images[current_frame_in_flight].size() - 1].image, minmax_buffer.buffer, VK_ACCESS_HOST_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
				VK_IMAGE_LAYOUT_GENERAL, 1, 1, sizeof(float) * 2);
			float* minmax_values = reinterpret_cast<float*>(Device.GetBufferData(minmax_buffer.allocation, sizeof(float) * 2));
			sdsm_nearplane = *minmax_values * (far_plane - near_plane) + near_plane;
			sdsm_farplane = *(minmax_values + 1) * (far_plane - near_plane) + near_plane;
		}
		
		std::vector<glm::mat4> shadow_views;
		std::vector<glm::mat4> shadow_orthos;
		cascade_nears.clear();
		cascade_depths.clear();
		//calculate new view and orthogonal matrixes for all cascaded shadow maps. just cpu only but we need gpu data for min/max z value
		CSM(CASCADE_COUNT, SDSM_ENABLE, STABLE_ENABLE, cascade_sun_distance, FOV, (SDSM_ENABLE) ? sdsm_nearplane : near_plane, (SDSM_ENABLE) ? sdsm_farplane : far_plane, window_extent, glm::inverse(cam_matrix),
			 glm::vec4(atmosphere->GetSunDirection(), 0), shadow_views, shadow_orthos, cascade_depths, cascade_nears, cam_matrix, proj_matrix);

		//cascade near planes gpu dependency
		for (int c = 0; c < CASCADE_COUNT; c++)
		{
			Device.BindData(shadowcascade_nearplane_buffers[current_frame_in_flight][c].allocation, &cascade_nears[c], sizeof(float));
		}

		//shadow pass light direction gpu dependency
		for (int c = 0; c < CASCADE_COUNT; c++)
		{
			std::vector<glm::mat4> shadow_matrix = { shadow_views[c], shadow_orthos[c] };
			Device.BindData(shadowcascade_pv_buffers[current_frame_in_flight][c].allocation, shadow_matrix.data(), sizeof(glm::mat4) * 2);
		}

		//shadow depth values
		Device.BindData(cascade_depthbuffers[current_frame_in_flight].allocation, cascade_depths.data(), sizeof(glm::vec4) * cascade_depths.size());

		//pbr cascade matrixes
		std::vector<glm::mat4> cascade_matrixes;
		for (int c = 0; c < CASCADE_COUNT; c++)
		{
			cascade_matrixes.push_back(shadow_views[c]);
			cascade_matrixes.push_back(shadow_orthos[c]);
		}
		Device.BindData(cascade_buffer[current_frame_in_flight].allocation, cascade_matrixes.data(), sizeof(glm::mat4) * cascade_matrixes.size());

		//proj/view matrix gpu dependency
		std::vector<glm::mat4> pv_matrix = { cam_matrix, proj_matrix };
		Device.BindData(pv_uniform[current_frame_in_flight].allocation, pv_matrix.data(), sizeof(glm::mat4) * 2);

		//we have resource key so here is where we update gpu dependencies for current frame
		lightmanager->Update(current_frame_in_flight); //gpu dependency, its changing current frames light buffer
		atmosphere->Update(current_frame_in_flight); //gpu dependency, its changing current frames shaderinfo buffer
		//lightmanager->SyncGPUBuffer();
		//resubmit commandbuffers that need to be updated every frame
		RecordDepthCmd(current_frame_in_flight);
		if(SDSM_ENABLE)
			RecordReduceCmd(current_frame_in_flight);
		RecordShadowCmd(current_frame_in_flight);
		RecordPBRCmd(current_frame_in_flight);
		RecordComputeCmd(current_frame_in_flight, imageIndex);
		RecordGuiCmd(current_frame_in_flight);
		RecordQuadCmd(current_frame_in_flight, imageIndex);

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

		//submit depth prepass
		VkSubmitInfo submit_info_depth = {};
		submit_info_depth.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info_depth.commandBufferCount = 1;
		submit_info_depth.pCommandBuffers = &cmdbuffer_depth[current_frame_in_flight];

		VkPipelineStageFlags stages_depthpass[] = { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT };
		submit_info_depth.pWaitDstStageMask = stages_depthpass;
		submit_info_depth.waitSemaphoreCount = 1;
		submit_info_depth.pWaitSemaphores = &semaphore_imagefetch[current_frame_in_flight];

		submit_info_depth.signalSemaphoreCount = 1;
		submit_info_depth.pSignalSemaphores = &semaphore_depth[current_frame_in_flight];

		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::GRAPHICS), 1, &submit_info_depth, VK_NULL_HANDLE);

		if (SDSM_ENABLE)
		{
			//submit depth reduce pass
			VkSubmitInfo submit_info_reduce = {};
			submit_info_reduce.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit_info_reduce.commandBufferCount = 1;
			submit_info_reduce.pCommandBuffers = &cmdbuffer_reduce[current_frame_in_flight];

			VkPipelineStageFlags stages_reduce[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
			submit_info_reduce.pWaitDstStageMask = stages_reduce;
			submit_info_reduce.waitSemaphoreCount = 1;
			submit_info_reduce.pWaitSemaphores = &semaphore_depth[current_frame_in_flight];

			submit_info_reduce.signalSemaphoreCount = 1;
			submit_info_reduce.pSignalSemaphores = &semaphore_reduce[current_frame_in_flight];

			vkQueueSubmit(Device.GetQueue(POOL_FAMILY::COMPUTE), 1, &submit_info_reduce, VK_NULL_HANDLE);
		}

		//submit shadow pass
		VkSubmitInfo submit_info_shadow = {};
		submit_info_shadow.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info_shadow.commandBufferCount = 1;
		submit_info_shadow.pCommandBuffers = &cmdbuffer_shadow[current_frame_in_flight];

		VkPipelineStageFlags stages_shadowpass[] = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit_info_shadow.pWaitDstStageMask = stages_shadowpass;
		submit_info_shadow.waitSemaphoreCount = 1;
		submit_info_shadow.pWaitSemaphores = (SDSM_ENABLE) ? &semaphore_reduce[current_frame_in_flight] : &semaphore_depth[current_frame_in_flight];

		submit_info_shadow.signalSemaphoreCount = 1;
		submit_info_shadow.pSignalSemaphores = &semaphore_shadow[current_frame_in_flight];

		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::GRAPHICS), 1, &submit_info_shadow, VK_NULL_HANDLE);

		//submit main color pass
		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmdbuffer_pbr[current_frame_in_flight];

		VkPipelineStageFlags stages_colorpass[] = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit_info.pWaitDstStageMask = stages_colorpass;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &semaphore_shadow[current_frame_in_flight];

		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &semaphore_colorpass[current_frame_in_flight];
		
		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::GRAPHICS), 1, &submit_info, VK_NULL_HANDLE);

		//submit Post-Process pass
		VkSubmitInfo submit_info_compute = {};
		submit_info_compute.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info_compute.commandBufferCount = 1;
		submit_info_compute.pCommandBuffers = &cmdbuffer_compute[current_frame_in_flight];

		VkPipelineStageFlags stages_compute[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
		submit_info_compute.pWaitDstStageMask = stages_colorpass;
		submit_info_compute.waitSemaphoreCount = 1;
		submit_info_compute.pWaitSemaphores = &semaphore_colorpass[current_frame_in_flight];

		submit_info_compute.signalSemaphoreCount = 1;
		submit_info_compute.pSignalSemaphores = &semaphore_computepass[current_frame_in_flight];

		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::COMPUTE), 1, &submit_info_compute, VK_NULL_HANDLE);

		//submit Gui pass
		VkSubmitInfo submit_info_gui = {};
		submit_info_gui.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info_gui.commandBufferCount = 1;
		submit_info_gui.pCommandBuffers = &cmdbuffer_gui[current_frame_in_flight];

		VkPipelineStageFlags stages_gui[] = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit_info_gui.pWaitDstStageMask = stages_gui;
		submit_info_gui.waitSemaphoreCount = 1;
		submit_info_gui.pWaitSemaphores = &semaphore_computepass[current_frame_in_flight];

		submit_info_gui.signalSemaphoreCount = 1;
		submit_info_gui.pSignalSemaphores = &semaphore_gui[current_frame_in_flight];

		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::GRAPHICS), 1, &submit_info_gui, VK_NULL_HANDLE);

		//submit quad pass
		VkSubmitInfo submit_info_quad = {};
		submit_info_quad.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info_quad.commandBufferCount = 1;
		submit_info_quad.pCommandBuffers = &cmdbuffer_quad[current_frame_in_flight];

		VkPipelineStageFlags stages_quad[] = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submit_info_quad.pWaitDstStageMask = stages_quad;
		submit_info_quad.waitSemaphoreCount = 1;
		submit_info_quad.pWaitSemaphores = &semaphore_gui[current_frame_in_flight];

		submit_info_quad.signalSemaphoreCount = 1;
		submit_info_quad.pSignalSemaphores = &semaphore_quad[current_frame_in_flight];

		VkFence submit_fence = inFlightFences[current_frame_in_flight];
		if (enable_imgui)
		{
			submit_fence = VK_NULL_HANDLE;
		}
		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::GRAPHICS), 1, &submit_info_quad, submit_fence);

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
			submit_info_imgui.pWaitSemaphores = &semaphore_quad[current_frame_in_flight];

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
		VkSemaphore sem = semaphore_quad[current_frame_in_flight];
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

	//obviously this is inefficient pretty much it recalcualtes everything for every light, ideally we would want to know waht lights get added/removed and deal with it accordingly
	//however this is just for lights getting added or removed with shadow_enabled so it doesn't really matter, and its kind of annoying to deal with in multipl_frames in flight
	//right now it removes every frame at once which is fine I guess
	void RenderManager::UpdateShadowLights(int current_frame)
	{
		bool update_shadowlights = false;
		if (lightmanager->GetShadowCastChanged())
		{
			update_shadowlights = true;
			//lightshadow_counter = FRAMES_IN_FLIGHT - 1;
		}
		else if (lightshadow_counter > 0)
		{
			//update_shadowlights = true;
			//lightshadow_counter--;
		}

		if (update_shadowlights)
		{
			std::cout << "updating point/spot shadows\n";

			//re-calculate matrixes
			std::vector<glm::mat4> point_cam_matrixes;
			std::vector<glm::mat4> point_proj_matrixes;
			std::vector<glm::mat4> spot_cam_matrixes;
			std::vector<glm::mat4> spot_proj_matrixes;
			PointSpotShadows(lightmanager, point_cam_matrixes, point_proj_matrixes, spot_cam_matrixes, spot_proj_matrixes);
			pointsize_current[current_frame] = point_cam_matrixes.size() / 6;
			spotsize_current[current_frame] = spot_cam_matrixes.size();
			point_current = point_cam_matrixes.size() / 6;
			spot_current = spot_cam_matrixes.size();

			//update gpu buffers
			std::vector<glm::mat4> total_matrixes;
			for (int k = 0; k < FRAMES_IN_FLIGHT; k++)
			{
				for (int i = 0; i < point_cam_matrixes.size(); i++)
				{
					int index = i;
					std::vector<glm::mat4> pv_matrix = { point_cam_matrixes[index], point_proj_matrixes[index] };
					Device.BindData(shadowpoint_pv_buffers[k][index].allocation, pv_matrix.data(), sizeof(glm::mat4) * 2);
					
					if (k == 0)
					{
						total_matrixes.push_back(point_cam_matrixes[index]);
						total_matrixes.push_back(point_proj_matrixes[index]);
					}
				}
			}

			for (int k = 0; k < FRAMES_IN_FLIGHT; k++)
			{
				for (int i = 0; i < spot_cam_matrixes.size(); i++)
				{
					int index = point_cam_matrixes.size() + i;
					std::vector<glm::mat4> pv_matrix = { spot_cam_matrixes[i], spot_proj_matrixes[i] };
					Device.BindData(shadowpoint_pv_buffers[k][index].allocation, pv_matrix.data(), sizeof(glm::mat4) * 2);

					if (k == 0)
					{
						total_matrixes.push_back(spot_cam_matrixes[i]);
						total_matrixes.push_back(spot_proj_matrixes[i]);
					}
				}
			}

			for (int k = 0; k < FRAMES_IN_FLIGHT; k++)
			{
				Device.BindData(point_pbrbuffer[k].allocation, total_matrixes.data(), sizeof(glm::mat4) * total_matrixes.size());
			}

			point_info = glm::vec4((float)shadowpoint_width, (float)shadowpoint_height, point_current, -1);
			std::vector<glm::vec4> infos = { point_info, bias_info };
			for (int k = 0; k < FRAMES_IN_FLIGHT; k++)
			{
				Device.BindData(point_infobuffer[k].allocation, infos.data(), sizeof(glm::vec4) * 2);
			}

			//remove all local descriptors
			int local_count = program_shadowpoint.GetLocalDescriptorSize();
			for (int i = 0; i < local_count; i++)
			{
				program_shadowpoint.RemoveLocalDescriptor(i);
			}

			//add all local descriptors
			for (int i = 0; i < point_cam_matrixes.size(); i++)
			{
				int index = i;
				DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);
				for (int k = 0; k < FRAMES_IN_FLIGHT; k++)
				{
					global_descriptors.buffersizes[k].push_back(sizeof(glm::mat4) * 2);
					global_descriptors.uniformbuffers[k].push_back(shadowpoint_pv_buffers[k][index]);
				}
				program_shadowpoint.AddLocalDescriptor(index, global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers,
					global_descriptors.bufferviews);
			}
			for (int i = 0; i < spot_cam_matrixes.size(); i++)
			{
				int index = point_cam_matrixes.size() + i;
				DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);
				for (int k = 0; k < FRAMES_IN_FLIGHT; k++)
				{
					global_descriptors.buffersizes[k].push_back(sizeof(glm::mat4) * 2);
					global_descriptors.uniformbuffers[k].push_back(shadowpoint_pv_buffers[k][index]);
				}

				program_shadowpoint.AddLocalDescriptor(index, global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers,
					global_descriptors.bufferviews);
			}
		}

	}
}