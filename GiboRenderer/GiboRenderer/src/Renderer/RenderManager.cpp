#include "../pch.h"
#include "RenderManager.h"
#include "vkcore/VulkanHelpers.h"

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
		WindowManager.Terminate();
		
		meshCache->CleanUp();
		delete meshCache;
		
		textureCache->CleanUp();
		delete textureCache;

		lightmanager->CleanUp();
		delete lightmanager;

		objectmanager->CleanUp();
		delete objectmanager;

		Device.DestroyDevice();
	}

	bool RenderManager::InitializeRenderer()
	{
		bool valid = true;
		window_extent.width = 800;
		window_extent.height = 600;

		valid = valid && WindowManager.InitializeVulkan(window_extent.width, window_extent.height, &framebuffer_size_callback, key_callback, cursor_position_callback, mouse_button_callback, scroll_callback);
		//set the callbacks to come back to this instance of rendermanager
		glfwSetWindowUserPointer(WindowManager.Getwindow(), reinterpret_cast<void*>(this));

		valid = valid && Device.CreateDevice("GiboRenderer", WindowManager.Getwindow(), window_extent, FRAMES_IN_FLIGHT);
		meshCache = new MeshCache(Device);
		textureCache = new TextureCache(Device);
		lightmanager = new LightManager(Device, FRAMES_IN_FLIGHT);
		objectmanager = new RenderObjectManager(Device, FRAMES_IN_FLIGHT);

		valid = valid && InitializeVulkan();
		CreatePBR();
		CreateAtmosphere();
		atmosphere->UpdateCamPosition(glm::vec4(0, 0, 0, 1));
		createPBRfinal();
		CreateCompute();

		//fences and semaphores
		semaphore_imagefetch.resize(FRAMES_IN_FLIGHT);
		semaphore_colorpass.resize(FRAMES_IN_FLIGHT);
		semaphore_computepass.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < semaphore_imagefetch.size(); i++)
		{
			VkSemaphoreCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			info.flags = 0;

			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_imagefetch[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_colorpass[i]);
			vkCreateSemaphore(Device.GetDevice(), &info, nullptr, &semaphore_computepass[i]);
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

		/*
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0;

		vkBeginCommandBuffer(main_cmdbuffer, &begin_info);
		VkRenderPassBeginInfo begin_rp = {};
		begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.renderPass = main_renderpass;
		begin_rp.framebuffer = main_framebuffer;
		begin_rp.renderArea.offset = { 0,0 };
		begin_rp.renderArea.extent = { window_extent.width, window_extent.height };
		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		begin_rp.pClearValues = clearValues.data();
		begin_rp.clearValueCount = 2;

		vkCmdBeginRenderPass(main_cmdbuffer, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(main_cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pipeline.pipeline);
		vkCmdBindDescriptorSets(main_cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pipeline.layout, 0, 1, &main_program.GetGlobalDescriptor(0), 0, nullptr);
		vkCmdBindDescriptorSets(main_cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pipeline.layout, 1, 1, &main_program.GetLocalDescriptor(coloruniformid, 0), 0, nullptr);
		vkCmdPushConstants(main_cmdbuffer, main_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &teapotobject->GetMatrix());

		VkDeviceSize sizes[] = { 0 };
		vkCmdBindVertexBuffers(main_cmdbuffer, 0, 1, &teapotobject->GetMesh().vbo, sizes);
		vkCmdBindIndexBuffer(main_cmdbuffer, teapotobject->GetMesh().ibo, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(main_cmdbuffer, teapotobject->GetMesh().index_size, 1, 0, 0, 0);

		atmosphere->Draw(main_cmdbuffer, );

		vkCmdEndRenderPass(main_cmdbuffer);
		vkEndCommandBuffer(main_cmdbuffer);
		*/
		return valid;
	}

	void RenderManager::CreateAtmosphere()
	{
		atmosphere = new Atmosphere(Device, textureCache->Get2DTexture("Images/missingtexture.png", 0));
		atmosphere->Create(window_extent, renderpass_pbr, *meshCache, FRAMES_IN_FLIGHT, pv_uniform);
		atmosphere->FillLUT();
	}

	void RenderManager::CreateCompute()
	{
		program_compute;
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/compgaussblur.spv", VK_SHADER_STAGE_COMPUTE_BIT},
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


		//descriptors
		//*theres no garuntee what swapchain image is getting used so you have to rebind descriptor data and redo command buffer every frame which is quick anyways.
		DescriptorHelper global_descriptors(FRAMES_IN_FLIGHT);

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			//color attachment from our main shader
			global_descriptors.imageviews[i].push_back(color_attachmentview[i]);
			global_descriptors.samplers[i].push_back(atmosphere->GetSampler());

			//swapchain image
			global_descriptors.imageviews[i].push_back(Device.GetswapchainView(i));
			global_descriptors.samplers[i].push_back(atmosphere->GetSampler());
		}
		program_compute.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);
	
		//create cmdbuffers
		cmdbuffer_compute.resize(FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE);
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = cmdbuffer_compute.size();
		VULKAN_CHECK(vkAllocateCommandBuffers(Device.GetDevice(), &allocInfo, cmdbuffer_compute.data()), "allocating compute cmdbuffers");
		
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_compute[i], &beginInfo), "begin post proccess cmdbuffer");

			TransitionImageLayout(Device, Device.GetswapchainImage(current_frame_in_flight), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_HOST_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
				VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, cmdbuffer_compute[i]);

			float WORKGROUP_SIZE = 1.0;
			vkCmdBindPipeline(cmdbuffer_compute[i], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute.pipeline);
			vkCmdBindDescriptorSets(cmdbuffer_compute[i], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute.layout, 0, 1, &program_compute.GetGlobalDescriptor(current_frame_in_flight), 0, nullptr);
			vkCmdDispatch(cmdbuffer_compute[i], window_extent.width / WORKGROUP_SIZE, window_extent.height / WORKGROUP_SIZE, 1);

			TransitionImageLayout(Device, Device.GetswapchainImage(current_frame_in_flight), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, cmdbuffer_compute[i]);
			VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_compute[i]), "end cmdbuffer");
		}
	}

	void RenderManager::ReSubmitComputeCmd(int current_frame, int current_imageindex)
	{
		//we have to set the current_frames global descriptorset to point to the specific swapchain image we have this frame in flight. 
		DescriptorHelper global_descriptors(1);
		global_descriptors.imageviews[0].push_back(color_attachmentview[current_frame]);
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

		TransitionImageLayout(Device, Device.GetswapchainImage(current_imageindex), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_HOST_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, cmdbuffer_compute[current_frame]);

		float WORKGROUP_SIZE = 1.0;
		vkCmdBindPipeline(cmdbuffer_compute[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_compute[current_frame], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute.layout, 0, 1, &program_compute.GetGlobalDescriptor(current_frame), 0, nullptr);
		vkCmdDispatch(cmdbuffer_compute[current_frame], window_extent.width / WORKGROUP_SIZE, window_extent.height / WORKGROUP_SIZE, 1);

		TransitionImageLayout(Device, Device.GetswapchainImage(current_imageindex), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, cmdbuffer_compute[current_frame]);
		VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_compute[current_frame]), "end cmdbuffer");
	}

	void RenderManager::CreatePBR()
	{
		//pv buffer
		float widthf = window_extent.width;
		float heightf = window_extent.height;
		glm::mat4 p_matrix = glm::perspective(glm::radians(45.0f), (widthf / heightf), 0.01f, 1000.0f);
		glm::mat4 v_matrix = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0, 0, 3) + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
		std::vector<glm::mat4> pv_matrix = { v_matrix, p_matrix };

		pv_uniform.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateBuffer(sizeof(glm::mat4) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, pv_uniform[i]);
			Device.BindData(pv_uniform[i].allocation, pv_matrix.data(), sizeof(glm::mat4) * 2);
		}

		//objects
		meshCache->LoadMeshFromFile("Models/uvsphere.obj");
		meshCache->LoadMeshFromFile("Models/teapot.fbx");
		teapotobjectpbr = new RenderObject(&Device, textureCache->Get2DTexture("Images/missingtexture.png", 0));
		teapotobjectpbr->SetMesh(meshCache->GetMesh("Models/uvsphere.obj"));
		teapotobjectpbr->SetTexture(textureCache->Get2DTexture("Images/uvmap2.png", true));
		teapotobjectpbr->GetMaterial().SetRoughness(1.0f);
		teapotobjectpbr->GetMaterial().SetAlbedo(glm::vec4(1.00, 0.71, 0.29, 1));
		teapotobjectpbr->GetMaterial().SetNormalMap(textureCache->Get2DTexture("Images/HexNormal.png", 0));
		teapotobjectpbr->SetTransformation(glm::vec3(2, 0, -3), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		spherepbr = new RenderObject(&Device, textureCache->Get2DTexture("Images/missingtexture.png", 0));
		spherepbr->SetMesh(meshCache->GetMesh("Models/teapot.fbx"));
		spherepbr->SetTexture(textureCache->Get2DTexture("Images/Rocks.png", true));
		spherepbr->GetMaterial().SetNormalMap(textureCache->Get2DTexture("Images/RocksHeight.png", 0));
		spherepbr->SetTransformation(glm::vec3(-2, 0, -3), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		objectmanager->AddRenderObject(teapotobjectpbr, RenderObjectManager::BIN_TYPE::REGULAR);
		objectmanager->AddRenderObject(spherepbr, RenderObjectManager::BIN_TYPE::REGULAR);

		//lights
		light1.setColor(glm::vec4(.4, .4f, 0.7f, 1)).setFallOff(10).setDirection(glm::vec4(0,0,-1,1)).setIntensity(100).setPosition(glm::vec4(0, 5, 0, 1)).setType(Light::light_type::POINT);
		lightmanager->AddLight(light1);
		//lightmanager->Update(0);
		//lightmanager->RemoveLight(light1);

		//renderpasss
		RenderPassCache& rpcache = Device.GetRenderPassCache();
		RenderPassAttachment colorattachment(0, Device.GetswapchainFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, RENDERPASSTYPE::COLOR);
		RenderPassAttachment depthattachment(1, findDepthFormat(Device.GetPhysicalDevice()), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			RENDERPASSTYPE::DEPTH);
		std::vector<RenderPassAttachment> attachments = { colorattachment, depthattachment };
		renderpass_pbr = rpcache.GetRenderPass(attachments.data(), attachments.size(), VK_PIPELINE_BIND_POINT_GRAPHICS);

		//image attachments
		VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		if (!CheckImageOptimalFormat(Device.GetPhysicalDevice(), color_format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
		{
			Logger::LogError("format not supported for main color attachment\n");
		}
		if (!CheckImageOptimalFormat(Device.GetPhysicalDevice(), color_format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		{
			Logger::LogError("storage format not supported for main color attachment\n");
		}
		
		color_attachment.resize(FRAMES_IN_FLIGHT);
		color_attachmentview.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateImage(VK_IMAGE_TYPE_2D, color_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, window_extent.width, window_extent.height, 1, 1, 1,
				VMA_MEMORY_USAGE_GPU_ONLY, 0, color_attachment[i]);

			color_attachmentview[i] = CreateImageView(Device.GetDevice(), color_attachment[i].image, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		}

		depth_attachment.resize(FRAMES_IN_FLIGHT);
		depth_view.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			Device.CreateImage(VK_IMAGE_TYPE_2D, findDepthFormat(Device.GetPhysicalDevice()), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT, window_extent.width,
				window_extent.height, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, depth_attachment[i]);
			depth_view[i] = CreateImageView(Device.GetDevice(), depth_attachment[i].image, findDepthFormat(Device.GetPhysicalDevice()), VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		}
		
		//framebuffer
		framebuffer_pbr.resize(FRAMES_IN_FLIGHT);
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			std::vector<VkImageView> imageview = { color_attachmentview[i], depth_view[i] };
			framebuffer_pbr[i] = CreateFrameBuffer(Device.GetDevice(), window_extent.width, window_extent.height, renderpass_pbr, imageview.data(), imageview.size());
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

		if (!program_pbr.Create(Device.GetDevice(), FRAMES_IN_FLIGHT, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(), pushconstants1.data(), pushconstants1.size(), 50))
		{
			Logger::LogError("failed to create pbr shaderprogram\n");
		}
		
		//pipeline
		PipelineCache& pipecache = Device.GetPipelineCache();
		PipelineData pipelinedata(window_extent.width, window_extent.height);
		
		pipelinedata.ColorBlendstate.blendenable = VK_FALSE;
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

		pipelinedata.Multisamplingstate.samplecount = VK_SAMPLE_COUNT_1_BIT;
		pipelinedata.Inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		std::vector<VkDescriptorSetLayout> layoutsz = { program_pbr.GetGlobalLayout(), program_pbr.GetLocalLayout() };

		pipeline_pbr = pipecache.GetGraphicsPipeline(pipelinedata, Device.GetPhysicalDevice(), renderpass_pbr, program_pbr.GetShaderStageInfo(), program_pbr.GetPushRanges(), layoutsz.data(), layoutsz.size());
	
		//image and views, framebuffer, shader, deallocatecommandbuffer
	}

	void RenderManager::createPBRfinal()
	{
		//descriptors global and local
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


		uint32_t teapotid, sphereid;
		for (int j = 0; j < objectmanager->GetVector().size(); j++)
		{
			DescriptorHelper local_descriptors(FRAMES_IN_FLIGHT);
			for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
			{

				local_descriptors.buffersizes[i].push_back(sizeof(Material::materialinfo));
				local_descriptors.uniformbuffers[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetBuffer());

				local_descriptors.imageviews[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetAlbedoMap().view);
				local_descriptors.samplers[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetAlbedoMap().sampler);

				local_descriptors.imageviews[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetSpecularMap().view);
				local_descriptors.samplers[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetSpecularMap().sampler);

				local_descriptors.imageviews[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetMetalMap().view);
				local_descriptors.samplers[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetMetalMap().sampler);

				local_descriptors.imageviews[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetNormalMap().view);
				local_descriptors.samplers[i].push_back(objectmanager->GetVector()[j]->GetMaterial().GetNormalMap().sampler);

			}

			program_pbr.AddLocalDescriptor((j == 0) ? teapotid : sphereid, local_descriptors.uniformbuffers, local_descriptors.buffersizes, local_descriptors.imageviews, local_descriptors.samplers, local_descriptors.bufferviews);
		}
		
		//commandbuffer
		cmdbuffer_pbr.resize(FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo cmd_info = {};
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandPool = Device.GetCommandPoolCache().GetCommandPool(POOL_TYPE::STATIC, POOL_FAMILY::GRAPHICS);
		cmd_info.commandBufferCount = cmdbuffer_pbr.size();
		vkAllocateCommandBuffers(Device.GetDevice(), &cmd_info, cmdbuffer_pbr.data());

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0;// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			vkBeginCommandBuffer(cmdbuffer_pbr[i], &begin_info);
			VkRenderPassBeginInfo begin_rp = {};
			begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin_rp.renderPass = renderpass_pbr;
			begin_rp.framebuffer = framebuffer_pbr[i];
			begin_rp.renderArea.offset = { 0,0 };
			begin_rp.renderArea.extent = { window_extent.width, window_extent.height };
			std::array<VkClearValue, 2> clearValues = {};
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
			clearValues[1].depthStencil = { 1.0f, 0 };
			begin_rp.pClearValues = clearValues.data();
			begin_rp.clearValueCount = 2;

			vkCmdBeginRenderPass(cmdbuffer_pbr[i], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmdbuffer_pbr[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.pipeline);

			vkCmdBindDescriptorSets(cmdbuffer_pbr[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 0, 1, &program_pbr.GetGlobalDescriptor(i), 0, nullptr);
			
			//per object descriptor/pushconstant/drawcall
			for (int j = 0; j < objectmanager->GetVector().size(); j++)
			{
				//objectmanager->GetVector()[j]
				vkCmdBindDescriptorSets(cmdbuffer_pbr[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_pbr.layout, 1, 1, &program_pbr.GetLocalDescriptor((j == 0) ? teapotid : sphereid, i), 0, nullptr);
				vkCmdPushConstants(cmdbuffer_pbr[i], pipeline_pbr.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &objectmanager->GetVector()[j]->GetMatrix());

				VkDeviceSize sizes[] = { 0 };
				vkCmdBindVertexBuffers(cmdbuffer_pbr[i], 0, 1, &objectmanager->GetVector()[j]->GetMesh().vbo, sizes);
				vkCmdBindIndexBuffer(cmdbuffer_pbr[i], objectmanager->GetVector()[j]->GetMesh().ibo, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmdbuffer_pbr[i], objectmanager->GetVector()[j]->GetMesh().index_size, 1, 0, 0, 0);
			}


			atmosphere->Draw(cmdbuffer_pbr[i], i);

			vkCmdEndRenderPass(cmdbuffer_pbr[i]);
			vkEndCommandBuffer(cmdbuffer_pbr[i]);
		}
		//vkResetCommandBuffer(cmdbuffer_pbr, 0);
		//vkFreeCommandBuffers();	
	}

	bool RenderManager::InitializeVulkan()
	{
		return true;
	}

	void RenderManager::Render()
	{
		//wait for resource key. we have a resource for every frame in flight.
		vkWaitForFences(Device.GetDevice(), 1, &inFlightFences[current_frame_in_flight], VK_TRUE, UINT32_MAX);

		//fetch image index were going to use. semaphore tells us when we actually acquired it
		uint32_t imageIndex;
		vkAcquireNextImageKHR(Device.GetDevice(), Device.GetSwapChain(), UINT64_MAX, semaphore_imagefetch[current_frame_in_flight], VK_NULL_HANDLE, &imageIndex);

		//update cpu data here and recreate commandbuffers
		ReSubmitComputeCmd(current_frame_in_flight, imageIndex);


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
		//vkQueueWaitIdle(Device.GetQueue(POOL_FAMILY::GRAPHICS));

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

		vkQueueSubmit(Device.GetQueue(POOL_FAMILY::COMPUTE), 1, &submit_info_compute, inFlightFences[current_frame_in_flight]);
		//vkQueueWaitIdle(Device.GetQueue(POOL_FAMILY::COMPUTE));

		//submit presentation
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		VkSwapchainKHR swapChains[] = { Device.GetSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &semaphore_computepass[current_frame_in_flight];

		vkQueuePresentKHR(Device.GetQueue(POOL_FAMILY::PRESENT), &presentInfo);
		//vkQueueWaitIdle(Device.GetQueue(POOL_FAMILY::PRESENT));

		current_frame_in_flight = (current_frame_in_flight + 1) % FRAMES_IN_FLIGHT;
	}

	void RenderManager::Update()
	{
		glfwPollEvents();
		
		lightmanager->Update(current_frame_in_flight);
		atmosphere->Update(current_frame_in_flight);
		objectmanager->Update();

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

}