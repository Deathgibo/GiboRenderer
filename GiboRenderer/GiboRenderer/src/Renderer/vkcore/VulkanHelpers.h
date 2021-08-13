#pragma once
#include "../../pch.h"
#include "PhysicalDeviceQuery.h"
#include "vkcoreDevice.h"

namespace Gibo {

	/*
		Holds vulkan helper functions like: finding depth format, creating image view, framebuffer, transition image layout, copy operations, mipmap generation
	*/

	static VkFormat findDepthFormat(VkPhysicalDevice PhysicalDevice) {
		std::vector<VkFormat> candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
		for (int i = 0; i < candidates.size(); i++)
		{
			if (PhysicalDeviceQuery::CheckImageOptimalFormat(PhysicalDevice, candidates[i], VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
			{
				return candidates[i];
			}
		}
		Logger::LogWarning("depth form not found\n");
		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	}

	static bool isDepthFormat(VkFormat format) {
		switch (format) {
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default:
			return false;
		}
	}

	static VkImageView CreateImageView(VkDevice LogicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectmask, uint32_t miplevels, int layercount, VkImageViewType viewtype)
	{
		VkImageView view;
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = viewtype;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectmask;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = miplevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = layercount;

		VULKAN_CHECK(vkCreateImageView(LogicalDevice, &viewInfo, nullptr, &view), "create image view");

		return view;
	}

	static VkFramebuffer CreateFrameBuffer(VkDevice device, uint32_t width, uint32_t height, VkRenderPass renderpass, VkImageView* views, uint32_t view_count, uint32_t layers = 1)
	{
		VkFramebuffer framebuffer;
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.pNext = nullptr;
		//framebufferInfo.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT; //set to this if your using attachmentcreateinfos
		framebufferInfo.renderPass = renderpass;
		framebufferInfo.attachmentCount = view_count;
		framebufferInfo.pAttachments = views;
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = layers;

		VULKAN_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer), "create framebuffer");
		return framebuffer;
	}

