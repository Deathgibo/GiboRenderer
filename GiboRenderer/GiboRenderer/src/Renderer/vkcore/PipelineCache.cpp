#include "../../pch.h"
#include "PipelineCache.h"
#include "PhysicalDeviceQuery.h"

namespace Gibo {

	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 tangent;
		glm::vec3 bitangent;

		static const uint32_t attribute_count = 5;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription descript{};
			descript.binding = 0;
			descript.stride = sizeof(Vertex);
			descript.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //VK_VERTEX_INPUT_RATE_INSTANCE

			return descript;
		}

		static std::array<VkVertexInputAttributeDescription, attribute_count> getAttributeDescription()
		{
			//SINT signed 32-bit integers
			//UINT unsigned 32-bit integers
			//SFLOAT 64 bit float

			std::array<VkVertexInputAttributeDescription, attribute_count> descripts = {};
			descripts[0].binding = 0;
			descripts[0].location = 0;
			descripts[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			descripts[0].offset = offsetof(Vertex, pos);

			descripts[1].binding = 0;
			descripts[1].location = 1;
			descripts[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			descripts[1].offset = offsetof(Vertex, normal);

			descripts[2].binding = 0;
			descripts[2].location = 2;
			descripts[2].format = VK_FORMAT_R32G32_SFLOAT;
			descripts[2].offset = offsetof(Vertex, uv);

			descripts[3].binding = 0;
			descripts[3].location = 3;
			descripts[3].format = VK_FORMAT_R32G32B32_SFLOAT;
			descripts[3].offset = offsetof(Vertex, tangent);

			descripts[4].binding = 0;
			descripts[4].location = 4;
			descripts[4].format = VK_FORMAT_R32G32B32_SFLOAT;
			descripts[4].offset = offsetof(Vertex, bitangent);

			return descripts;
		}
	};

	void PipelineCache::PrintDebug() const
	{
		Logger::Log("-----PipelineCache Info-----\n");
		Logger::Log("number of Pipelines stored: ", PipelineArray.size(), "capacity in vector: ", PipelineArray.capacity(), "\n");
	}

	void PipelineCache::Cleanup()
	{
		
		/*  I want to be able to control pipeline memory because swap chain recreation 
		//free every Pipeline Layout
		for (auto layout : LayoutArray)
		{
			vkDestroyPipelineLayout(deviceref, layout, nullptr);
		}
		LayoutArray.clear();
		//free every Pipeline
		for (auto pipeline : PipelineArray)
		{
			vkDestroyPipeline(deviceref, pipeline, nullptr);
		}
		PipelineArray.clear();
		*/
	}

	vkcorePipeline PipelineCache::GetGraphicsPipeline(PipelineData data, VkPhysicalDevice physicaldevice, VkRenderPass renderpass, const std::vector<VkPipelineShaderStageCreateInfo>& moduleinfo,
														std::vector<VkPushConstantRange>& ranges, VkDescriptorSetLayout* layouts, uint32_t layouts_size)
	{
		for (auto& pushconstantsize : ranges)
		{
			if (pushconstantsize.size > PhysicalDeviceQuery::GetDeviceLimits(physicaldevice).maxPushConstantsSize)
			{
				Logger::LogError("Push constant size greater than ", PhysicalDeviceQuery::GetDeviceLimits(physicaldevice).maxPushConstantsSize, " bytes\n");
			}
		}

		VkPipeline current_pipeline;
		VkPipelineLayout current_layout;

		//specify the format of the vertex data like a vao
		VkVertexInputBindingDescription bindingDescription                    = Vertex::getBindingDescription();
		std::array<VkVertexInputAttributeDescription, Vertex::attribute_count> attributeDescription = Vertex::getAttributeDescription();

		VkPipelineVertexInputStateCreateInfo vertexinputinfo = {};
		vertexinputinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexinputinfo.vertexAttributeDescriptionCount = Vertex::attribute_count;
		vertexinputinfo.pVertexAttributeDescriptions = attributeDescription.data(); //describe attributes, bindings, offsets
		vertexinputinfo.vertexBindingDescriptionCount = 1;
		vertexinputinfo.pVertexBindingDescriptions = &bindingDescription; //spacing between data

		//specify the topology and if you want to restart indices
		VkPipelineInputAssemblyStateCreateInfo assemblyinfo = {};
		assemblyinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assemblyinfo.topology = data.Inputassembly.topology;
		assemblyinfo.primitiveRestartEnable = VK_FALSE;

		//tessellation - not supported yet
		VkPipelineTessellationStateCreateInfo tessinfo = {};

		//viewport specify the viewports and scissors you want
		VkPipelineViewportStateCreateInfo viewportinfo = {};
		viewportinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportinfo.viewportCount = 1;
		viewportinfo.pViewports = &data.ViewPortstate.viewport;
		viewportinfo.scissorCount = 1;
		viewportinfo.pScissors = &data.ViewPortstate.scissor;

		//specify some functionality you want the rasterizer to do like culling, depth values, drawing
		VkPipelineRasterizationStateCreateInfo rasterinfo = {};
		rasterinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterinfo.depthClampEnable = VK_FALSE;
		rasterinfo.rasterizerDiscardEnable = VK_FALSE;
		rasterinfo.polygonMode = data.Rasterizationstate.polygonmode;
		rasterinfo.lineWidth = data.Rasterizationstate.linewidth;
		rasterinfo.cullMode = data.Rasterizationstate.cullmode;// VK_CULL_MODE_BACK_BIT;
		rasterinfo.frontFace = data.Rasterizationstate.frontface;
		rasterinfo.depthBiasEnable = data.Rasterizationstate.depthBiasEnable;
		rasterinfo.depthBiasConstantFactor = data.Rasterizationstate.depthBiasConstantFactor;
		rasterinfo.depthBiasSlopeFactor = data.Rasterizationstate.depthBiasSlopeFactor;
		rasterinfo.depthBiasClamp = data.Rasterizationstate.depthBiasClamp;
		rasterinfo.depthClampEnable = data.Rasterizationstate.depthClampEnable;

		//multisampling features like anti-aliasing can be enabled here
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.flags = 0;
		multisampling.sampleShadingEnable = data.Multisamplingstate.sampleshadingenable;
		multisampling.minSampleShading = data.Multisamplingstate.minsampleshading;
		multisampling.rasterizationSamples = data.Multisamplingstate.samplecount;// vkcoreState::GetInstance()->GetSampleCount();

		//configure the depth and stencil buffer like depth and stencil tests
		VkPipelineDepthStencilStateCreateInfo depthinfo = {};
		depthinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthinfo.depthTestEnable = data.DepthStencilstate.depthtestenable;
		depthinfo.depthWriteEnable = data.DepthStencilstate.depthwriteenable;
		depthinfo.depthCompareOp = data.DepthStencilstate.depthcompareop;
		depthinfo.depthBoundsTestEnable = data.DepthStencilstate.boundsenable;
		depthinfo.minDepthBounds = data.DepthStencilstate.min_bounds;
		depthinfo.maxDepthBounds = data.DepthStencilstate.max_bounds;
		depthinfo.stencilTestEnable = data.DepthStencilstate.stenciltestenable;

		//changes info about the color blending equation
		//destination is color stored in buffer currently
		//src is output of current frag shader
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = data.ColorBlendstate.blendenable;
		colorBlendAttachment.colorBlendOp = data.ColorBlendstate.colorblendop;
		colorBlendAttachment.dstColorBlendFactor = data.ColorBlendstate.dstblendfactor;
		colorBlendAttachment.srcColorBlendFactor = data.ColorBlendstate.srcblendfactor;
		colorBlendAttachment.alphaBlendOp = data.ColorBlendstate.alphablendop;
		colorBlendAttachment.dstAlphaBlendFactor = data.ColorBlendstate.dstalphablendfactor;
		colorBlendAttachment.srcAlphaBlendFactor = data.ColorBlendstate.srcalphablendfactor;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = data.ColorBlendstate.blendconstant0;
		colorBlending.blendConstants[1] = data.ColorBlendstate.blendconstant1;
		colorBlending.blendConstants[2] = data.ColorBlendstate.blendconstant2;
		colorBlending.blendConstants[3] = data.ColorBlendstate.blendconstant3;

		//for stages in pipeline you want to dynamically change - not supported
		VkPipelineDynamicStateCreateInfo dynamicinfo = {};
		dynamicinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicinfo.flags = 0;
		dynamicinfo.dynamicStateCount = 2;
		VkDynamicState dstates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		dynamicinfo.pDynamicStates = dstates;

		//create pipelinecache if you want to commuincate between pipelines - not supported
		/*VkPipelineCacheCreateInfo cacheinfo = {};
		cacheinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		cacheinfo.flags = 0;
		cacheinfo.initialDataSize = 0;
		cacheinfo.pInitialData = nullptr;
		VULKAN_CHECK(vkCreatePipelineCache(deviceref, &cacheinfo, nullptr, &pipelinecache), "creating pipeline cache");
		*/

		//this is where you set up uniform values in shaders, specifies what resources can be accessed by a pipeline
		VkPipelineLayoutCreateInfo layoutinfo = {};
		layoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutinfo.setLayoutCount = layouts_size;
		layoutinfo.pSetLayouts = layouts;
		layoutinfo.pushConstantRangeCount = ranges.size();
		if (ranges.size() != 0)
		{
			layoutinfo.pPushConstantRanges = ranges.data();
		}


		VULKAN_CHECK(vkCreatePipelineLayout(deviceref, &layoutinfo, nullptr, &current_layout), "creating pipeline layout");

		//create the graphicspipelinecreateinfo and add everything to the monolithic struct
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = moduleinfo.size();
		pipelineInfo.pStages = moduleinfo.data();

		pipelineInfo.pVertexInputState = &vertexinputinfo;
		pipelineInfo.pInputAssemblyState = &assemblyinfo;
		pipelineInfo.pViewportState = &viewportinfo;
		pipelineInfo.pRasterizationState = &rasterinfo;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthinfo;
		pipelineInfo.pColorBlendState = &colorBlending;
		if (data.ViewPortstate.dynamicviewport)
		{
			pipelineInfo.pDynamicState = &dynamicinfo;
		}
		
		pipelineInfo.layout = current_layout;
		pipelineInfo.renderPass = renderpass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		VULKAN_CHECK(vkCreateGraphicsPipelines(deviceref, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &current_pipeline), "create graphics pipeline");

		//store pipeline and layout is arrays
		PipelineArray.push_back(current_pipeline);
		LayoutArray.push_back(current_layout);

		vkcorePipeline pipe_out;
		pipe_out.pipeline = current_pipeline;
		pipe_out.layout = current_layout;

		return pipe_out;
	}

	vkcorePipeline PipelineCache::GetComputePipeline(VkPipelineShaderStageCreateInfo moduleinfo, VkDescriptorSetLayout* layouts, uint32_t layouts_size, std::vector<VkPushConstantRange>& ranges)
	{
		VkPipeline current_pipeline;
		VkPipelineLayout current_layout;

		std::vector<VkDescriptorSetLayout> current_layouts;
		for (int i = 0; i < layouts_size; i++) {
			VkDescriptorSetLayout* l = layouts + i;
			current_layouts.push_back(*l);
		}

		VkPipelineLayoutCreateInfo layoutinfo = {};
		layoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutinfo.setLayoutCount = layouts_size;
		layoutinfo.pSetLayouts = layouts;
		layoutinfo.pushConstantRangeCount = ranges.size();
		if (ranges.size() != 0)
		{
			layoutinfo.pPushConstantRanges = ranges.data();
		}

		VULKAN_CHECK(vkCreatePipelineLayout(deviceref, &layoutinfo, nullptr, &current_layout), "creating pipeline layout");

		VkComputePipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		info.flags = 0;
		info.stage = moduleinfo;
		info.layout = current_layout;

		VULKAN_CHECK(vkCreateComputePipelines(deviceref, VK_NULL_HANDLE, 1, &info, nullptr, &current_pipeline), "create compute pipeline");

		//store pipeline and layout is arrays
		PipelineArray.push_back(current_pipeline);
		LayoutArray.push_back(current_layout);

		vkcorePipeline pipe_out;
		pipe_out.pipeline = current_pipeline;
		pipe_out.layout = current_layout;

		return pipe_out;
	}
}