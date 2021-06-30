#pragma once
#include "../../pch.h"

namespace Gibo {
	/*
		All pipelines in this engine right now are just ahead of time compilation. I should know every pipeline I need and create it at the start of the program
		This cache handles building graphics/compute pipelines and storing them in just a vector. It also has a nice interfact of structs to pass in information you want.
		It also handles all the memory management.

		This is also where the shader vertex attributes are defined which every shader needs to follow for now (in the cpp file)
		dynamic states are not supported for pipelines right now
		todo - research more about pipeline caches to reduce start_time
	*/

	struct RasterizationState {
		float linewidth = 1.0;
		VkPolygonMode polygonmode = VK_POLYGON_MODE_FILL;
		VkCullModeFlags cullmode = VK_CULL_MODE_NONE;
		VkFrontFace frontface = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	};
	struct MultisamplingState {
		float minsampleshading = 0.0;
		VkBool32 sampleshadingenable = false;
		VkSampleCountFlagBits samplecount = VK_SAMPLE_COUNT_1_BIT;
	};
	struct DepthStencilState {
		VkCompareOp depthcompareop = VK_COMPARE_OP_LESS;
		VkBool32 depthtestenable = VK_TRUE;
		VkBool32 depthwriteenable = VK_TRUE;
		VkBool32 stenciltestenable = VK_FALSE;
	};
	struct ColorBlendState {
		float blendconstant0 = 0.0;
		float blendconstant1 = 0.0;
		float blendconstant2 = 0.0;
		float blendconstant3 = 0.0;
		VkBlendOp colorblendop = VK_BLEND_OP_ADD;
		VkBlendOp alphablendop = VK_BLEND_OP_ADD;
		VkBlendFactor dstblendfactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		VkBlendFactor srcblendfactor = VK_BLEND_FACTOR_SRC_ALPHA;
		VkBlendFactor dstalphablendfactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		VkBlendFactor srcalphablendfactor = VK_BLEND_FACTOR_SRC_ALPHA;
		VkBool32 blendenable = VK_FALSE;
	};
	struct ViewPortState {
		VkViewport viewport;
		VkRect2D scissor;
	};
	struct InputAssembly {
		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	};
	struct PushConstants {
		std::vector<VkShaderStageFlags> shaderstage;
		std::vector<uint32_t> offset;   //must be multiple of 4
		std::vector<uint32_t> size;     //must be multiple of 4
	};
	struct PipelineData
	{
		RasterizationState Rasterizationstate;
		MultisamplingState Multisamplingstate;
		DepthStencilState DepthStencilstate;
		ColorBlendState ColorBlendstate;
		ViewPortState ViewPortstate;
		InputAssembly Inputassembly;
		PushConstants Pushconstants;

		PipelineData(float swapchainwidth, float swapchainheight)
		{
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = swapchainwidth;
			viewport.height = swapchainheight;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = { (uint32_t)swapchainwidth, (uint32_t)swapchainheight };

			ViewPortstate.viewport = viewport;
			ViewPortstate.scissor = scissor;
		}
	};

	class PipelineCache
	{ 
	public:
		PipelineCache(VkDevice device) : deviceref(device), PipelineArray(8), LayoutArray(8) {  };
		~PipelineCache() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		PipelineCache(PipelineCache const&) = delete;
		PipelineCache(PipelineCache&&) = delete;
		PipelineCache& operator=(PipelineCache const&) = delete;
		PipelineCache& operator=(PipelineCache&&) = delete;

		void Cleanup();
		void PrintDebug() const;

		VkPipeline GetGraphicsPipeline(PipelineData data, VkRenderPass renderpass, const std::vector<VkPipelineShaderStageCreateInfo>& moduleinfo, VkDescriptorSetLayout* layouts, uint32_t layouts_size);
		VkPipeline GetComputePipeline(VkPipelineShaderStageCreateInfo moduleinfo, VkDescriptorSetLayout* layouts, uint32_t layouts_size);
	private:
		std::vector<VkPipeline> PipelineArray;
		std::vector<VkPipelineLayout> LayoutArray;
		VkPipelineCache pipelinecache;
		VkDevice deviceref;
	};


}
