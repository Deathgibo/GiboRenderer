#pragma once
#include "vkcoreDevice.h"

namespace Gibo {

	/*
	This class is meant for creating and encapsulating all things that each shader needs for itself: shader modules, VkPipelineShaderStageCreateInfo, descriptors, descriptor layout, descriptor pool
	The important thing is that this class handles the descriptors for each shader. It has 2 sets of descriptors: global and local. It also holds a descriptor for each frame in flight.
	On creation it creates the descriptor layouts and the pool, and you need to specify the max number of descriptors the pool can allocate. 
	On creation it also creates the global descriptor.

	Besides this you can add and remove local descriptors and set the global descriptor. On creation it will give you an id that you need to store and return once you want it deleted.
	The descriptors get allocated framesperflight times per creation.

	Memory managment is all handled by this class as well

	Todo - make passing in double vector less messy?
	*/

	class ShaderProgram
	{
	public:
		struct shadersinfo
		{
			std::string name;
			VkShaderStageFlagBits stage;

			shadersinfo();
			shadersinfo(std::string name_, VkShaderStageFlagBits stage_) : name(name_), stage(stage_) {}
		};

		struct descriptorinfo
		{
			std::string name;
			uint32_t binding;
			VkDescriptorType type;
			VkShaderStageFlags stageflags;
			VkImageLayout imagelayout = VK_IMAGE_LAYOUT_UNDEFINED;
			int descriptorcount = 1;
			bool perinstance = false;

			descriptorinfo();
			descriptorinfo(std::string name_, uint32_t binding_, VkDescriptorType type_, VkShaderStageFlags stageflags_, VkImageLayout imagelayout_ = VK_IMAGE_LAYOUT_UNDEFINED) : name(name_),
				binding(binding_), type(type_), stageflags(stageflags_), imagelayout(imagelayout_) {}
		};

		struct pushconstantinfo 
		{
			VkShaderStageFlags shaderstage;
			uint32_t offset;   //must be multiple of 4
			uint32_t size;     //must be multiple of 4
		};

	public:
		ShaderProgram() = default;
		~ShaderProgram() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		ShaderProgram(ShaderProgram const&) = delete;
		ShaderProgram(ShaderProgram&&) = delete;
		ShaderProgram& operator=(ShaderProgram const&) = delete;
		ShaderProgram& operator=(ShaderProgram&&) = delete;

		bool Create(VkDevice device, uint32_t framesinflight, shadersinfo* shaderinformation, uint32_t shaderinfo_count, descriptorinfo* globaldescriptors, uint32_t global_count, 
			        descriptorinfo* localdescriptors, uint32_t local_count, pushconstantinfo* pushinfo, uint32_t push_count, uint32_t maxlocaldescriptorsallowed, uint32_t maxglobaldescriptorallowed = 1);
		void CleanUp();

		void SetGlobalDescriptor(std::vector<std::vector<vkcoreBuffer>>& uniformbuffers, std::vector<std::vector<uint64_t>>& buffersizes, std::vector<std::vector<VkImageView>>& imageviews,
								 std::vector<std::vector<VkSampler>>& samplers, std::vector<std::vector<VkBufferView>>& bufferviews);
		void SetSpecificGlobalDescriptor(int current_frame, std::vector<vkcoreBuffer>& uniformbuffers, std::vector<uint64_t>& buffersizes, std::vector<VkImageView>& imageviews,
			std::vector<VkSampler>& samplers, std::vector<VkBufferView>& bufferviews);

		void AddLocalDescriptor(uint32_t descriptor_id, std::vector<std::vector<vkcoreBuffer>>& uniformbuffers, std::vector<std::vector<uint64_t>>& buffersizes,
			               std::vector<std::vector<VkImageView>>& imageviews, std::vector<std::vector<VkSampler>>& samplers, std::vector<std::vector<VkBufferView>>& bufferviews);
		void RemoveLocalDescriptor(uint32_t descriptor_id);

		VkDescriptorSetLayout& GetLocalLayout() { return LocalLayout; }
		VkDescriptorSetLayout& GetGlobalLayout() { return GlobalLayout; }
		VkDescriptorPool GetGlobalPool() { return GlobalPool; }
		std::vector<descriptorinfo> GetGlobalDescriptorInfo() { return GlobalDescriptorInfo; }
		std::vector<VkShaderModule> GetShaderModules() { return ShaderModules; }
		std::vector<VkPipelineShaderStageCreateInfo> GetShaderStageInfo() { return ShaderStageInfo; }
		VkDescriptorSet& GetGlobalDescriptor(int frameinflight) { return GlobalSet[frameinflight]; }
		VkDescriptorSet& GetLocalDescriptor(uint32_t descriptor_id, int frameinflight) { return DescriptorSets[descriptor_id][frameinflight]; }
		int GetLocalDescriptorSize() { return DescriptorSets.size(); }
		std::vector<VkPushConstantRange>& GetPushRanges() { return Push_ConstantRanges; }

		void UpdateDescriptorSet(VkDescriptorSet descriptorset, const std::vector<descriptorinfo>& descriptorinfo, vkcoreBuffer* uniformbuffers, uint64_t* buffersizes, VkImageView* imageviews, VkSampler* samplers, VkBufferView* bufferviews);
		bool AllocateSets(VkDescriptorSet* sets, uint32_t sets_size, VkDescriptorPool pool, VkDescriptorSetLayout layout);
	private:
		std::vector<char> readFile(const std::string& filename) const;
	private:
		//descriptors
		std::unordered_map<uint32_t, std::vector<VkDescriptorSet>> DescriptorSets; //an id maps to a descriptor set which has one for each frame in flight
		std::vector<VkDescriptorSet> GlobalSet;
		std::vector<descriptorinfo> LocalDescriptorInfo;
		std::vector<descriptorinfo> GlobalDescriptorInfo;
		VkDescriptorPool GlobalPool;
		VkDescriptorPool LocalPool;
		VkDescriptorSetLayout LocalLayout;
		VkDescriptorSetLayout GlobalLayout;
		std::vector<VkPushConstantRange> Push_ConstantRanges;

		//pipeline information
		std::vector<VkShaderModule> ShaderModules;
		std::vector<VkPipelineShaderStageCreateInfo> ShaderStageInfo;
			
		//other
		VkDevice deviceref;
		uint32_t mframesinflight;
	};

}

