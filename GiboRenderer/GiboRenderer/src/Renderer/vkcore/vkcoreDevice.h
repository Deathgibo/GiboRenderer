#pragma once
#include <GLFW/glfw3.h>
/*
 This is the main class of the vulkan backend. This will be ideally used by frontend as the main interface, passed around by reference.
 It will create the vulkan device, pick the pysical device, create swapcain, logical device. As well as it will hold framebuffercaces.

 *Make sure to call DestroyDevice() after use
*/

namespace Gibo {

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

		bool CreateDevice(std::string name, GLFWwindow* window);
		void DestroyDevice();

		VkPhysicalDevice GetDevice() const { return PhysicalDevice; }
	private:
		bool CreateVulkanInstance(std::string name);
		bool CreateSurface(GLFWwindow* window);
		bool PickPhysicalDevice();
		bool CreateLogicalDevice();
		bool CreateSwapChain();

		bool checkvalidationLayerSupport();
		bool CheckDeviceQueueSupport(VkPhysicalDevice device, VkSurfaceKHR surface, VkQueueFlags flags);
		int GetQueueFamily(VkPhysicalDevice device, VkQueueFlags flags);
		int GetPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface);

	private:
		//VK_Handles are 8 bytes so 8 can fit on a cache line
		VkInstance Instance;
		VkSurfaceKHR Surface;
		VkPhysicalDevice PhysicalDevice;
		VkDevice LogicalDevice;
		VkSwapchainKHR SwapChain;

		VkQueue GraphicsQueue;
		VkQueue PresentQueue;
		VkQueue ComputeQueue;
		VkQueue TransferQueue;
	};

	//Queue are just something you submit command buffers to that actually put the commands on a queue for the gpu to run
	//Queues can handle different operations on different hardware of the gpu so they have different types
	//Families are just queues that have the same exact property. Really theres different queues for multi-threading purposes
	//Graphics: For creating graphics pipelines and drawing (vkCmdDraw)
	//Compute: For creating compute pipelines and dispatching compute shaders (vkCmdDispatch)
	//Transfer: Used for very fast memory-copying operations (vkCmdCopy)
	//Sparse: Allows for additional memory management features (vkQueueBindSparse)
	//Present: Used for presenting Command

}

