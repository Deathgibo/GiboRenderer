#include "../../pch.h"
#include "SamplerCache.h"

namespace Gibo {

	void SamplerCache::Cleanup()
	{
		for (auto& sampler : cache)
		{
			vkDestroySampler(deviceref, sampler.second, nullptr);
		}
		cache.clear();
	}

	void SamplerCache::PrintData() const
	{
		Logger::Log("-----SamplerCache-----\n");
		Logger::Log("cache elements: ", cache.size(), "\n");
		for (int i = 0; i < cache.bucket_count(); i++)
		{
			Logger::Log("bucket ", i, " elements ", cache.bucket_size(i), "\n");
		}
	}

	VkSampler SamplerCache::GetSampler(SamplerKey key)
	{
		//check to see if sampler is in cache
		if (cache.count(key) == 1)
		{
			return cache[key];
		}
		//create sampler, store in cache, and return handle
		VkSampler sampler;
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = key.magFilter;
		samplerInfo.minFilter = key.minFilter;
		samplerInfo.addressModeU = key.addressModeU;
		samplerInfo.addressModeV = key.addressModeV;
		samplerInfo.addressModeW = key.addressModeW;
		samplerInfo.anisotropyEnable = key.anisotropyenable;
		samplerInfo.maxAnisotropy = key.maxAnisotropy;
		samplerInfo.borderColor = key.borderColor;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = key.mipmapmode;
		samplerInfo.minLod = key.minLod;
		samplerInfo.maxLod = key.maxLod;
		samplerInfo.mipLodBias = key.mipLodBias;

		VkResult result = vkCreateSampler(deviceref, &samplerInfo, nullptr, &sampler);
		VULKAN_CHECK(result, "creating sampler");
		
		cache[key] = sampler;
		return sampler;
	}
}