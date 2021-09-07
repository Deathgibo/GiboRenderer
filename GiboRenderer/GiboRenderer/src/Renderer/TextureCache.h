#pragma once
#include "vkcore/vkcoreDevice.h"

namespace Gibo {

	struct vkcoreTexture
	{
		VkImage image;
		VkImageView view;
		VkSampler sampler;

		vkcoreTexture() : image(VK_NULL_HANDLE), view(VK_NULL_HANDLE), sampler(VK_NULL_HANDLE) {}
	};

	/*
	Stores all the textures so you never have to reload one again as well as its stored on vram once unless you want it mipped and nonmipped.
	The sampler is taken from the samplercache. You fill out your texturekey and it returns a texture.
	cubemaps don't have a cache.
	*/
	class TextureCache
	{
	public:
		TextureCache(vkcoreDevice& device) : deviceref(device), gpumemory_size(0) {};
		~TextureCache() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		TextureCache(TextureCache const&) = delete;
		TextureCache(TextureCache&&) = delete;
		TextureCache& operator=(TextureCache const&) = delete;
		TextureCache& operator=(TextureCache&&) = delete;

		void CleanUp();
		void PrintInfo() const;

		vkcoreTexture Get2DTexture(std::string filename, bool mipped, VkFilter magfilter = VK_FILTER_LINEAR, VkFilter minfilter = VK_FILTER_LINEAR, float maxanisotropy = 4.0f, VkSamplerAddressMode addressmode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
		vkcoreTexture GetCubeMapTexture(std::string* paths, int path_count, VkFilter magfilter = VK_FILTER_LINEAR, VkFilter minfilter = VK_FILTER_LINEAR, float maxanisotropy = 4.0f, VkSamplerAddressMode addressmode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	private:
		struct internaltexture
		{
			vkcoreImage image;
			VkImageView view;
			uint32_t miplevels;
		};

		internaltexture Create2DTexture(std::string filename, bool mipped);
		internaltexture CreateCubeMap(std::string* paths, int path_count);
	private:
		struct textureKey
		{
			std::string filename;
			bool mipped;	

			textureKey(std::string name, bool mipped_) : filename(name), mipped(mipped_) {};

			bool operator==(const textureKey& p) const
			{
				return filename == p.filename && mipped == p.mipped;
			}
		};
		class MyHashFunction {
		public:
			size_t operator()(const textureKey& p) const
			{
				size_t count = 0;
				for (int i = 0; i < p.filename.size(); i++)
				{
					count += p.filename[i];
				}
				return count;
			}
		};
	

		std::unordered_map<textureKey, internaltexture, MyHashFunction> texture2dcache;
		std::vector<internaltexture> cubemaparray;

		vkcoreDevice& deviceref;
		size_t gpumemory_size;
	};
}

