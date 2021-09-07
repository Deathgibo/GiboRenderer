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

		light_stagingbuffer.resize(framesinflight);
		lightcounter_stagingbuffer.resize(framesinflight);
		for (int i = 0; i < framesinflight; i++)
		{
			deviceref.CreateBuffer(sizeof(Light::lightparams) * MAX_LIGHTS, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, light_stagingbuffer[i]);
			deviceref.CreateBuffer(sizeof(int), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, lightcounter_stagingbuffer[i]);
		}

		gpumemory_usage = sizeof(Light::lightparams) * MAX_LIGHTS * framesinflight + sizeof(int) * framesinflight + sizeof(Light::lightparams) * MAX_LIGHTS + sizeof(int);
	}

	void LightManager::PrintInfo()
	{
		Logger::Log("-----Light Manager-----\n", "number of current lights: ", light_map.size(), " GPU memory usage: ", gpumemory_usage, "\n");
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
		for (int i = 0; i < light_stagingbuffer.size(); i++)
		{
			deviceref.DestroyBuffer(light_stagingbuffer[i]);
			deviceref.DestroyBuffer(lightcounter_stagingbuffer[i]);
		}
	}

	void LightManager::UpdateBuffers(int framecount)
	{
		//we have a bunch of pointers on the map. Go through the map and create a contiguous array of memory from the pointers. Order doesn't matter for lights.
		void* mappedData;
		VULKAN_CHECK(vmaMapMemory(deviceref.GetAllocator(), light_stagingbuffer[framecount].allocation, &mappedData), "mapping memory");
		int counter = 0;
		for (std::unordered_map<int, Light::lightparams*>::iterator it = light_map.begin(); it != light_map.end(); ++it)
		{
			Light::lightparams* ptr = it->second;
			memcpy(static_cast<Light::lightparams*>(mappedData) + counter, ptr, sizeof(Light::lightparams));
			counter++;
		}
		vmaUnmapMemory(deviceref.GetAllocator(), light_stagingbuffer[framecount].allocation);

		CopyBufferToBuffer(deviceref, light_stagingbuffer[framecount].buffer, light_buffer[framecount].buffer, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, sizeof(Light::lightparams) * counter);//MAX_LIGHTS

		int lightcount = light_map.size();
		deviceref.BindData(lightcounter_stagingbuffer[framecount].allocation, &lightcount, sizeof(int));
		CopyBufferToBuffer(deviceref, lightcounter_stagingbuffer[framecount].buffer, lightcounter_buffer[framecount].buffer, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, sizeof(int));
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

		if (shadow_casts_changed)
		{
			shadow_casts_changed = false;
		}

		shadow_casts_changed = true;
	}

	void LightManager::SetShadowCaster(Light& light, bool cast)
	{
		if (cast)
		{
			shadow_casts.push_back(light.lightmanager_id);
			shadow_casts_changed = true;

			light.getParamsPtr()->cast_shadow = 1;
		}
		else
		{
			shadow_casts_changed = true;
			for (int i = 0; i < shadow_casts.size(); i++)
			{
				if (shadow_casts[i] == light.lightmanager_id)
				{
					shadow_casts.erase(shadow_casts.begin() + i);
				}
			}
			light.getParamsPtr()->cast_shadow = 0;
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

		//see if we want it to cast shadows
		if (light.getParams().cast_shadow == 1.0f)
		{
			//shadow_casts.push_back(light.lightmanager_id);
			//shadow_casts_changed = true;
		}
	}

	void LightManager::RemoveLight(Light& light)
	{
		//see if it was casting shadows and remove.
		SetShadowCaster(light, false);
		/*if (light.getParams().cast_shadow == 1.0f)
		{
			shadow_casts_changed = true;
			for (int i = 0; i < shadow_casts.size(); i++)
			{
				if (shadow_casts[i] == light.lightmanager_id)
				{
					//shadow_casts.erase(shadow_casts.begin() + i);
				}
			}
		}*/

		if (light_map.count(light.lightmanager_id) != 0)
		{
			light_map.erase(light.lightmanager_id);
		}
		else
		{
#ifdef _DEBUG
			Logger::LogWarning("removing light that isn't in lightmanager\n");
#endif
		}
		light.lightmanager_id = -1;
		NotifyUpdate();
	}

	void LightManager::SyncGPUBuffer()
	{
		NotifyUpdate();
	}

}