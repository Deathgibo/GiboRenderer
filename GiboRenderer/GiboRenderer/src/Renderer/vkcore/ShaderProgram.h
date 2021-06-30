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

	struct shadersinfo 
	{
		std::string name;
		VkShaderStageFlagBits stage;
	};

	struct descriptorinfo
	{
		std::string name;
		uint32_t binding;
		VkDescriptorType type;
		VkShaderStageFlags stageflags;
		VkImageLayout imagelayout;
		int descriptorcount = 1;
		bool perinstance;
	};

	//just a quick way to hand out id's. counter will increment and repeat at IDHELPERSIZE and the slots just represent if that id is taken or not
	const int IDHELPERSIZE = 1000;
	struct idhelper 
	{
		std::array<bool, IDHELPERSIZE>slots;
		uint32_t counter;

		idhelper()
		{
			slots.fill(false);
			counter = 0;
		}

		uint32_t GetNextID()//assumes every slot will never all be true
		{
			while (slots[counter] == true)
			{
				counter = (counter + 1) % IDHELPERSIZE;
			}
			uint32_t returncounter = counter;
			slots[returncounter] = true;
			counter = (counter + 1) % IDHELPERSIZE;
			return returncounter;
		}

		void FreeID(uint32_t id)
		{
			slots[id] = false;
		}
	};

	class ShaderProgram
	{
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
			        descriptorinfo* localdescriptors, uint32_t local_count, uint32_t maxlocaldescriptorsallowed);
		void CleanUp();

		void SetGlobalDescriptor(std::vector<std::vector<vkcoreBuffer>>& uniformbuffers, std::vector<std::vector<uint64_t>>& buffersizes, std::vector<std::vector<VkImageView>>& imageviews,
								 std::vector<std::vector<VkSampler>>& samplers, std::vector<std::vector<VkBufferView>>& bufferviews);
		void AddLocalDescriptor(uint32_t& id, std::vector<std::vector<vkcoreBuffer>>& uniformbuffers, std::vector<std::vector<uint64_t>>& buffersizes,
			               std::vector<std::vector<VkImageView>>& imageviews, std::vector<std::vector<VkSampler>>& samplers, std::vector<std::vector<VkBufferView>>& bufferviews);
		void RemoveLocalDescriptor(uint32_t id);

	private:
		std::vector<char> readFile(const std::string& filename) const;
		void UpdateDescriptorSet(VkDescriptorSet descriptorset, const std::vector<descriptorinfo>& descriptorinfo, vkcoreBuffer* uniformbuffers, uint64_t* buffersizes, VkImageView* imageviews, VkSampler* samplers, VkBufferView* bufferviews);
		bool AllocateSets(VkDescriptorSet* sets, uint32_t sets_size, VkDescriptorPool pool, VkDescriptorSetLayout layout);
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

		//pipeline information
		std::vector<VkShaderModule> ShaderModules;
		std::vector<VkPipelineShaderStageCreateInfo> ShaderStageInfo;
			
		//other
		idhelper id_handler;
		VkDevice deviceref;
		uint32_t mframesinflight;
	};

}

