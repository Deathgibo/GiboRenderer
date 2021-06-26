#include "../../pch.h"
#include <GLFW/glfw3.h>
#include "vkcoreDevice.h"
#include "vkcorePrintHelper.h"
#include <optional>
#include <set>

namespace Gibo {

	#ifdef _DEBUG
		static bool enablevalidation = true;
	#else
		static bool enablevalidation = false;
	#endif

	static std::vector<const char*> validationLayers = 
	{
		"VK_LAYER_KHRONOS_validation"
	};

	void vkcoreDevice::DestroyDevice()
	{
		vkDestroyDevice(LogicalDevice, nullptr);
		vkDestroySurfaceKHR(Instance, Surface, nullptr);
		vkDestroyInstance(Instance, nullptr);
	}

	bool vkcoreDevice::CreateDevice(std::string name, GLFWwindow* window)
	{
		bool valid = true;
		valid = valid && CreateVulkanInstance(name);
		valid = valid && CreateSurface(window);
		valid = valid && PickPhysicalDevice();
		valid = valid && CreateLogicalDevice();
		valid = valid && CreateSwapChain();
		
		return valid;
	}

	bool vkcoreDevice::CreateVulkanInstance(std::string name)
	{
		uint32_t API_VERSION = VK_API_VERSION_1_1;

		if (enablevalidation && !checkvalidationLayerSupport()) {
			Logger::LogError("validation layers requested, but not available!");
			enablevalidation = false;
		}

		PrintVulkanAPIVersion(API_VERSION);
		PrintVulkanExtentions();
		PrintVulkanLayers();

		VkApplicationInfo appcreate{};
		appcreate.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appcreate.pNext = nullptr;
		appcreate.pApplicationName = name.c_str();
		appcreate.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
		appcreate.pEngineName = name.c_str();
		appcreate.engineVersion = VK_MAKE_VERSION(1, 1, 0);
		//this api version matters!
		appcreate.apiVersion = API_VERSION;

		/* Example of instance extension
		VkValidationFeaturesEXT valf{};
		valf.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		valf.pNext = nullptr;
		std::vector<VkValidationFeatureEnableEXT> enables = { VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT };
		valf.enabledValidationFeatureCount = enables.size();
		valf.pEnabledValidationFeatures = enables.data();
		std::vector<VkValidationFeatureEnableEXT> disables = { };
		valf.disabledValidationFeatureCount = disables.size();
		valf.pDisabledValidationFeatures = nullptr;
		*/
		
		VkInstanceCreateInfo create{};
		create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create.pNext = nullptr;
		create.flags = 0;
		create.pApplicationInfo = &appcreate;

		//VULKAN LAYERS
		if (enablevalidation) {
			create.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			create.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			create.enabledLayerCount = 0;
			create.ppEnabledLayerNames = nullptr;
		}

		//VULKAN EXTENSIONS
		uint32_t glfwExtensionsCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
		create.enabledExtensionCount = glfwExtensionsCount;
		create.ppEnabledExtensionNames = glfwExtensions;

		auto result = vkCreateInstance(&create, nullptr, &Instance);
		VULKAN_CHECK(result, "create instance");
		if (result != VK_SUCCESS)
		{
			return false;
		}
		return true;
	}

	bool vkcoreDevice::CreateSurface(GLFWwindow* window)
	{
		if (glfwCreateWindowSurface(Instance, window, nullptr, &Surface) != VK_SUCCESS)
		{
			Logger::LogError("can't create window surface!");
			return false;
		}	
		return true;
	}

