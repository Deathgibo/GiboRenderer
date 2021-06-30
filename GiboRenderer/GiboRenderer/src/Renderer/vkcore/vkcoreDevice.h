#pragma once
#include <GLFW/glfw3.h>
#include "RenderPassCache.h"
#include "PipelineCache.h"
#include "SamplerCache.h"
#include "CommandPoolCache.h"
#include "VulkanHelpers.h"

/*
 This is the main class of the vulkan backend. This will be ideally used by frontend as the main interface, passed around by reference.
 It will create the vulkan device, pick the pysical device, create swapcain, logical device. As well as it will hold most of the main backend classes.
 It is also responsible for holding the VMA allocator and should be called when you want to create/destroy a buffer or image.

 *Make sure to call DestroyDevice() after use
*/

namespace Gibo {

	struct vkcoreBuffer
	{
		VkBuffer buffer;
		VmaAllocation allocation;
		void* mapped_data = nullptr;
	};

	struct vkcoreImage
	{
		VkImage image;
		VmaAllocation allocation;
		void* mapped_data = nullptr;;
	};

	struct DescriptorHelper {
		std::vector<std::vector<vkcoreBuffer>> uniformbuffers;
		std::vector<std::vector<uint64_t>> buffersizes;
		std::vector<std::vector<VkImageView>> imageviews;
		std::vector<std::vector<VkSampler>> samplers;
		std::vector<std::vector<VkBufferView>> bufferviews;

		DescriptorHelper(int size) : uniformbuffers(), buffersizes(size), imageviews(size), samplers(size), bufferviews(size)
		{

		}
	};

	class vkcoreDevice
	{
	public:
		vkcoreDevice() = default;
		~vkcoreDevice() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		vkcoreDevice(vkcoreDevice const&) = delete;
		vkcoreDevice(vkcoreDevice&&) = delete;
		vkcoreDevice& operator=(vkcoreDevice const&) = delete;
		vkcoreDevice& operator=(vkcoreDevice&&) = delete;

		bool CreateDevice(std::string name, GLFWwindow* window, VkExtent2D& window_extent);
		void DestroyDevice();

		//VMA allocation calls
		bool CreateImage(VkImageType image_type, VkFormat Format, VkImageUsageFlags usage, VkSampleCountFlagBits samplecount, uint32_t width, uint32_t height, uint32_t depth,
			             uint32_t mip_levels, uint32_t array_layers, VmaMemoryUsage memusage, VmaAllocationCreateFlags mapped_bit_flag, vkcoreImage& vkimage);
		void DestroyImage(vkcoreImage& vkimage);
		bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memusage, VmaAllocationCreateFlags mapped_bit_flag, vkcoreBuffer& vkbuffer);
		void DestroyBuffer(vkcoreBuffer& vkbuffer);
		void BindData(VmaAllocation allocation, void* data, size_t size);
		void BindDataAlwaysMapped(void* mapped_ptr, void* data, size_t size);
		VmaAllocationInfo GetAllocationInfo(VmaAllocation allocation);
		void PrintVMAStats();

		VkCommandBuffer beginSingleTimeCommands(POOL_FAMILY familyoperation);
		void submitSingleTimeCommands(VkCommandBuffer buffer, POOL_FAMILY familyoperation);

		//Set/Get
		VkPhysicalDevice GetPhysicalDevice() const { return PhysicalDevice; }
		VkDevice GetDevice() const { return LogicalDevice; }
		RenderPassCache& GetRenderPassCache() { return *renderpassCache; }
		PipelineCache& GetPipelineCache() { return *pipelineCache; }
		SamplerCache& GetSamplerCache() { return *samplerCache; }
		CommandPoolCache& GetCommandPoolCache() { return *cmdpoolCache; }
	private:
		bool CreateVulkanInstance(std::string name);
		bool CreateSurface(GLFWwindow* window);
		bool PickPhysicalDevice();
		bool CreateLogicalDevice();
		bool CreateAllocator();
		bool CreateSwapChain(VkExtent2D& window_extent);

		bool checkvalidationLayerSupport();
		bool CheckDeviceQueueSupport(VkPhysicalDevice device, VkSurfaceKHR surface, VkQueueFlags flags);

	private:
		//put most frequently used up top lease at bottom. Group things used together. Larger objects up top too.
		//VK_Handles are 8 bytes so 8 can fit on a cache line
		VmaAllocator Allocator;
		RenderPassCache* renderpassCache;
		PipelineCache* pipelineCache;
		SamplerCache* samplerCache;
		CommandPoolCache* cmdpoolCache;
		VkInstance Instance;
		VkSurfaceKHR Surface;
		VkPhysicalDevice PhysicalDevice;
		VkDevice LogicalDevice;
		VkSwapchainKHR SwapChain;
		std::vector<VkImage> swapChainImages;
		std::vector<VkImageView> swapChainImageViews;


		VkQueue GraphicsQueue;
		VkQueue PresentQueue;
		VkQueue ComputeQueue;
		VkQueue TransferQueue;

		uint32_t buffer_allocations = 0;
		uint32_t image_allocations = 0;
	};

}

