#include "../../pch.h"
#include "ShaderProgram.h"
#include <fstream>

namespace Gibo {

	void ShaderProgram::CleanUp()
	{
		for (int i = 0; i < ShaderModules.size(); i++)
		{
			vkDestroyShaderModule(deviceref, ShaderModules[i], nullptr);
		}

		//free descriptors local and global descriptor sets
		vkFreeDescriptorSets(deviceref, GlobalPool, GlobalSet.size(), GlobalSet.data());
		for (auto& sets : DescriptorSets)
		{
			vkFreeDescriptorSets(deviceref, LocalPool, sets.second.size(), sets.second.data());
		}
		DescriptorSets.clear();
		GlobalSet.clear();

		vkDestroyDescriptorPool(deviceref, GlobalPool, nullptr);
		vkDestroyDescriptorPool(deviceref, LocalPool, nullptr);

		vkDestroyDescriptorSetLayout(deviceref, GlobalLayout, nullptr);
		vkDestroyDescriptorSetLayout(deviceref, LocalLayout, nullptr);
	}

	bool ShaderProgram::Create(VkDevice device, uint32_t framesinflight, shadersinfo* shaderinformation, uint32_t shaderinfo_count, descriptorinfo* globaldescriptors, uint32_t global_count,
			    descriptorinfo* localdescriptors, uint32_t local_count, pushconstantinfo* pushinfo, uint32_t push_count, uint32_t maxlocaldescriptorsallowed, uint32_t maxglobaldescriptorallowed)
	{
		Logger::LogInfo("Creating shader program: ", shaderinformation->name, "\n");
		deviceref = device;
		mframesinflight = framesinflight;
		bool is_valid = true;
		VkResult result;

		//create shader modules and VkPipelineShaderStageCreateInfo
		for (int i = 0; i < shaderinfo_count; i++)
		{
			shadersinfo* shader = shaderinformation + i;

			VkShaderModule smodule;
			auto ShaderCode = readFile(shader->name);
			VkShaderModuleCreateInfo shaderinfo{};
			shaderinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderinfo.pNext = nullptr;
			shaderinfo.flags = 0;
			shaderinfo.codeSize = ShaderCode.size();
			shaderinfo.pCode = reinterpret_cast<const uint32_t*>(ShaderCode.data());
			result = vkCreateShaderModule(device, &shaderinfo, nullptr, &smodule);
			VULKAN_CHECK(result, "create shader module");
			if (result != VK_SUCCESS)
			{
				is_valid = false;
			}
			ShaderModules.push_back(smodule);

			VkPipelineShaderStageCreateInfo vertexinfo = {};
			vertexinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertexinfo.stage = shader->stage;
			vertexinfo.module = smodule;
			vertexinfo.pName = "main";
			ShaderStageInfo.push_back(vertexinfo);
		}

		//create descriptor layouts for global and local
		std::vector<VkDescriptorSetLayoutBinding> global_bindings;
		for (int i = 0; i < global_count; i++)
		{
			descriptorinfo* ginfo = globaldescriptors + i;
			GlobalDescriptorInfo.push_back(*ginfo);

			VkDescriptorSetLayoutBinding bindinginfo = {};
			bindinginfo.binding = ginfo->binding;
			bindinginfo.descriptorCount = ginfo->descriptorcount;
			bindinginfo.descriptorType = ginfo->type;
			bindinginfo.pImmutableSamplers = nullptr;
			bindinginfo.stageFlags = ginfo->stageflags;
			global_bindings.push_back(bindinginfo);
		}
		VkDescriptorSetLayoutCreateInfo globallayoutInfo = {};
		globallayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		globallayoutInfo.bindingCount = static_cast<uint32_t>(global_bindings.size());
		globallayoutInfo.pBindings = global_bindings.data();
		result = vkCreateDescriptorSetLayout(device, &globallayoutInfo, nullptr, &GlobalLayout);
		VULKAN_CHECK(result, "create global descriptor layout");
		if (result != VK_SUCCESS)
		{
			is_valid = false;
		}

		std::vector<VkDescriptorSetLayoutBinding>local_bindings;
		for (int i = 0; i < local_count; i++)
		{
			descriptorinfo* ginfo = localdescriptors + i;
			LocalDescriptorInfo.push_back(*ginfo);

			VkDescriptorSetLayoutBinding bindinginfo = {};
			bindinginfo.binding = ginfo->binding;
			bindinginfo.descriptorCount = ginfo->descriptorcount;
			bindinginfo.descriptorType = ginfo->type;
			bindinginfo.pImmutableSamplers = nullptr;
			bindinginfo.stageFlags = ginfo->stageflags;
			local_bindings.push_back(bindinginfo);
		}
		VkDescriptorSetLayoutCreateInfo locallayoutInfo = {};
		locallayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		locallayoutInfo.bindingCount = static_cast<uint32_t>(local_bindings.size());
		locallayoutInfo.pBindings = local_bindings.data();
		result = vkCreateDescriptorSetLayout(device, &locallayoutInfo, nullptr, &LocalLayout);
		VULKAN_CHECK(result, "create local descriptor layout");
		if (result != VK_SUCCESS)
		{
			is_valid = false;
		}
		
		//when creating pool you just give the block size and how many blocks you want. Then you can allocate and deallocate which doesn't actually allocate any memory.
		//if you need to you can flush out the whole pool 
		//A simple way if you run out of space, you could hold an array of these and create a new pool when you need too.
		uint32_t localpool_maxSets = (maxlocaldescriptorsallowed * framesinflight) + 1;//maximum number of objects we can allocate for this shader
		uint32_t globalpool_maxsets = (maxglobaldescriptorallowed * framesinflight) + 1;//global just needs 1 per frame in flight, but you can add more if you want

		std::vector<VkDescriptorPoolSize> GlobalpoolSizes;
		for (int i = 0; i < global_count; i++)
		{
			descriptorinfo* ginfo = globaldescriptors + i;
			VkDescriptorPoolSize poolinfo = {};
			poolinfo.descriptorCount = globalpool_maxsets; //is the number of descriptors of that type to allocate
			poolinfo.type = ginfo->type;
			GlobalpoolSizes.push_back(poolinfo);
		}
		//if we don't have global descriptor just create a dummy one
		if (global_count == 0)
		{
			descriptorinfo ginfo("dummy", 9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
			VkDescriptorPoolSize poolinfo = {};
			poolinfo.descriptorCount = 1; //is the number of descriptors of that type to allocate
			poolinfo.type = ginfo.type;
			GlobalpoolSizes.push_back(poolinfo);
		}
		VkDescriptorPoolCreateInfo GlobalpoolInfo = {};
		GlobalpoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		GlobalpoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		GlobalpoolInfo.poolSizeCount = static_cast<uint32_t>(GlobalpoolSizes.size());
		GlobalpoolInfo.pPoolSizes = GlobalpoolSizes.data();
		GlobalpoolInfo.maxSets = globalpool_maxsets;
		result = vkCreateDescriptorPool(device, &GlobalpoolInfo, nullptr, &GlobalPool);
		VULKAN_CHECK(result, "creating global descriptor pool");
		if (result != VK_SUCCESS)
		{
			is_valid = false;
		}
		std::vector<VkDescriptorPoolSize> LocalpoolSizes;
		for (int i = 0; i < local_count; i++)
		{
			descriptorinfo* ginfo = localdescriptors + i;
			VkDescriptorPoolSize poolinfo = {};
			poolinfo.descriptorCount = localpool_maxSets; ////is the number of descriptors of that type to allocate
			poolinfo.type = ginfo->type;
			LocalpoolSizes.push_back(poolinfo);
		}
		//if we don't have local descriptor just create a dummy one
		if (local_count == 0)
		{
			descriptorinfo ginfo("dummy", 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
			VkDescriptorPoolSize poolinfo = {};
			poolinfo.descriptorCount = 1; //is the number of descriptors of that type to allocate
			poolinfo.type = ginfo.type;
			LocalpoolSizes.push_back(poolinfo);
		}
		VkDescriptorPoolCreateInfo LocalpoolInfo = {};
		LocalpoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		LocalpoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		LocalpoolInfo.poolSizeCount = static_cast<uint32_t>(LocalpoolSizes.size());
		LocalpoolInfo.pPoolSizes = LocalpoolSizes.data();
		LocalpoolInfo.maxSets = localpool_maxSets;
		result = vkCreateDescriptorPool(device, &LocalpoolInfo, nullptr, &LocalPool);
		VULKAN_CHECK(result, "creating local descriptor pool");
		if (result != VK_SUCCESS)
		{
			is_valid = false;
		}

		//allocate global_pool because we know we need it and its going to be 1 set only per frame in flight
		GlobalSet.resize(framesinflight);
		bool mresult = AllocateSets(GlobalSet.data(), GlobalSet.size(), GlobalPool, GlobalLayout);
		if (mresult != true)
		{
			is_valid = false;
		}

		uint32_t total_pcsize = 0;
		for (int i = 0; i < push_count; i++)
		{
			pushconstantinfo* info = pushinfo + i;

			VkPushConstantRange range = {};
			range.stageFlags = info->shaderstage;
			range.offset = info->offset;//must be multiple of 4
			range.size = info->size;//must be multiple of 4

			Push_ConstantRanges.push_back(range);

			total_pcsize += info->size;
			if ((info->offset % 4) != 0)
			{
				Logger::LogError("Push constant offset needs to be a multiple of 4\n");
			}
			if ((info->size % 4) != 0)
			{
				Logger::LogError("Push constant size needs to be a multiple of 4\n");
			}
		}
		if (total_pcsize > 128)
		{
			Logger::LogError("Push constant size greater than 128 bytes\n");
		}

		return is_valid;
	}
	
	void ShaderProgram::SetGlobalDescriptor(std::vector<std::vector<vkcoreBuffer>>& uniformbuffers, std::vector<std::vector<uint64_t>>& buffersizes, std::vector<std::vector<VkImageView>>& imageviews,
											std::vector<std::vector<VkSampler>>& samplers, std::vector<std::vector<VkBufferView>>& bufferviews)
	{
		//we already allocated the memory for the global set per frame in flight just update them
		for (int i = 0; i < GlobalSet.size(); i++)
		{
			if (uniformbuffers[i].size() + imageviews[i].size() != GlobalDescriptorInfo.size())
			{
				Logger::LogWarning("global descriptor arrays don't match size of programs descriptorlayout\n");
			}
			UpdateDescriptorSet(GlobalSet[i], GlobalDescriptorInfo, uniformbuffers[i].data(), buffersizes[i].data(), imageviews[i].data(), samplers[i].data(), bufferviews[i].data());
		}
	}

	void ShaderProgram::SetSpecificGlobalDescriptor(int current_frame, std::vector<vkcoreBuffer>& uniformbuffers, std::vector<uint64_t>& buffersizes, std::vector<VkImageView>& imageviews,
		std::vector<VkSampler>& samplers, std::vector<VkBufferView>& bufferviews)
	{
		if (uniformbuffers.size() + imageviews.size() != GlobalDescriptorInfo.size())
		{
			Logger::LogWarning("global descriptor arrays don't match size of programs descriptorlayout\n");
		}
		UpdateDescriptorSet(GlobalSet[current_frame], GlobalDescriptorInfo, uniformbuffers.data(), buffersizes.data(), imageviews.data(), samplers.data(), bufferviews.data());
	}

	void ShaderProgram::AddLocalDescriptor(uint32_t descriptor_id, std::vector<std::vector<vkcoreBuffer>>& uniformbuffers, std::vector<std::vector<uint64_t>>& buffersizes,
		                               std::vector<std::vector<VkImageView>>& imageviews, std::vector<std::vector<VkSampler>>& samplers, std::vector<std::vector<VkBufferView>>& bufferviews)
	{
#ifdef _DEBUG
		if (DescriptorSets.count(descriptor_id) != 0)
		{
			Logger::LogError("adding local descriptor with id that already exists\n");
		}
#endif
		//create and allocate n descriptor sets, store them in our data structure
		std::vector<VkDescriptorSet>& sets = DescriptorSets[descriptor_id];
		sets.resize(mframesinflight);
		AllocateSets(sets.data(), sets.size(), LocalPool, LocalLayout);
		for (int i = 0; i < sets.size(); i++)
		{
			if (uniformbuffers[i].size() + imageviews[i].size() != LocalDescriptorInfo.size())
			{
				Logger::LogWarning("local descriptor arrays don't match size of programs descriptorlayout\n");
			}
			UpdateDescriptorSet(sets[i], LocalDescriptorInfo, uniformbuffers[i].data(), buffersizes[i].data(), imageviews[i].data(), samplers[i].data(), bufferviews[i].data());
		}
	}
	
	void ShaderProgram::RemoveLocalDescriptor(uint32_t descriptor_id)
	{
#ifdef _DEBUG
		if (DescriptorSets.count(descriptor_id) == 0)
		{
			Logger::LogError("Removing local descriptor that doesn't exist\n");
		}
#endif
		//free descriptors from the pool, remove from map, and free id
		std::vector<VkDescriptorSet>& set = DescriptorSets[descriptor_id];
		
		for (int i = 0; i < set.size(); i++)
		{
			vkFreeDescriptorSets(deviceref, LocalPool, 1, &set[i]);
		}
		//vkFreeDescriptorSets(deviceref, LocalPool, set.size(), set.data());

		DescriptorSets.erase(descriptor_id);
	}

	//creates n local descriptor sets for each frame in flight for this shader program. Uses the local Pool to allocate n.
	bool ShaderProgram::AllocateSets(VkDescriptorSet* sets, uint32_t sets_size, VkDescriptorPool pool, VkDescriptorSetLayout layout)
	{
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;

		bool valid = true;
		for (int i = 0; i < sets_size; i++)
		{
			VkResult result = vkAllocateDescriptorSets(deviceref, &allocInfo, sets + i);
			VULKAN_CHECK(result, "allocating descriptor sets");
			if (result != VK_SUCCESS)
			{
				valid = false;
			}
		}

		return valid;
	}

	//*this expects the data to be in the same order as the descriptor order you gave the shader on creation based on type. images and views sorted, buffers and sizes sorted.
	void ShaderProgram::UpdateDescriptorSet(VkDescriptorSet descriptorset, const std::vector<descriptorinfo>& descriptorinfo, vkcoreBuffer* uniformbuffers, uint64_t* buffersizes, VkImageView* imageviews, VkSampler* samplers, VkBufferView* bufferviews)
	{
		std::vector<VkWriteDescriptorSet> descriptorWrites;
		descriptorWrites.reserve(descriptorinfo.size());
		std::vector<VkDescriptorBufferInfo> bufferinfos(descriptorinfo.size());
		std::vector<VkDescriptorImageInfo> imageinfos(descriptorinfo.size());
		int buffercounter = 0;
		int imagecounter = 0;
		int bufferviewcount = 0;
		for (int i = 0; i < descriptorinfo.size(); i++)
		{
			if (descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_SAMPLER)
			{
				imageinfos[imagecounter].sampler = samplers[imagecounter];

				VkWriteDescriptorSet info = {};
				info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				info.dstSet = descriptorset;
				info.dstBinding = descriptorinfo[i].binding;
				info.dstArrayElement = 0;
				info.descriptorType = descriptorinfo[i].type;
				info.descriptorCount = descriptorinfo[i].descriptorcount;
				info.pImageInfo = &imageinfos[imagecounter];

				descriptorWrites.push_back(info);
				imagecounter++;
			}
			else if (descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			{
				imageinfos[imagecounter].imageLayout = descriptorinfo[i].imagelayout;// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageinfos[imagecounter].imageView = imageviews[imagecounter];

				VkWriteDescriptorSet info = {};
				info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				info.dstSet = descriptorset;
				info.dstBinding = descriptorinfo[i].binding;
				info.dstArrayElement = 0;
				info.descriptorType = descriptorinfo[i].type;
				info.descriptorCount = descriptorinfo[i].descriptorcount;
				info.pImageInfo = &imageinfos[imagecounter];

				descriptorWrites.push_back(info);
				imagecounter++;
			}
			else if (descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
				descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
			{
				bufferinfos[buffercounter].buffer = uniformbuffers[buffercounter].buffer;
				bufferinfos[buffercounter].offset = 0; //TODO - might have to change this
				bufferinfos[buffercounter].range = buffersizes[buffercounter];


				VkWriteDescriptorSet info = {};
				info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				info.dstSet = descriptorset;
				info.dstBinding = descriptorinfo[i].binding;
				info.dstArrayElement = 0;
				info.descriptorType = descriptorinfo[i].type;
				info.descriptorCount = descriptorinfo[i].descriptorcount;
				info.pBufferInfo = &bufferinfos[buffercounter];


				descriptorWrites.push_back(info);
				buffercounter++;
			}
			else if (descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER || descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
			{
				VkWriteDescriptorSet info = {};
				info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				info.dstSet = descriptorset;
				info.dstBinding = descriptorinfo[i].binding;
				info.dstArrayElement = 0;
				info.descriptorType = descriptorinfo[i].type;
				info.descriptorCount = descriptorinfo[i].descriptorcount;
				info.pTexelBufferView = &bufferviews[bufferviewcount];

				descriptorWrites.push_back(info);
				bufferviewcount++;
			}
			else if (descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || descriptorinfo[i].type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
				imageinfos[imagecounter].imageLayout = descriptorinfo[i].imagelayout;// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageinfos[imagecounter].imageView = imageviews[imagecounter];
				imageinfos[imagecounter].sampler = samplers[imagecounter];

				VkWriteDescriptorSet info = {};
				info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				info.dstSet = descriptorset;
				info.dstBinding = descriptorinfo[i].binding;
				info.dstArrayElement = 0;
				info.descriptorType = descriptorinfo[i].type;
				info.descriptorCount = descriptorinfo[i].descriptorcount;
				info.pImageInfo = &imageinfos[imagecounter];

				descriptorWrites.push_back(info);
				imagecounter++;
			}
			else {
				Logger::LogError("descriptor type wrong");
			}
		}

		vkUpdateDescriptorSets(deviceref, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	std::vector<char> ShaderProgram::readFile(const std::string& filename) const
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			Logger::LogError("failed to open shader file: ", filename, '\n');
		}
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}
}