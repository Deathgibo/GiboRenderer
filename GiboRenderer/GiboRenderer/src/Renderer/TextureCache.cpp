#include "../pch.h"
#include "TextureCache.h"
#include "../ThirdParty/stb_image.h"
#include "vkcore/VulkanHelpers.h"

namespace Gibo {

	void TextureCache::CleanUp()
	{
		for (auto& texture : texture2dcache)
		{
			deviceref.DestroyImage(texture.second.image);
			vkDestroyImageView(deviceref.GetDevice(), texture.second.view, nullptr);
			//sampler is handled by samplercache
		}
		texture2dcache.clear();

		for (int i = 0; i < cubemaparray.size(); i++)
		{
			deviceref.DestroyImage(cubemaparray[i].image);
			vkDestroyImageView(deviceref.GetDevice(), cubemaparray[i].view, nullptr);
		}
	}

	void TextureCache::PrintInfo() const
	{
		Logger::Log("total memory size ", gpumemory_size / 1000000, "MB", "2D texture count: ", texture2dcache.size(), "cube array count: ", cubemaparray.size(), "\n");
	}

	//TODO - research more about image format loading higher precision like 16 bit. also dont enforce alpha for saving memory.
	TextureCache::internaltexture TextureCache::Create2DTexture(std::string filename, bool mipped)
	{
		internaltexture texture;

		//load texture
		int texWidth, texHeight, texChannels;
		unsigned char* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);//last parameter will load pixels with that many channels

		if (!pixels)
		{
			const char* message = stbi_failure_reason();
			printf("STBI MESSAGE: %s  %s\n", message, filename);
		}

		VkDeviceSize imageSize = texWidth * texHeight * STBI_rgb_alpha;
		uint32_t miplevels = 1;
		if (mipped)
		{
			miplevels = static_cast<uint32_t>(std::floor(std::log2((texWidth > texHeight) ? texWidth : texHeight))) + 1;
		}
		texture.miplevels = miplevels;
		Logger::Log(filename, " width: ", texWidth, " height: ", texHeight, " miplevels: ", miplevels, " ", imageSize / 1000000, "MB", "\n");