	bool vkcoreDevice::checkvalidationLayerSupport() 
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layername : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layername, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	bool vkcoreDevice::PickPhysicalDevice()
	{
		uint32_t devicecount = 0;
		vkEnumeratePhysicalDevices(Instance, &devicecount, nullptr);
		if (devicecount == 0) {
			Logger::LogError("no physical devices found! (that support vulkan)");
			return false;
		}
		std::vector<VkPhysicalDevice> devices(devicecount);
		VkResult result = vkEnumeratePhysicalDevices(Instance, &devicecount, devices.data());

		bool device_chosen = false;
		//Make sure physical device has all the queue I use (graphics, present, compute) as well as swapchain extensions. Maybe do a gpu ranking system to pick best one
		for (int i = 0; i < devicecount; i++)
		{
			Logger::Log("Physical Device: ", i, '\n');
			PrintPhysicalDeviceProperties(devices[i]);
			PrintPhysicalDeviceFeatures(devices[i]);
			PrintPhysicalDeviceQueueFamilyProperties(devices[i], Surface);
			PrintPhysicalDeviceMemoryProperties(devices[i]);
			//PrintPhysicalDeviceExtensions(devices[i]);

			//check that this device has graphics, compute, transfer, and present support
			if (!CheckDeviceQueueSupport(devices[i], Surface, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
			{
				Logger::LogWarning("physical device doesn't support graphics, compute, transfer and present queue!");
				continue;
			}

			//check swapchain support
			uint32_t extensionCount;
			vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extensionCount, nullptr);
			std::vector<VkExtensionProperties> extensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extensionCount, extensions.data());

			bool swapsupported = false;
			for (int i = 0; i < extensions.size(); i++) {
				char swapchainextension[256] = "VK_KHR_swapchain";
				if (strcmp(swapchainextension, extensions[i].extensionName)) {
					swapsupported = true;
				}
			}
			if (!swapsupported) {
				Logger::LogWarning("physical device doesn't support swapchain!");
				continue;
			}
			
			PhysicalDevice = devices[i];
			device_chosen = true;
			break;
		}

		return device_chosen;
	}

	bool vkcoreDevice::CreateLogicalDevice()
	{
		//choose which queues we want to have
		uint32_t graphicsfamily = GetQueueFamily(PhysicalDevice, VK_QUEUE_GRAPHICS_BIT);
		uint32_t computefamily = GetQueueFamily(PhysicalDevice, VK_QUEUE_COMPUTE_BIT);
		uint32_t transferfamily = GetQueueFamily(PhysicalDevice, VK_QUEUE_TRANSFER_BIT);
		uint32_t presentfamily = GetPresentQueueFamily(PhysicalDevice, Surface);

		std::set<uint32_t> uniqueQueueFamilies = { graphicsfamily, computefamily, transferfamily, presentfamily };
		
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		//[0.0-1.0] : 0 lowest and 1 highest priority
		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.pNext = nullptr;
			queueCreateInfo.flags = 0;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;

			queueCreateInfos.push_back(queueCreateInfo);
		}

		//choose which physical device features we want to enable
		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(PhysicalDevice, &features);
		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.geometryShader = VK_TRUE;
		deviceFeatures.tessellationShader = VK_TRUE;
		deviceFeatures.fillModeNonSolid = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
		if (features.geometryShader == FALSE) { Logger::LogWarning("device doesn't have geometryShader"); }
		if (features.tessellationShader == FALSE) { Logger::LogWarning("device doesn't have tessellationShader"); }
		if (features.fillModeNonSolid == FALSE) { Logger::LogWarning("device doesn't have fillModeNonSolid"); }
		if (features.sampleRateShading == FALSE) { Logger::LogWarning("device doesn't have sampleRateShading"); }
		if (features.samplerAnisotropy == FALSE) { Logger::LogWarning("device doesn't have samplerAnisotropy"); }
		if (features.fragmentStoresAndAtomics == FALSE) { Logger::LogWarning("device doesn't have fragmentStoresAndAtomics"); }

		//pick which physical device extension we need to support
		const std::vector<const char*> deviceExtensions = {
			"VK_KHR_swapchain"
		};

		VkDeviceCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		info.pQueueCreateInfos = queueCreateInfos.data();
		info.pEnabledFeatures = &deviceFeatures;
		info.enabledExtensionCount = deviceExtensions.size();
		info.ppEnabledExtensionNames = deviceExtensions.data();
		//depracted and ignored
		if (enablevalidation) {
			info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			info.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			info.enabledLayerCount = 0;
			info.ppEnabledLayerNames = nullptr;
		}

		VkResult result = vkCreateDevice(PhysicalDevice, &info, nullptr, &LogicalDevice);
		VULKAN_CHECK(result, "create device");
		if (result != VK_SUCCESS)
		{
			Logger::LogError("Couldn't create device\n");
			return false;
		}

		//grab handles to queues
		vkGetDeviceQueue(LogicalDevice, graphicsfamily, 0, &GraphicsQueue);
		vkGetDeviceQueue(LogicalDevice, presentfamily, 0, &PresentQueue);
		vkGetDeviceQueue(LogicalDevice, computefamily, 0, &ComputeQueue);
		vkGetDeviceQueue(LogicalDevice, transferfamily, 0, &TransferQueue);

