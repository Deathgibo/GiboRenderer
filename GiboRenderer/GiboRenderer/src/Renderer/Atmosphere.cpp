#include "../pch.h"
#include "Atmosphere.h"
#include "vkcore/PhysicalDeviceQuery.h"
#include "vkcore/VulkanHelpers.h"
#include "vkcore/ShaderProgram.h"
#include "RenderObject.h"

namespace Gibo {

	void Atmosphere::Create(VkExtent2D proj_extent, VkExtent2D pipeline_extent, float fov, VkRenderPass renderpass, MeshCache& mcache, int framesinflight, std::vector<vkcoreBuffer> pv_uniform, VkSampleCountFlagBits sample_count)
	{
		//set info
		float EARTH_RADIUS = 6360000.f;
		float ATM_TOP_HEIGHT = 80000.f;
		glm::vec3 f3EarthCentre = -glm::vec3(0, 1, 0) * EARTH_RADIUS;
		info.earth_center = f3EarthCentre;
		info.earth_radius = EARTH_RADIUS;
		info.image_width = lut_width;
		info.image_height = lut_height;
		info.image_depth = lut_depth;
		info.sun_intensity = glm::vec3(.9, .9, .9);
		info.atmosphere_height = ATM_TOP_HEIGHT;

		shader_info.atmosphere_height = ATM_TOP_HEIGHT;
		shader_info.earth_center = glm::vec4(f3EarthCentre, 1);
		shader_info.earth_radius = EARTH_RADIUS;
		shader_info.lightdir = glm::vec4(0, -1, 0, 1);

		float farplanez = 10000;
		float halfx = tan(glm::radians(fov / 2.0)) * farplanez * (proj_extent.width / proj_extent.height);
		float halfy = tan(glm::radians(fov / 2.0)) * farplanez;

		shader_info.farplane = glm::vec4(halfx, halfy, farplanez, 1);
		shader_info.camdirection;
		shader_info.campos;


		//BUFFERS
		shaderinfo_buffer.resize(framesinflight);
		skymatrix_buffer;
		info_buffer;

		//sky matrix buffer
		glm::mat4 skymatrix(1.0f);
		deviceref.CreateBufferStaged(sizeof(glm::mat4), &skymatrix, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, skymatrix_buffer,
			VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

		//info_buffer
		deviceref.CreateBufferStaged(sizeof(atmosphere_info), &info, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, info_buffer,
			VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);


		//shader buffer
		for (int i = 0; i < framesinflight; i++)
		{
			deviceref.CreateBuffer(sizeof(atmosphere_shader), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, shaderinfo_buffer[i]);
		}


		//SKY GEOMETRY
		mcache.LoadMeshFromFile("Models/quad.obj");
		mcache.SetObjectMesh("Models/quad.obj", Sky_Mesh.GetMesh());


		VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT; //VK_FORMAT_R64G64B64A64_SFLOAT
		//RAYLEIGH-LUT
		deviceref.CreateImage(VK_IMAGE_TYPE_3D, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SAMPLE_COUNT_1_BIT, info.image_width, info.image_height, info.image_depth,
			1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, atmosphereLUT);

		PhysicalDeviceQuery::CheckImageOptimalFormat(deviceref.GetPhysicalDevice(), format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
		PhysicalDeviceQuery::CheckImageOptimalFormat(deviceref.GetPhysicalDevice(), format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

		atmosphereview = CreateImageView(deviceref.GetDevice(), atmosphereLUT.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_3D);

		SamplerKey key(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true,
			1.0f, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 1.0f, 0);
		atmospheresampler = deviceref.GetSamplerCache().GetSampler(key);

		TransitionImageLayout(deviceref, atmosphereLUT.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			1, 1, VK_IMAGE_ASPECT_COLOR_BIT);


		//MIE-LUT
		deviceref.CreateImage(VK_IMAGE_TYPE_3D, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SAMPLE_COUNT_1_BIT, info.image_width, info.image_height, info.image_depth,
			1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, MieLUT);
		Mieview = CreateImageView(deviceref.GetDevice(), MieLUT.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_3D);

		TransitionImageLayout(deviceref, MieLUT.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			1, 1, VK_IMAGE_ASPECT_COLOR_BIT);

		//MULTI-SCATTER
		for (int i = 0; i < K; i++)
		{
			//RAYLEIGH GATHERED
			deviceref.CreateImage(VK_IMAGE_TYPE_3D, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, info.image_width, info.image_height, info.image_depth,
				1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, gatheredLUT[i]);
			gatheredView[i] = CreateImageView(deviceref.GetDevice(), gatheredLUT[i].image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_3D);
			TransitionImageLayout(deviceref, gatheredLUT[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				1, 1, VK_IMAGE_ASPECT_COLOR_BIT);

			//MIE GATHERED
			deviceref.CreateImage(VK_IMAGE_TYPE_3D, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, info.image_width, info.image_height, info.image_depth,
				1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, MiegatheredLUT[i]);
			MiegatheredView[i] = CreateImageView(deviceref.GetDevice(), MiegatheredLUT[i].image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_3D);
			TransitionImageLayout(deviceref, MiegatheredLUT[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
		}

		//AMBIENT
		deviceref.CreateImage(VK_IMAGE_TYPE_2D, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, ambient_width, ambient_height, 1, 1, 1,
			VMA_MEMORY_USAGE_GPU_ONLY, 0, ambientLUT);
		ambientview = CreateImageView(deviceref.GetDevice(), ambientLUT.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		TransitionImageLayout(deviceref, ambientLUT.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			1, 1, VK_IMAGE_ASPECT_COLOR_BIT);

		//TRANSMITTANCE
		deviceref.CreateImage(VK_IMAGE_TYPE_2D, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, ambient_width, ambient_height, 1, 1, 1,
			VMA_MEMORY_USAGE_GPU_ONLY, 0, transmittanceLUT);
		transmittanceview = CreateImageView(deviceref.GetDevice(), transmittanceLUT.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D);
		TransitionImageLayout(deviceref, transmittanceLUT.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			1, 1, VK_IMAGE_ASPECT_COLOR_BIT);


		//SKY-RENDERING
		std::vector<ShaderProgram::shadersinfo> info1 = {
			{"Shaders/spv/atmospherevert.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/atmospherefrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo1 = {
			{"ViewProjBuffer", 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
			{"UniformBufferObject", 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
			{"InfoBuffer", 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT},
			{"RayleighLUT", 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"MieLUT", 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo1 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants1 = {
		};

		if (!program_sky.Create(deviceref.GetDevice(), framesinflight, info1.data(), info1.size(), globalinfo1.data(), globalinfo1.size(), localinfo1.data(), localinfo1.size(), pushconstants1.data(), pushconstants1.size(), 0))
		{
			Logger::LogError("failed to create sky shaderprogram\n");
		}

		//SINGLE-SCATTERING
		std::vector<ShaderProgram::shadersinfo> info2 = {
			{"Shaders/spv/singlescatter.spv", VK_SHADER_STAGE_COMPUTE_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo2 = {
			{"ImageBuffer", 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{"RayleighLUT", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"MieLUT", 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo2 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants2 = {
		};

		if (!program_singlescatter.Create(deviceref.GetDevice(), 1, info2.data(), info2.size(), globalinfo2.data(), globalinfo2.size(), localinfo2.data(), localinfo2.size(), pushconstants2.data(), pushconstants2.size(), 0))
		{
			Logger::LogError("failed to create singlescatter shaderprogram\n");
		}

		//MULTI-SCATTERING
		std::vector<ShaderProgram::shadersinfo> info3 = {
			{"Shaders/spv/multiscatter.spv", VK_SHADER_STAGE_COMPUTE_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo3 = {
			{"ImageBuffer", 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{"Rayleighoutput", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"Mieoutput", 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"RayleighLUT", 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"MieLUT", 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo3 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants3 = {
		};

		if (!program_multiscatter.Create(deviceref.GetDevice(), 1, info3.data(), info3.size(), globalinfo3.data(), globalinfo3.size(), localinfo3.data(), localinfo3.size(), pushconstants3.data(), pushconstants3.size(), 0))
		{
			Logger::LogError("failed to create multiscatter shaderprogram\n");
		}

		//COMBINING SCATTERS
		std::vector<ShaderProgram::shadersinfo> info4 = {
			{"Shaders/spv/multiscattercombine.spv", VK_SHADER_STAGE_COMPUTE_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo4 = {
			{"Routput_tex", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"Moutput_tex", 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"Rinput_tex", 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"Minput_tex", 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo4 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants4 = {
		};

		if (!program_combine.Create(deviceref.GetDevice(), 1, info4.data(), info4.size(), globalinfo4.data(), globalinfo4.size(), localinfo4.data(), localinfo4.size(), pushconstants4.data(), pushconstants4.size(), 0))
		{
			Logger::LogError("failed to create combine shaderprogram\n");
		}

		//AMBIENT 
		std::vector<ShaderProgram::shadersinfo> info5 = {
			{"Shaders/spv/ambientatmosphere.spv", VK_SHADER_STAGE_COMPUTE_BIT}
		};
		std::vector<ShaderProgram::descriptorinfo> globalinfo5 = {
			{"AtmosphereLUT", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"ImageBuffer", 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{"TransmittanceLUT", 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL},
			{"RayleighLUT", 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{"MieLUT", 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};
		std::vector<ShaderProgram::descriptorinfo> localinfo5 = {
		};
		std::vector<ShaderProgram::pushconstantinfo> pushconstants5 = {
		};

		if (!program_ambient.Create(deviceref.GetDevice(), 1, info5.data(), info5.size(), globalinfo5.data(), globalinfo5.size(), localinfo5.data(), localinfo5.size(), pushconstants5.data(), pushconstants5.size(), 0))
		{
			Logger::LogError("failed to create ambient shaderprogram\n");
		}

		//pipelines
		pipeline_singlescatter = deviceref.GetPipelineCache().GetComputePipeline(program_singlescatter.GetShaderStageInfo()[0], &program_singlescatter.GetGlobalLayout(), 1, program_singlescatter.GetPushRanges());

		pipeline_multiscatter = deviceref.GetPipelineCache().GetComputePipeline(program_multiscatter.GetShaderStageInfo()[0], &program_multiscatter.GetGlobalLayout(), 1, program_multiscatter.GetPushRanges());

		pipeline_combine = deviceref.GetPipelineCache().GetComputePipeline(program_combine.GetShaderStageInfo()[0], &program_combine.GetGlobalLayout(), 1, program_combine.GetPushRanges());

		pipeline_ambient = deviceref.GetPipelineCache().GetComputePipeline(program_ambient.GetShaderStageInfo()[0], &program_ambient.GetGlobalLayout(), 1, program_ambient.GetPushRanges());


		CreateSwapChainData(pipeline_extent, renderpass, sample_count);

		//filling descriptors

		//SKY-RENDERING
		DescriptorHelper global_descriptors(framesinflight);
		for (int i = 0; i < framesinflight; i++)
		{
			//projview buffer
			global_descriptors.buffersizes[i].push_back(sizeof(glm::mat4) * 2);
			global_descriptors.uniformbuffers[i].push_back(pv_uniform[i]); 
			//model matrix, I can just make this myself since its constant
			global_descriptors.buffersizes[i].push_back(sizeof(glm::mat4));
			global_descriptors.uniformbuffers[i].push_back(skymatrix_buffer);
			//atmosphere shader
			global_descriptors.buffersizes[i].push_back(sizeof(atmosphere_shader));
			global_descriptors.uniformbuffers[i].push_back(shaderinfo_buffer[i]);

			if (K == 0)
			{
				global_descriptors.imageviews[i].push_back(atmosphereview);
				global_descriptors.imageviews[i].push_back(Mieview);
			}
			else
			{
				global_descriptors.imageviews[i].push_back(gatheredView[K - 1]);
				global_descriptors.imageviews[i].push_back(MiegatheredView[K - 1]);
			}
			global_descriptors.samplers[i].push_back(atmospheresampler);
			global_descriptors.samplers[i].push_back(atmospheresampler);

		}
		program_sky.SetGlobalDescriptor(global_descriptors.uniformbuffers, global_descriptors.buffersizes, global_descriptors.imageviews, global_descriptors.samplers, global_descriptors.bufferviews);

		//SINGLE-SCATTER
		DescriptorHelper global_descriptors2(1);

		global_descriptors2.buffersizes[0].push_back(sizeof(atmosphere_info));
		global_descriptors2.uniformbuffers[0].push_back(info_buffer);

		global_descriptors2.imageviews[0].push_back(atmosphereview);
		global_descriptors2.samplers[0].push_back(atmospheresampler);

		global_descriptors2.imageviews[0].push_back(Mieview);
		global_descriptors2.samplers[0].push_back(atmospheresampler);

		program_singlescatter.SetGlobalDescriptor(global_descriptors2.uniformbuffers, global_descriptors2.buffersizes, global_descriptors2.imageviews, global_descriptors2.samplers, global_descriptors2.bufferviews);

		//MULTI-SCATTER
		DescriptorHelper global_descriptors3(1);
		if (K > 0)
		{
			global_descriptors3.buffersizes[0].push_back(sizeof(atmosphere_info));
			global_descriptors3.uniformbuffers[0].push_back(info_buffer);

			global_descriptors3.imageviews[0].push_back(gatheredView[0]);
			global_descriptors3.samplers[0].push_back(atmospheresampler);

			global_descriptors3.imageviews[0].push_back(MiegatheredView[0]);
			global_descriptors3.samplers[0].push_back(atmospheresampler);

			global_descriptors3.imageviews[0].push_back(atmosphereview);
			global_descriptors3.samplers[0].push_back(atmospheresampler);

			global_descriptors3.imageviews[0].push_back(Mieview);
			global_descriptors3.samplers[0].push_back(atmospheresampler);

			program_multiscatter.SetGlobalDescriptor(global_descriptors3.uniformbuffers, global_descriptors3.buffersizes, global_descriptors3.imageviews, global_descriptors3.samplers, global_descriptors3.bufferviews);

		}

		//COMBINING MULTI-SCATTER
		DescriptorHelper global_descriptors4(1);
		if (K > 0)
		{
			global_descriptors4.imageviews[0].push_back(gatheredView[0]);
			global_descriptors4.samplers[0].push_back(atmospheresampler);

			global_descriptors4.imageviews[0].push_back(MiegatheredView[0]);
			global_descriptors4.samplers[0].push_back(atmospheresampler);

			global_descriptors4.imageviews[0].push_back(atmosphereview);
			global_descriptors4.samplers[0].push_back(atmospheresampler);

			global_descriptors4.imageviews[0].push_back(Mieview);
			global_descriptors4.samplers[0].push_back(atmospheresampler);

			program_combine.SetGlobalDescriptor(global_descriptors4.uniformbuffers, global_descriptors4.buffersizes, global_descriptors4.imageviews, global_descriptors4.samplers, global_descriptors4.bufferviews);
		}

		//AMBIENT
		DescriptorHelper global_descriptors5(1);

		global_descriptors5.buffersizes[0].push_back(sizeof(atmosphere_info));
		global_descriptors5.uniformbuffers[0].push_back(info_buffer);

		global_descriptors5.imageviews[0].push_back(ambientview);
		global_descriptors5.samplers[0].push_back(atmospheresampler);

		global_descriptors5.imageviews[0].push_back(transmittanceview);
		global_descriptors5.samplers[0].push_back(atmospheresampler);

		if (K == 0)
		{
			global_descriptors5.imageviews[0].push_back(atmosphereview);
			global_descriptors5.imageviews[0].push_back(Mieview);
		}
		else
		{
			global_descriptors5.imageviews[0].push_back(gatheredView[K - 1]);
			global_descriptors5.imageviews[0].push_back(MiegatheredView[K - 1]);
		}
		global_descriptors5.samplers[0].push_back(atmospheresampler);
		global_descriptors5.samplers[0].push_back(atmospheresampler);

		program_ambient.SetGlobalDescriptor(global_descriptors5.uniformbuffers, global_descriptors5.buffersizes, global_descriptors5.imageviews, global_descriptors5.samplers, global_descriptors5.bufferviews);


		//COMMANDBUFFERS
		VkCommandPool computepool = deviceref.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE);

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = computepool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		VULKAN_CHECK(vkAllocateCommandBuffers(deviceref.GetDevice(), &allocInfo, &cmdbuffer_singlescatter), "allocating compute atmosphere cmdbuffers");
		VULKAN_CHECK(vkAllocateCommandBuffers(deviceref.GetDevice(), &allocInfo, &cmdbuffer_multiscatter), "allocating compute atmosphere cmdbuffers");
		VULKAN_CHECK(vkAllocateCommandBuffers(deviceref.GetDevice(), &allocInfo, &cmdbuffer_combine), "allocating compute atmosphere cmdbuffers");
		VULKAN_CHECK(vkAllocateCommandBuffers(deviceref.GetDevice(), &allocInfo, &cmdbuffer_ambient), "allocating compute atmosphere cmdbuffers");

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		//TODO - make sure we don't overuse them
		VkPhysicalDeviceLimits limits = PhysicalDeviceQuery::GetDeviceLimits(deviceref.GetPhysicalDevice());
		limits.maxComputeWorkGroupInvocations;
		

		//single-scatter
		VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_singlescatter, &beginInfo), "begin single scatter cmdbuffer");
		vkCmdBindPipeline(cmdbuffer_singlescatter, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_singlescatter.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_singlescatter, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_singlescatter.layout, 0, 1, &program_singlescatter.GetGlobalDescriptor(0), 0, nullptr);
		vkCmdDispatch(cmdbuffer_singlescatter, info.image_width / WORKGROUP_SIZE.x, info.image_height / WORKGROUP_SIZE.y, info.image_depth / WORKGROUP_SIZE.z);
		VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_singlescatter), "end cmdbuffer");

		if (K > 0)
		{
			//multi-scatter
			VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_multiscatter, &beginInfo), "begin multi scatter cmdbuffer");
			vkCmdBindPipeline(cmdbuffer_multiscatter, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_multiscatter.pipeline);
			vkCmdBindDescriptorSets(cmdbuffer_multiscatter, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_multiscatter.layout, 0, 1, &program_multiscatter.GetGlobalDescriptor(0), 0, nullptr);
			vkCmdDispatch(cmdbuffer_multiscatter, info.image_width / WORKGROUP_SIZE.x, info.image_height / WORKGROUP_SIZE.y, info.image_depth / WORKGROUP_SIZE.z);
			VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_multiscatter), "end cmdbuffer");

			//combine
			VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_combine, &beginInfo), "begin combine cmdbuffer");
			vkCmdBindPipeline(cmdbuffer_combine, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_combine.pipeline);
			vkCmdBindDescriptorSets(cmdbuffer_combine, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_combine.layout, 0, 1, &program_combine.GetGlobalDescriptor(0), 0, nullptr);
			vkCmdDispatch(cmdbuffer_combine, info.image_width / WORKGROUP_SIZE.x, info.image_height / WORKGROUP_SIZE.y, info.image_depth / WORKGROUP_SIZE.z);
			VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_combine), "end cmdbuffer");
		}
		//ambient
		VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_ambient, &beginInfo), "begin ambient cmdbuffer");
		vkCmdBindPipeline(cmdbuffer_ambient, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_ambient.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer_ambient, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_ambient.layout, 0, 1, &program_ambient.GetGlobalDescriptor(0), 0, nullptr);
		vkCmdDispatch(cmdbuffer_ambient, ambient_width / WORKGROUP_SIZE_AMBIENT.x, ambient_height / WORKGROUP_SIZE_AMBIENT.y, 1 / WORKGROUP_SIZE_AMBIENT.z);
		VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_ambient), "end cmdbuffer");
	}

	void Atmosphere::CleanUp()
	{
		for (int i = 0; i < shaderinfo_buffer.size(); i++)
		{
			deviceref.DestroyBuffer(shaderinfo_buffer[i]);
		}
		//TODO - free up my one time memory stuff after were done
		deviceref.DestroyBuffer(info_buffer);
		deviceref.DestroyBuffer(skymatrix_buffer);
	

		deviceref.DestroyImage(atmosphereLUT);
		vkDestroyImageView(deviceref.GetDevice(), atmosphereview, nullptr);

		deviceref.DestroyImage(MieLUT);
		vkDestroyImageView(deviceref.GetDevice(), Mieview, nullptr);

		for (int i = 0; i < gatheredLUT.size(); i++)
		{
			deviceref.DestroyImage(gatheredLUT[i]);
			vkDestroyImageView(deviceref.GetDevice(), gatheredView[i], nullptr);

			deviceref.DestroyImage(MiegatheredLUT[i]);
			vkDestroyImageView(deviceref.GetDevice(), MiegatheredView[i], nullptr);
		}

		deviceref.DestroyImage(ambientLUT);
		vkDestroyImageView(deviceref.GetDevice(), ambientview, nullptr);

		deviceref.DestroyImage(transmittanceLUT);
		vkDestroyImageView(deviceref.GetDevice(), transmittanceview, nullptr);



		program_sky.CleanUp();
		program_singlescatter.CleanUp();
		program_multiscatter.CleanUp();
		program_combine.CleanUp();
		program_ambient.CleanUp();

		vkDestroyPipelineLayout(deviceref.GetDevice(), pipeline_sky.layout, nullptr);
		vkDestroyPipeline(deviceref.GetDevice(), pipeline_sky.pipeline, nullptr);
		
		vkDestroyPipelineLayout(deviceref.GetDevice(), pipeline_singlescatter.layout, nullptr);
		vkDestroyPipeline(deviceref.GetDevice(), pipeline_singlescatter.pipeline, nullptr);
		
		vkDestroyPipelineLayout(deviceref.GetDevice(), pipeline_multiscatter.layout, nullptr);
		vkDestroyPipeline(deviceref.GetDevice(), pipeline_multiscatter.pipeline, nullptr);
		
		vkDestroyPipelineLayout(deviceref.GetDevice(), pipeline_combine.layout, nullptr);
		vkDestroyPipeline(deviceref.GetDevice(), pipeline_combine.pipeline, nullptr);
		
		vkDestroyPipelineLayout(deviceref.GetDevice(), pipeline_ambient.layout, nullptr);
		vkDestroyPipeline(deviceref.GetDevice(), pipeline_ambient.pipeline, nullptr);


		vkFreeCommandBuffers(deviceref.GetDevice(), deviceref.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE), 1, &cmdbuffer_singlescatter);
		vkFreeCommandBuffers(deviceref.GetDevice(), deviceref.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE), 1, &cmdbuffer_multiscatter);
		vkFreeCommandBuffers(deviceref.GetDevice(), deviceref.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE), 1, &cmdbuffer_combine);
		vkFreeCommandBuffers(deviceref.GetDevice(), deviceref.GetCommandPoolCache().GetCommandPool(POOL_TYPE::DYNAMIC, POOL_FAMILY::COMPUTE), 1, &cmdbuffer_ambient);
	}

	void Atmosphere::FillLUT()
	{
		//compute single-scatter
		VkSubmitInfo submitinfo_singlescatter = {};
		submitinfo_singlescatter.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitinfo_singlescatter.waitSemaphoreCount = 0;
		submitinfo_singlescatter.commandBufferCount = 1;
		submitinfo_singlescatter.pCommandBuffers = &cmdbuffer_singlescatter;
		submitinfo_singlescatter.signalSemaphoreCount = 0;

		VULKAN_CHECK(vkQueueSubmit(deviceref.GetQueue(POOL_FAMILY::COMPUTE), 1, &submitinfo_singlescatter, VK_NULL_HANDLE), "submitting single scatter cmd");
		vkQueueWaitIdle(deviceref.GetQueue(POOL_FAMILY::COMPUTE));
		vkDeviceWaitIdle(deviceref.GetDevice());

		VkPipelineStageFlags laststage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		if (K > 0)
		{
			laststage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
		TransitionImageLayout(deviceref, atmosphereLUT.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, laststage, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
		TransitionImageLayout(deviceref, MieLUT.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, laststage, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
		vkDeviceWaitIdle(deviceref.GetDevice());

		//compute K amount of multiscattering using previous one as input for the next
		for (int i = 0; i < K; i++)
		{
			VkSubmitInfo submitinfo_multiscatter = {};
			submitinfo_multiscatter.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitinfo_multiscatter.waitSemaphoreCount = 0;
			submitinfo_multiscatter.commandBufferCount = 1;
			submitinfo_multiscatter.pCommandBuffers = &cmdbuffer_multiscatter;
			submitinfo_multiscatter.signalSemaphoreCount = 0;

			VULKAN_CHECK(vkQueueSubmit(deviceref.GetQueue(POOL_FAMILY::COMPUTE), 1, &submitinfo_multiscatter, VK_NULL_HANDLE), "submitting multiscatter cmd buffer");
			vkQueueWaitIdle(deviceref.GetQueue(POOL_FAMILY::COMPUTE));

			TransitionImageLayout(deviceref, gatheredLUT[i].image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
			TransitionImageLayout(deviceref, MiegatheredLUT[i].image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);

			//change descriptorset
			//*might have to rebind cmdbuffer?
			if (i < K - 1)
			{
				DescriptorHelper global_descriptors3(1);
				int j = i + 1;
				global_descriptors3.buffersizes[0].push_back(sizeof(atmosphere_info));
				global_descriptors3.uniformbuffers[0].push_back(info_buffer);

				global_descriptors3.imageviews[0].push_back(gatheredView[j]);
				global_descriptors3.samplers[0].push_back(atmospheresampler);

				global_descriptors3.imageviews[0].push_back(MiegatheredView[j]);
				global_descriptors3.samplers[0].push_back(atmospheresampler);

				global_descriptors3.imageviews[0].push_back(gatheredView[j - 1]);
				global_descriptors3.samplers[0].push_back(atmospheresampler);

				global_descriptors3.imageviews[0].push_back(MiegatheredView[j - 1]);
				global_descriptors3.samplers[0].push_back(atmospheresampler);

				program_multiscatter.SetGlobalDescriptor(global_descriptors3.uniformbuffers, global_descriptors3.buffersizes, global_descriptors3.imageviews, global_descriptors3.samplers, global_descriptors3.bufferviews);
				
				//we changed descriptorset rebind cmdbuffer
				vkResetCommandBuffer(cmdbuffer_multiscatter, 0);

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_multiscatter, &beginInfo), "begin multi scatter cmdbuffer");
				vkCmdBindPipeline(cmdbuffer_multiscatter, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_multiscatter.pipeline);
				vkCmdBindDescriptorSets(cmdbuffer_multiscatter, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_multiscatter.layout, 0, 1, &program_multiscatter.GetGlobalDescriptor(0), 0, nullptr);
				vkCmdDispatch(cmdbuffer_multiscatter, info.image_width / WORKGROUP_SIZE.x, info.image_height / WORKGROUP_SIZE.y, info.image_depth / WORKGROUP_SIZE.z);
				VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_multiscatter), "end cmdbuffer");
			}
		}

		//if K==0 just use atmosphereLUT else add up the k gatheredLUTs
		if (K == 0)
		{
			//do nothing
		}
		else
		{
			//transition each back to general
			for (int i = 0; i < K; i++)
			{
				TransitionImageLayout(deviceref, gatheredLUT[i].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, 
					VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);	
				TransitionImageLayout(deviceref, MiegatheredLUT[i].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT,
					VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
			}
			TransitionImageLayout(deviceref, atmosphereLUT.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
			TransitionImageLayout(deviceref, MieLUT.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
			//combine
			for (int i = 0; i < K; i++)
			{
				VkSubmitInfo submitinfo_combine = {};
				submitinfo_combine.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitinfo_combine.waitSemaphoreCount = 0;
				submitinfo_combine.commandBufferCount = 1;
				submitinfo_combine.pCommandBuffers = &cmdbuffer_combine;
				submitinfo_combine.signalSemaphoreCount = 0;

				VULKAN_CHECK(vkQueueSubmit(deviceref.GetQueue(POOL_FAMILY::COMPUTE), 1, &submitinfo_combine, VK_NULL_HANDLE), "submitting combine cmd buffer");
				vkQueueWaitIdle(deviceref.GetQueue(POOL_FAMILY::COMPUTE));

				//change descriptorset
				//*might have to rebind cmdbuffer?
				if (i < K - 1)
				{
					DescriptorHelper global_descriptors4(1);
					int j = i + 1;
					global_descriptors4.imageviews[0].push_back(gatheredView[j]);
					global_descriptors4.samplers[0].push_back(atmospheresampler);

					global_descriptors4.imageviews[0].push_back(MiegatheredView[j]);
					global_descriptors4.samplers[0].push_back(atmospheresampler);

					global_descriptors4.imageviews[0].push_back(gatheredView[i]);
					global_descriptors4.samplers[0].push_back(atmospheresampler);

					global_descriptors4.imageviews[0].push_back(MiegatheredView[i]);
					global_descriptors4.samplers[0].push_back(atmospheresampler);

					program_combine.SetGlobalDescriptor(global_descriptors4.uniformbuffers, global_descriptors4.buffersizes, global_descriptors4.imageviews, global_descriptors4.samplers, global_descriptors4.bufferviews);
				
					//we changed descriptorset rebind cmdbuffer
					vkResetCommandBuffer(cmdbuffer_combine, 0);

					VkCommandBufferBeginInfo beginInfo{};
					beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
					VULKAN_CHECK(vkBeginCommandBuffer(cmdbuffer_combine, &beginInfo), "begin combine cmdbuffer");
					vkCmdBindPipeline(cmdbuffer_combine, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_combine.pipeline);
					vkCmdBindDescriptorSets(cmdbuffer_combine, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_combine.layout, 0, 1, &program_combine.GetGlobalDescriptor(0), 0, nullptr);
					vkCmdDispatch(cmdbuffer_combine, info.image_width / WORKGROUP_SIZE.x, info.image_height / WORKGROUP_SIZE.y, info.image_depth / WORKGROUP_SIZE.z);
					VULKAN_CHECK(vkEndCommandBuffer(cmdbuffer_combine), "end cmdbuffer");
				}
			}
			//transition each back to shaderreadonly
			for (int i = 0; i < K; i++)
			{
				TransitionImageLayout(deviceref, gatheredLUT[i].image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, 
					VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
				TransitionImageLayout(deviceref, MiegatheredLUT[i].image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
					VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
			}
			TransitionImageLayout(deviceref, atmosphereLUT.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
			TransitionImageLayout(deviceref, MieLUT.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
		}

		//Fill Ambient LUT
		info.image_width = ambient_width;
		info.image_height = ambient_height;
		//copy data by staging buffer
		vkcoreBuffer stagingbuffer;
		deviceref.CreateBuffer(sizeof(atmosphere_info), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, stagingbuffer);
		deviceref.BindData(stagingbuffer.allocation, &info, sizeof(atmosphere_info));
		CopyBufferToBuffer(deviceref, stagingbuffer.buffer, info_buffer.buffer, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, sizeof(atmosphere_info));
		deviceref.DestroyBuffer(stagingbuffer);

		VkSubmitInfo submitAmbient = {};
		submitAmbient.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitAmbient.waitSemaphoreCount = 0;
		submitAmbient.commandBufferCount = 1;
		submitAmbient.pCommandBuffers = &cmdbuffer_ambient;
		submitAmbient.signalSemaphoreCount = 0;

		VULKAN_CHECK(vkQueueSubmit(deviceref.GetQueue(POOL_FAMILY::COMPUTE), 1, &submitAmbient, VK_NULL_HANDLE), "submitting ambient cmd buffer");
		vkQueueWaitIdle(deviceref.GetQueue(POOL_FAMILY::COMPUTE));

		TransitionImageLayout(deviceref, ambientLUT.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
		TransitionImageLayout(deviceref, transmittanceLUT.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void Atmosphere::Update(int framecount)
	{
		if (needs_updated)
		{
			BindAtmosphereBuffer(framecount);

			frames_updated++;
			if (frames_updated >= shaderinfo_buffer.size())
			{
				frames_updated = 0;
				needs_updated = false;
			}
		}
	}

	void Atmosphere::UpdateFarPlane(VkExtent2D window_extent, float fov)
	{
		float width = window_extent.width;
		float height = window_extent.height;
		float angle = fov;

		float farplanez = 10000;
		float halfx = tan(glm::radians(angle / 2.0)) * farplanez * (width / height);
		float halfy = tan(glm::radians(angle / 2.0)) * farplanez;

		shader_info.farplane = glm::vec4(halfx, halfy, farplanez, 1);
		NotifyUpdate();
	}

	void Atmosphere::BindAtmosphereBuffer(int x)
	{
		deviceref.BindData(shaderinfo_buffer[x].allocation, &shader_info, sizeof(atmosphere_shader));
	}

	void Atmosphere::CreateSwapChainData(VkExtent2D pipeline_extent, VkRenderPass renderpass, VkSampleCountFlagBits sample_count)
	{
		PipelineData pipelinedata(pipeline_extent.width, pipeline_extent.height);

		pipelinedata.ColorBlendstate.blendenable = VK_FALSE;

		pipelinedata.Rasterizationstate.cullmode = VK_CULL_MODE_NONE; // VK_CULL_MODE_FRONT_BIT
		pipelinedata.Rasterizationstate.frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelinedata.Rasterizationstate.polygonmode = VK_POLYGON_MODE_FILL;

		pipelinedata.DepthStencilstate.depthtestenable = VK_TRUE;
		pipelinedata.DepthStencilstate.depthcompareop = VK_COMPARE_OP_LESS_OR_EQUAL;
		pipelinedata.DepthStencilstate.depthwriteenable = VK_FALSE;

		pipelinedata.Multisamplingstate.samplecount = sample_count;
		pipelinedata.Multisamplingstate.sampleshadingenable = VK_TRUE;
		pipelinedata.Multisamplingstate.minsampleshading = .5;


		pipelinedata.Inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		pipeline_sky = deviceref.GetPipelineCache().GetGraphicsPipeline(pipelinedata, deviceref.GetPhysicalDevice(), renderpass, program_sky.GetShaderStageInfo(), program_sky.GetPushRanges(),
			&program_sky.GetGlobalLayout(), 1);
	}

	void Atmosphere::SwapChainRecreate(VkExtent2D pipeline_extent, VkExtent2D proj_extent, float fov, VkRenderPass renderpass, VkSampleCountFlagBits sample_count)
	{
		vkDestroyPipelineLayout(deviceref.GetDevice(), pipeline_sky.layout, nullptr);
		vkDestroyPipeline(deviceref.GetDevice(), pipeline_sky.pipeline, nullptr);

		//recreate
		CreateSwapChainData(pipeline_extent, renderpass, sample_count);

		UpdateFarPlane(proj_extent, fov);
	}

	void Atmosphere::Draw(VkCommandBuffer cmdbuffer, int current_frame)
	{
		//SKY_RENDERING
		vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_sky.pipeline);
		vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_sky.layout, 0, 1, &program_sky.GetGlobalDescriptor(current_frame), 0, nullptr);

		VkBuffer buffers[] = { Sky_Mesh.GetMesh().vbo };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmdbuffer, 0, 1, buffers, offsets);
		vkCmdBindIndexBuffer(cmdbuffer, Sky_Mesh.GetMesh().ibo, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(cmdbuffer, static_cast<uint32_t>(Sky_Mesh.GetMesh().index_size), 1, 0, 0, 0);
	}
}