		//TODO - find formats that have to be support and try 16 bit textures?
		//we need to pick a format supported by linear sampling if we are going to mip-map 
		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB; //what formats are supported?
		VkFormat desired_format[5] = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8G8B8A8_SINT, VK_FORMAT_R8G8B8A8_UINT,VK_FORMAT_R8G8B8A8_SNORM };
		for (int i = 0; i < 5; i++)
		{
			if (!PhysicalDeviceQuery::CheckImageOptimalFormat(deviceref.GetPhysicalDevice(), desired_format[i], VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
			{
				Logger::LogWarning("image format not supported for 2D texture sampling");
				continue;
			}

			if (mipped && !PhysicalDeviceQuery::CheckImageOptimalFormat(deviceref.GetPhysicalDevice(), desired_format[i], VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			{
				Logger::LogWarning("image format not supported for 2D texture linear blitting");
				continue;
			}

			format = desired_format[i];
			break;
		}

		//create image, imageview
		deviceref.CreateImage(VK_IMAGE_TYPE_2D, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SAMPLE_COUNT_1_BIT, texWidth,
			texHeight, 1, miplevels, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, texture.image);
		texture.view = CreateImageView(deviceref.GetDevice(), texture.image.image, format, VK_IMAGE_ASPECT_COLOR_BIT, miplevels, 1, VK_IMAGE_VIEW_TYPE_2D);

		//create staging buffer, bind pixels to it, copy to image
		vkcoreBuffer stagingbuffer;
		deviceref.CreateBuffer(imageSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, stagingbuffer);

		deviceref.BindData(stagingbuffer.allocation, pixels, imageSize);

		//transition from undefined to transfer_dst_optimal
		//copy
		CopyBufferToImage(deviceref, texture.image.image, stagingbuffer.buffer, VK_IMAGE_LAYOUT_UNDEFINED, (mipped) ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_READ_BIT, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, texWidth, texHeight, VK_IMAGE_ASPECT_COLOR_BIT, miplevels, 1);

		//generate mip maps and transition it to shader_read_only
		if (mipped)
		{
			//todo- check if format is supported VK_FORMAT_FEATURE_BLIT_DST_BIT
			//generate mipmaps
			generateMipmaps(deviceref, texture.image.image, format, texWidth, texHeight, miplevels, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		deviceref.DestroyBuffer(stagingbuffer);
		stbi_image_free(pixels);

		gpumemory_size += imageSize;
		return texture;
	}

	//layout(binding = X) uniform samplerCube cubeMapTexture;
	TextureCache::internaltexture TextureCache::CreateCubeMap(std::string* paths, int path_count)
	{
		internaltexture texture;
		texture.miplevels = 1;

		//load 6 images
		int texWidth, texHeight, texChannels;
		stbi_uc* pixel1 = stbi_load(paths->c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb);
		stbi_uc* pixel2 = stbi_load((paths + 1)->c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb);
		stbi_uc* pixel3 = stbi_load((paths + 2)->c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb);
		stbi_uc* pixel4 = stbi_load((paths + 3)->c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb);
		stbi_uc* pixel5 = stbi_load((paths + 4)->c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb);
		stbi_uc* pixel6 = stbi_load((paths + 5)->c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb);

		if (!pixel1) { printf("STBI MESSAGE: %s  %s\n", stbi_failure_reason(), paths->c_str()); }
		if (!pixel2) { printf("STBI MESSAGE: %s  %s\n", stbi_failure_reason(), (paths + 1)->c_str()); }
		if (!pixel3) { printf("STBI MESSAGE: %s  %s\n", stbi_failure_reason(), (paths + 2)->c_str()); }
		if (!pixel4) { printf("STBI MESSAGE: %s  %s\n", stbi_failure_reason(), (paths + 3)->c_str()); }
		if (!pixel5) { printf("STBI MESSAGE: %s  %s\n", stbi_failure_reason(), (paths + 4)->c_str()); }
		if (!pixel6) { printf("STBI MESSAGE: %s  %s\n", stbi_failure_reason(), (paths + 5)->c_str()); }
		if (!pixel1 || !pixel2 || !pixel3 || !pixel4 || !pixel5 || !pixel6)
		{
			Logger::LogError("failed to load cubemap images", paths->c_str(), "\n");
		}

		VkDeviceSize imageSize = texWidth * texHeight * STBI_rgb * 6;
		VkDeviceSize layerSize = imageSize / 6;

		Logger::LogInfo("loading cubemap: ", paths->c_str(), " ", imageSize / 1000000.f, "MB", "\n");

		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB; //what formats are supported?
		VkFormat desired_format[5] = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8G8B8A8_SINT, VK_FORMAT_R8G8B8A8_UINT,VK_FORMAT_R8G8B8A8_SNORM };
		for (int i = 0; i < 5; i++)
		{
			if (!PhysicalDeviceQuery::CheckImageOptimalFormat(deviceref.GetPhysicalDevice(), desired_format[i], VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
			{
				Logger::LogWarning("image format not supported for 2D texture sampling");
				continue;
			}

			format = desired_format[i];
			break;
		}

		//create image with 6 layers, view with viewtypecube, and sampler
		deviceref.CreateImage(VK_IMAGE_TYPE_2D, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, texWidth,
			texHeight, 1, 1, 6, VMA_MEMORY_USAGE_GPU_ONLY, 0, texture.image);
		texture.view = CreateImageView(deviceref.GetDevice(), texture.image.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 6, VK_IMAGE_VIEW_TYPE_CUBE);

		//create staging buffer, bind pixels to it, copy to image
		vkcoreBuffer stagingbuffer;
		deviceref.CreateBuffer(imageSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, stagingbuffer);

		void* mappedData;
		VULKAN_CHECK(vmaMapMemory(deviceref.GetAllocator(), stagingbuffer.allocation, &mappedData), "mapping memory");
		for (int i = 0; i < 6; i++)
		{
			stbi_uc* current_pixels = nullptr;
			switch (i)
			{
			case 0: current_pixels = pixel1; break;
			case 1: current_pixels = pixel2; break;
			case 2: current_pixels = pixel3; break;
			case 3: current_pixels = pixel4; break;
			case 4: current_pixels = pixel5; break;
			case 5: current_pixels = pixel6; break;
			}

			memcpy(static_cast<stbi_uc*>(mappedData) + (layerSize*i), current_pixels, static_cast<size_t>(layerSize));
		}
		vmaUnmapMemory(deviceref.GetAllocator(), stagingbuffer.allocation);

		//copy buffer to image
		CopyBufferToImage(deviceref, texture.image.image, stagingbuffer.buffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, 0,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, texWidth, texHeight, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);

		deviceref.DestroyBuffer(stagingbuffer);
		stbi_image_free(pixel1);
		stbi_image_free(pixel2);
		stbi_image_free(pixel3);
		stbi_image_free(pixel4);
		stbi_image_free(pixel5);
		stbi_image_free(pixel6);

		gpumemory_size += imageSize;
		return texture;
	}

	vkcoreTexture TextureCache::Get2DTexture(std::string filename, bool mipped, VkFilter magfilter, VkFilter minfilter, float maxanisotropy, VkSamplerAddressMode addressmode)
	{
		vkcoreTexture texture;

		textureKey key(filename, mipped);
		if (texture2dcache.count(key) == 1)
		{
			//its in cache get values
			texture.image = texture2dcache[key].image.image;
			texture.view = texture2dcache[key].view;
		}
		else
		{
			//its not in cache create it, store in cache, and set values
			internaltexture internaltex = Create2DTexture(filename, mipped);
			texture2dcache[key] = internaltex;
			texture.image = internaltex.image.image;
			texture.view = internaltex.view;
		}

		bool anisotropy_supported = PhysicalDeviceQuery::GetDeviceFeatures(deviceref.GetPhysicalDevice()).samplerAnisotropy;
		SamplerKey sampler_data(magfilter, minfilter, addressmode, addressmode, addressmode, anisotropy_supported, maxanisotropy, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0, texture2dcache[key].miplevels, 0.0);

		texture.sampler = deviceref.GetSamplerCache().GetSampler(sampler_data);

		return texture;
	}

	vkcoreTexture TextureCache::GetCubeMapTexture(std::string* paths, int path_count, VkFilter magfilter, VkFilter minfilter, float maxanisotropy, VkSamplerAddressMode addressmode)
	{
		vkcoreTexture texture;

		internaltexture internaltex = CreateCubeMap(paths, path_count);
		cubemaparray.push_back(internaltex);
		texture.image = internaltex.image.image;
		texture.view = internaltex.view;

		bool anisotropy_supported = PhysicalDeviceQuery::GetDeviceFeatures(deviceref.GetPhysicalDevice()).samplerAnisotropy;
		SamplerKey sampler_data(magfilter, minfilter, addressmode, addressmode, addressmode, anisotropy_supported, maxanisotropy, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			                    VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 1, 0.0);
		texture.sampler = deviceref.GetSamplerCache().GetSampler(sampler_data);

		return texture;
	}


}