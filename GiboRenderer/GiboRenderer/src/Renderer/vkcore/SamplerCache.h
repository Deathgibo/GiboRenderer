#pragma once
#include "../../pch.h"

namespace Gibo {
	/*
	 This class is responsible for creating samplers with a nice interface. You fill in the samplerkey with what type of sampler you want and it will
	 either fetch it from the cache or create it first. Samplers are held in an unorderedmap which uses the samplerkey as the key.
	 It also handles all the memory for the samplers

	*/

	struct SamplerKey
	{
		float minLod;
		float maxLod;
		float mipLodBias;
		float maxAnisotropy;
		VkFilter magFilter;
		VkFilter minFilter;
		VkSamplerAddressMode addressModeU;
		VkSamplerAddressMode addressModeV;
		VkSamplerAddressMode addressModeW;
		VkBorderColor borderColor;
		VkSamplerMipmapMode mipmapmode;
		VkBool32 anisotropyenable;

		SamplerKey(VkFilter magFilter_, VkFilter minFilter_, VkSamplerAddressMode addressModeU_, VkSamplerAddressMode addressModeV_, VkSamplerAddressMode addressModeW_, VkBool32 anisotropyenable_,
			float maxAnisotropy_, VkBorderColor borderColor_, VkSamplerMipmapMode mipmapmode_, float minLod_, float maxLod_, float mipLodBias_) : magFilter(magFilter_), minFilter(minFilter_),
			addressModeU(addressModeU_), addressModeV(addressModeV_), addressModeW(addressModeW_), anisotropyenable(anisotropyenable_), maxAnisotropy(maxAnisotropy_), borderColor(borderColor_),
			mipmapmode(mipmapmode_), minLod(minLod_), maxLod(maxLod_), mipLodBias(mipLodBias_) {}

		bool operator==(const SamplerKey& p) const
		{
			return minLod == p.minLod && maxLod == p.maxLod && mipLodBias == p.mipLodBias && maxAnisotropy == p.maxAnisotropy && magFilter == p.magFilter
				&& minFilter == p.minFilter && addressModeU == p.addressModeU && addressModeV == p.addressModeV && addressModeW == p.addressModeW
				&& borderColor == p.borderColor && mipmapmode == p.mipmapmode && anisotropyenable == p.anisotropyenable;
		}
	};

	class SamplerCache
	{
	public:
		SamplerCache(VkDevice device) : deviceref(device) {}
		~SamplerCache() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		SamplerCache(SamplerCache const&) = delete;
		SamplerCache(SamplerCache&&) = delete;
		SamplerCache& operator=(SamplerCache const&) = delete;
		SamplerCache& operator=(SamplerCache&&) = delete;

		void Cleanup();
		void PrintData() const;
		VkSampler GetSampler(SamplerKey key);

	private:
		class MyHashFunction {
		public:
			size_t operator()(const SamplerKey& p) const
			{
				return p.magFilter + p.addressModeU + p.maxAnisotropy + p.maxLod;
			}
		};

		std::unordered_map<SamplerKey, VkSampler, MyHashFunction> cache;
		VkDevice deviceref;
	};


	/* common sampler
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(miplevels);
	samplerInfo.mipLodBias = 0;
	*/
}