    static void TransitionImageLayout(vkcoreDevice& device, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkAccessFlags srcaccess, VkAccessFlags dstaccess,
										VkPipelineStageFlags srcstage, VkPipelineStageFlags dststage, uint32_t miplevel, uint32_t layercount, VkImageAspectFlagBits aspectmask, VkCommandBuffer incmdbuffer = VK_NULL_HANDLE)
	{
		VkCommandBuffer cmdbuffer = incmdbuffer;
		if (incmdbuffer == VK_NULL_HANDLE)
		{
			cmdbuffer = device.beginSingleTimeCommands(POOL_FAMILY::TRANSFER);
		}

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldlayout;
		barrier.newLayout = newlayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = aspectmask;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = miplevel;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = layercount;
		barrier.srcAccessMask = srcaccess;
		barrier.dstAccessMask = dstaccess;

		VkPipelineStageFlags sourceStage = srcstage;
		VkPipelineStageFlags destinationStage = dststage;

		vkCmdPipelineBarrier(
			cmdbuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		if (incmdbuffer == VK_NULL_HANDLE)
		{
			device.submitSingleTimeCommands(cmdbuffer, POOL_FAMILY::TRANSFER);
		}
	}

	//make sure buffer we are copying from has VK_BUFFER_USAGE_TRANSFER_SRC_BIT and the buffer we are copying into has VK_BUFFER_USAGE_TRANSFER_DST_BIT
	//dstbufferaccess is what the memory is currently being accesed for, dstpipelinestage is what stage it is last being used by, maybe just do bottom_bit, or vertex_input_bit
	static void CopyBufferToBuffer(vkcoreDevice& device, VkBuffer srcbuffer, VkBuffer dstbuffer, VkAccessFlags dstcurrentacces, VkPipelineStageFlags dstcurrentpipelinestage,
											VkDeviceSize srcoffset, VkDeviceSize dstoffset, VkDeviceSize sizetocopy)
	{
		VkCommandBuffer commandBuffer = device.beginSingleTimeCommands(POOL_FAMILY::TRANSFER);

		//transition dst buffer from whatever it was to a transfer write
		VkBufferMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask = dstcurrentacces;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = dstbuffer;
		barrier.offset = 0;
		barrier.size = sizetocopy;
		VkPipelineStageFlags srcstage = dstcurrentpipelinestage;
		VkPipelineStageFlags dststage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkBufferCopy copy = {};
		copy.srcOffset = srcoffset;
		copy.dstOffset = dstoffset;
		copy.size = sizetocopy;

		vkCmdPipelineBarrier(commandBuffer, srcstage, dststage, 0, 0, nullptr, 1, &barrier, 0, nullptr);

		vkCmdCopyBuffer(commandBuffer, srcbuffer, dstbuffer, 1, &copy);

		//transition buffer from transfer write to whatever it was
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = dstcurrentacces;
		srcstage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dststage = dstcurrentpipelinestage;

		vkCmdPipelineBarrier(commandBuffer, srcstage, dststage, 0, 0, nullptr, 1, &barrier, 0, nullptr);


		device.submitSingleTimeCommands(commandBuffer, POOL_FAMILY::TRANSFER);
	}

	//make sure image has usage VK_BUFFER_USAGE_TRANSFER_SRC_BIT and image layout is VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL and buffer has VK_BUFFER_USAGE_TRANSFER_DST_BIT
	static void CopyImageToBuffer(vkcoreDevice& device, VkImage srcimage, VkBuffer dstbuffer, VkAccessFlags dstbufferacces, VkPipelineStageFlags dstpipelinestage,
										    VkImageLayout srclayout, uint32_t width, uint32_t height, VkDeviceSize buffersize)
	{
		VkCommandBuffer commandBuffer = device.beginSingleTimeCommands(POOL_FAMILY::TRANSFER);

		//transition dst buffer from whatever it was to a transfer write
		VkBufferMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask = dstbufferacces;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = dstbuffer;
		barrier.offset = 0;
		barrier.size = buffersize;
		VkPipelineStageFlags srcstage = dstpipelinestage;
		VkPipelineStageFlags dststage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkBufferImageCopy copy = {};
		copy.bufferOffset = 0;
		copy.bufferRowLength = 0;
		copy.bufferImageHeight = 0;
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.mipLevel = 0;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageOffset = { 0, 0, 0 };
		copy.imageExtent = {
			width,
			height,
			1
		};

		vkCmdPipelineBarrier(commandBuffer, srcstage, dststage, 0, 0, nullptr, 1, &barrier, 0, nullptr);

		vkCmdCopyImageToBuffer(commandBuffer, srcimage, srclayout, dstbuffer, 1, &copy);

		//transition buffer from transfer write to whatever it was
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = dstbufferacces;
		srcstage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dststage = dstpipelinestage;

		vkCmdPipelineBarrier(commandBuffer, srcstage, dststage, 0, 0, nullptr, 1, &barrier, 0, nullptr);

		device.submitSingleTimeCommands(commandBuffer, POOL_FAMILY::TRANSFER);
	}

	//make sure buffer has VK_BUFFER_USAGE_TRANSFER_SRC_BIT and image has VK_BUFFER_USAGE_TRANSFER_DST_BIT and VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	static void CopyBufferToImage(vkcoreDevice& device, VkImage dstimage, VkBuffer srcbuffer, VkImageLayout dstlayoutcurrent, VkImageLayout dstlayoutfinaldesired, VkAccessFlags dstimageaccess, VkAccessFlags srcimageaccess, VkPipelineStageFlags dstpipelinestage,
		                                   uint32_t width, uint32_t height, VkImageAspectFlags aspectflags, uint32_t miplevels, int layercount)
	{
		VkCommandBuffer commandBuffer = device.beginSingleTimeCommands(POOL_FAMILY::TRANSFER);

		VkImageMemoryBarrier imagebarrier = {};
		imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imagebarrier.srcAccessMask = srcimageaccess;
		imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagebarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imagebarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imagebarrier.image = dstimage;
		imagebarrier.oldLayout = dstlayoutcurrent;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.subresourceRange.aspectMask = aspectflags;
		imagebarrier.subresourceRange.baseArrayLayer = 0;
		imagebarrier.subresourceRange.layerCount = layercount;
		imagebarrier.subresourceRange.baseMipLevel = 0;
		imagebarrier.subresourceRange.levelCount = miplevels;
		VkPipelineStageFlags srcstage = dstpipelinestage;
		VkPipelineStageFlags dststage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkBufferImageCopy copy = {};
		copy.bufferOffset = 0;
		copy.bufferRowLength = 0;
		copy.bufferImageHeight = 0;
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.mipLevel = 0;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = layercount;
		copy.imageOffset = { 0, 0, 0 };
		copy.imageExtent = {
			width,
			height,
			1
		};

		vkCmdPipelineBarrier(commandBuffer, srcstage, dststage, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		vkCmdCopyBufferToImage(commandBuffer, srcbuffer, dstimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

		imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagebarrier.dstAccessMask = dstimageaccess;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.newLayout = dstlayoutfinaldesired;
		srcstage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dststage = dstpipelinestage;

		vkCmdPipelineBarrier(commandBuffer, srcstage, dststage, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		device.submitSingleTimeCommands(commandBuffer, POOL_FAMILY::TRANSFER);
	}

	//make sure image we are copying from has VK_BUFFER_USAGE_TRANSFER_SRC_BIT and VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL 
	//and the image we are copying into has VK_BUFFER_USAGE_TRANSFER_DST_BIT and VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	//TODO - support multiple mip level/array layer copying
	static void CopyImageToImage(vkcoreDevice& device, VkImage srcimage, VkImage dstimage, VkImageLayout srclayout, VkImageLayout dstlayoutcurrent, VkImageLayout dstlayoutfinaldesired,
		VkAccessFlags dstimageaccess, VkPipelineStageFlags dstpipelinestage, uint32_t width, uint32_t height, VkImageAspectFlags dstaspectflags, uint32_t dstmiplevels, int dstlayercount,
		VkOffset3D dstoffset, VkOffset3D srcoffset, VkCommandBuffer incmdbuffer = VK_NULL_HANDLE)
	{
		VkCommandBuffer commandBuffer = incmdbuffer;
		if (incmdbuffer == VK_NULL_HANDLE)
		{
			commandBuffer = device.beginSingleTimeCommands(POOL_FAMILY::TRANSFER);
		}

		VkImageMemoryBarrier imagebarrier = {};
		imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imagebarrier.srcAccessMask = dstimageaccess;
		imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagebarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imagebarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imagebarrier.image = dstimage;
		imagebarrier.oldLayout = dstlayoutcurrent;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.subresourceRange.aspectMask = dstaspectflags;
		imagebarrier.subresourceRange.baseArrayLayer = 0;
		imagebarrier.subresourceRange.layerCount = dstlayercount;
		imagebarrier.subresourceRange.baseMipLevel = 0;
		imagebarrier.subresourceRange.levelCount = dstmiplevels;
		VkPipelineStageFlags srcstage = dstpipelinestage;
		VkPipelineStageFlags dststage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkImageCopy region = {};
		region.dstOffset = dstoffset;
		region.dstSubresource.aspectMask = dstaspectflags;
		region.dstSubresource.mipLevel = 0;
		region.dstSubresource.baseArrayLayer = 0;
		region.dstSubresource.layerCount = 1;

		region.srcOffset = srcoffset;
		region.srcSubresource.aspectMask = dstaspectflags;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;

		region.extent = {
			width,
			height,
			1
		};

		vkCmdPipelineBarrier(commandBuffer, srcstage, dststage, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		vkCmdCopyImage(commandBuffer, srcimage, srclayout, dstimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagebarrier.dstAccessMask = dstimageaccess;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.newLayout = dstlayoutfinaldesired;
		srcstage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dststage = dstpipelinestage;

		vkCmdPipelineBarrier(commandBuffer, srcstage, dststage, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		if (incmdbuffer == VK_NULL_HANDLE)
		{
			device.submitSingleTimeCommands(commandBuffer, POOL_FAMILY::TRANSFER);
		}
	}

	//make sure image is supported with VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
	static void generateMipmaps(vkcoreDevice& device, VkImage image, VkFormat format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, VkImageLayout finallayout)
	{
		VkCommandBuffer commandbuffer = device.beginSingleTimeCommands(POOL_FAMILY::TRANSFER);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			VkImageBlit blit = {};
			blit.srcOffsets[0] = { 0,0,0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0,0,0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1 ,1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit, VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = finallayout;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = finallayout;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		device.submitSingleTimeCommands(commandbuffer, POOL_FAMILY::TRANSFER);
	}

	//vkCmdResolveImage - Resolves a multisamplingimage to a non-multisampling image
	//vkCmdBlitImage - Copy regions of an image, potentially performing format conversion
	static void blitresolveimage()
	{
		/*
		Probably have to do pipeline barriers and check if there is blit and resolve support
		
		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0,0,0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0,0,0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1 ,1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit, VK_FILTER_LINEAR);


		VkImageResolve resolveregion;
		resolveregion.dstOffset;
		resolveregion.srcOffset;
		resolveregion.dstSubresource;
		resolveregion.srcSubresource;
		resolveregion.extent;

		vkCmdResolveImage(cmdbuffer, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, resolveRegions);
		*/
	}

	/* QUERY NOTES
	VkPhysicalDeviceLimits::timestampComputeAndGraphics is a boolean telling you whether the device supports these or not
	VkPhysicalDeviceLimits::timestampPeriod is the number of nanoseconds required for a timestamp query to be incremented by 1

	VkQueryType: VK_QUERY_TYPE_OCCLUSION - is for occlusion testing, I think it tells you how many fragments get ran or overwritten or something
	VK_QUERY_TYPE_PIPELINE_STATISTICS - this tells you how many invocations each pipeline stage gets ran like fragment, vertex. seems useful
	VK_QUERY_TYPE_TIMESTAMP - this is the one that gives you the time gpu takes.
	*/
	static void QueryStamps(VkPhysicalDevice physicaldevice)
	{
		/*

		use muiltiple pools per frame in flight

		the timer value will not be written until all previously submitted commands reached the specified stage
		*/

		/*VkQueryPoolCreateInfo poolinfo;
		poolinfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		poolinfo.flags;
		poolinfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		poolinfo.queryCount = 5;
		poolinfo.pipelineStatistics;

		vkCreateQueryPool(device, &poolinfo, nullptr, pool);
		
		vkCmdWriteTimestamp(commandbuffer, pipelinestage, pool, query);
		
		vkGetQueryPoolResults(device, pool, firstquery, querycount, datasize, data, stride, flags);
		VK_QUERY_RESULT_64_BIT
		VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
		*/

	}
}