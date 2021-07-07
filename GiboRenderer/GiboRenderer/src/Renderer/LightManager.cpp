#include "../pch.h"
#include "LightManager.h"
#include "vkcore/VulkanHelpers.h"

namespace Gibo {

	void LightManager::CreateBuffers(int framesinflight)
	{
		light_buffer.resize(framesinflight);
		for (int i = 0; i < framesinflight; i++)
		{
			deviceref.CreateBuffer(sizeof(Light::lightparams) * MAX_LIGHTS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, light_buffer[i]);
		}

		lightcounter_buffer.resize(framesinflight);
		int data_size = 0;
		for (int i = 0; i < framesinflight; i++)
		{
			deviceref.CreateBufferStaged(sizeof(int), &data_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, lightcounter_buffer[i],
										VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}

		deviceref.CreateBuffer(sizeof(Light::lightparams) * MAX_LIGHTS, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, light_stagingbuffer);
		deviceref.CreateBuffer(sizeof(int), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, lightcounter_stagingbuffer);
	}

	void LightManager::CleanUp()
	{
		for (int i = 0; i < light_buffer.size(); i++)
		{
			deviceref.DestroyBuffer(light_buffer[i]);
		}
		for (int i = 0; i < lightcounter_buffer.size(); i++)
		{
			deviceref.DestroyBuffer(lightcounter_buffer[i]);
		}

		deviceref.DestroyBuffer(light_stagingbuffer);
		deviceref.DestroyBuffer(lightcounter_stagingbuffer);
	}

	void LightManager::UpdateBuffers(int framecount)
	{
		//we have a bunch of pointers on the map. Go through the map and create a contiguous array of memory from the pointers. Order doesn't matter for lights.
		void* mappedData;
		VULKAN_CHECK(vmaMapMemory(deviceref.GetAllocator(), light_stagingbuffer.allocation, &mappedData), "mapping memory");
		int counter = 0;
		for (std::unordered_map<int, Light::lightparams*>::iterator it = light_map.begin(); it != light_map.end(); ++it)
		{
			Light::lightparams* ptr = it->second;
			memcpy(static_cast<Light::lightparams*>(mappedData) + counter, ptr, sizeof(Light::lightparams));
			counter++;
		}
		vmaUnmapMemory(deviceref.GetAllocator(), light_stagingbuffer.allocation);

		CopyBufferToBuffer(deviceref, light_stagingbuffer.buffer, light_buffer[framecount].buffer, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, sizeof(Light::lightparams) * MAX_LIGHTS);

		int lightcount = light_map.size();
		deviceref.BindData(lightcounter_stagingbuffer.allocation, &lightcount, sizeof(int));
		CopyBufferToBuffer(deviceref, lightcounter_stagingbuffer.buffer, lightcounter_buffer[framecount].buffer, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, sizeof(int));
	}

	void LightManager::Update(int framecount)
	{
		if (needs_updated)
		{
			UpdateBuffers(framecount);
			
			frames_updated++;
			if (frames_updated >= light_buffer.size())
			{
				frames_updated = 0;
				needs_updated = false;
			}
		}
	}

	void LightManager::AddLight(Light& light)
	{
#ifdef _DEBUG
		if (light.lightmanager_id != -1)
		{
			Logger::LogError("Light is already in manager error\n");
		}
#endif
		light.lightmanager_id = id_count;
		light_map[id_count] = light.getParamsPtr();
#ifdef _DEBUG
		if (light_map.size() > MAX_LIGHTS)
		{
			Logger::LogError("You have more lights than allowable. Change MAX_LIGHTS count");
		}
#endif
		id_count++;
		NotifyUpdate();
	}

	void LightManager::RemoveLight(Light& light)
	{
		if (light_map.count(light.lightmanager_id) != 0)
		{
			light_map.erase(light.lightmanager_id);
		}
		light.lightmanager_id = -1;
		NotifyUpdate();
	}

}