		return true;
	}

	bool vkcoreDevice::CreateSwapChain()
	{
		//Presentation mode
		//IMMEDIATE: images are just immediately replacing previous ones. screen tearing happens
		//FIFO: *must be supported. sends them to a qeueu that are displayed in sync (vsync)
		//FIFO RELAXED:  like fifo but no vsync so screen tearing will happen if frames are coming in too slow
		//Mailbox: triple buffering
		//check if physical device supports swap chain and enable extension in logical device

		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &capabilities);
		
		std::vector<VkSurfaceFormatKHR> formats;
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &formatCount, nullptr);
		if (formatCount != 0) {
			formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &formatCount, formats.data());
		}
	
		std::vector<VkPresentModeKHR> presentmodes;
		uint32_t presentCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &presentCount, nullptr);
		if (presentCount != 0) {
			presentmodes.resize(presentCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &presentCount, presentmodes.data());
		}

		if (formatCount == 0 || presentCount == 0) {
			throw std::runtime_error("swap chain doesnt have formats or presentmodes");
		}

		//Pick format and color space
		//surface formats specify color spaces as well as the channel format
		VkSurfaceFormatKHR thesurfaceformat;
		for (int i = 0; i < formats.size(); i++) {
			if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				thesurfaceformat = formats[i];
				break;
			}
		}

		//present mode is just an enum about how the swap chain handles swapping the images, it can be vsync, triple buffer, single, etc
		VkPresentModeKHR thesurfacepresentmode;
		for (int i = 0; i < presentmodes.size(); i++) {
			//VK_PRESENT_MODE_MAILBOX_KHR
			if (presentmodes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
				thesurfacepresentmode = presentmodes[i];
				break;
			}
		}

		//surfacecapabilities describes the available resolutions of the swap chain
		//you want to usually set this to the windows resolution
		capabilities.currentExtent.width = width;
		capabilities.currentExtent.height = height;

		uint32_t imageCount = capabilities.minImageCount + 1;
		VkSwapchainCreateInfoKHR swapinfo = {};
		swapinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapinfo.surface = surface;
		swapinfo.minImageCount = imageCount;
		swapinfo.imageFormat = thesurfaceformat.format;
		swapinfo.imageColorSpace = thesurfaceformat.colorSpace;
		swapinfo.imageExtent = capabilities.currentExtent;
		swapinfo.imageArrayLayers = 1;
		swapinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; //VK_IMAGE_USAGE_TRANSFER_DST_BIT for post-processing

		vkcoreQueues::QueueFamilyIndices indices = vkcoreQueues::findQueueFamilies(PhysicalDevice, surface);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		if (indices.graphicsFamily != indices.presentFamily) {
			swapinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapinfo.queueFamilyIndexCount = 2;
			swapinfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			swapinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		swapinfo.preTransform = capabilities.currentTransform;
		swapinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapinfo.presentMode = thesurfacepresentmode;
		swapinfo.clipped = VK_TRUE;
		swapinfo.oldSwapchain = VK_NULL_HANDLE;

		VULKAN_CHECK(vkCreateSwapchainKHR(LogicalDevice, &swapinfo, nullptr, &swapchain), "creating swapchain");

		//get the swap chain images
		vkGetSwapchainImagesKHR(LogicalDevice, swapchain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(LogicalDevice, swapchain, &imageCount, swapChainImages.data());

		swapChainFormat = thesurfaceformat.format;
		swapChainExtent = capabilities.currentExtent;

		createImageViews();
	}

	bool vkcoreDevice::CheckDeviceQueueSupport(VkPhysicalDevice device, VkSurfaceKHR surface, VkQueueFlags flags)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		bool hasgraphics = !(flags & VK_QUEUE_GRAPHICS_BIT);
		bool hascompute = !(flags & VK_QUEUE_COMPUTE_BIT);
		bool haspresent = false;
		bool hastransfer = !(flags & VK_QUEUE_TRANSFER_BIT);
		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				hasgraphics = true;
			}
			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				hascompute = true;
			}
			if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				hastransfer = true;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport) {
				haspresent = true;
			}
			i++;
		}

		return hasgraphics && hascompute && haspresent && hastransfer;
	}

	int vkcoreDevice::GetQueueFamily(VkPhysicalDevice device, VkQueueFlags flags)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & flags) {
				return i;
			}
			i++;
		}

		return -1;
	}

	int vkcoreDevice::GetPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport) {
				return i;
			}
			i++;
		}

		return -1;
	}
}