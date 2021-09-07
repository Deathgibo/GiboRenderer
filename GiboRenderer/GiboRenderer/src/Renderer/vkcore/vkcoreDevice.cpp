#include "../../pch.h"
#include <GLFW/glfw3.h>
#include "vkcoreDevice.h"
#include "vkcorePrintHelper.h"
#include "PhysicalDeviceQuery.h"
#include "VulkanHelpers.h"
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

	uint32_t API_VERSION = VK_API_VERSION_1_1;

	void vkcoreDevice::DestroyDevice()
	{
		vmaDestroyAllocator(Allocator);

		cmdpoolCache->Cleanup();
		delete cmdpoolCache;

		pipelineCache->Cleanup();
		delete pipelineCache;

		samplerCache->Cleanup();
		delete samplerCache;

		renderpassCache->Cleanup();
		delete renderpassCache;

		querymanager->CleanUp();
		delete querymanager;

		for (int i = 0; i < swapChainImageViews.size(); i++)
		{
			vkDestroyImageView(LogicalDevice, swapChainImageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(LogicalDevice, SwapChain, nullptr);
		vkDestroyDevice(LogicalDevice, nullptr);
		vkDestroySurfaceKHR(Instance, Surface, nullptr);
		vkDestroyInstance(Instance, nullptr);
	}

	bool vkcoreDevice::CreateDevice(std::string name, GLFWwindow* window, VkExtent2D& window_extent, VkExtent2D& Resolution, int framesinflight)
	{
		bool valid = true;

		valid = valid && CreateVulkanInstance(name);
		valid = valid && CreateSurface(window);
		valid = valid && PickPhysicalDevice();
		valid = valid && CreateLogicalDevice();
		valid = valid && CreateSwapChain(window_extent, framesinflight);
		valid = valid && CreateAllocator();

		renderpassCache = new RenderPassCache(LogicalDevice);
		samplerCache = new SamplerCache(LogicalDevice);
		pipelineCache = new PipelineCache(LogicalDevice);
		cmdpoolCache = new CommandPoolCache(LogicalDevice, PhysicalDevice, Surface);
		char a;
		std::cin >> a;
		cmdpoolCache->PrintInfo();
		querymanager = new QueryManager(LogicalDevice, PhysicalDevice, framesinflight);

		return valid;
	}

	bool vkcoreDevice::CreateAllocator()
	{
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.vulkanApiVersion = API_VERSION;
		allocatorInfo.physicalDevice = PhysicalDevice;
		allocatorInfo.device = LogicalDevice;
		allocatorInfo.instance = Instance;
		allocatorInfo.pHeapSizeLimit;
		allocatorInfo.preferredLargeHeapBlockSize;
		//allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

		auto result = vmaCreateAllocator(&allocatorInfo, &Allocator);
		VULKAN_CHECK(result, "creating vma allocator");
		if (result != VK_SUCCESS)
		{
			Logger::LogError("Failed to create vma allocator!\n");
			return false;
		}

		return true;
	}

	bool vkcoreDevice::CreateVulkanInstance(std::string name)
	{
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
		uint32_t graphicsfamily = PhysicalDeviceQuery::GetQueueFamily(PhysicalDevice, VK_QUEUE_GRAPHICS_BIT);
		uint32_t computefamily = PhysicalDeviceQuery::GetQueueFamily(PhysicalDevice, VK_QUEUE_COMPUTE_BIT);
		uint32_t transferfamily = PhysicalDeviceQuery::GetQueueFamily(PhysicalDevice, VK_QUEUE_TRANSFER_BIT);
		uint32_t presentfamily = PhysicalDeviceQuery::GetPresentQueueFamily(PhysicalDevice, Surface);

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
		VkPhysicalDeviceHostQueryResetFeatures resetFeatures = {};
		resetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
		resetFeatures.pNext = nullptr;
		resetFeatures.hostQueryReset = VK_TRUE;

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
		deviceFeatures.imageCubeArray = VK_TRUE;

		if (features.geometryShader == FALSE) { Logger::LogWarning("device doesn't have geometryShader"); }
		if (features.tessellationShader == FALSE) { Logger::LogWarning("device doesn't have tessellationShader"); }
		if (features.fillModeNonSolid == FALSE) { Logger::LogWarning("device doesn't have fillModeNonSolid"); }
		if (features.sampleRateShading == FALSE) { Logger::LogWarning("device doesn't have sampleRateShading"); }
		if (features.samplerAnisotropy == FALSE) { Logger::LogWarning("device doesn't have samplerAnisotropy"); }
		if (features.fragmentStoresAndAtomics == FALSE) { Logger::LogWarning("device doesn't have fragmentStoresAndAtomics"); }
		if (features.imageCubeArray == FALSE) { Logger::LogWarning("device doesn't have imageCubeArray"); }

		//pick which physical device extension we need to support
		const std::vector<const char*> deviceExtensions = {
			"VK_KHR_swapchain"
		};

		VkDeviceCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		info.pNext = &resetFeatures; //nullptr
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

	void vkcoreDevice::CleanSwapChain()
	{
		//swapchain
		vkDestroySwapchainKHR(LogicalDevice, SwapChain, nullptr);
		for (int i = 0; i < swapChainImageViews.size(); i++)
		{
			vkDestroyImageView(LogicalDevice, swapChainImageViews[i], nullptr);
		}
		swapChainImageViews.clear();
	}

	bool vkcoreDevice::CreateSwapChain(VkExtent2D& window_extent, int framesinflight)
	{
		//Presentation mode
		//IMMEDIATE: images are just immediately replacing previous ones. screen tearing happens
		//FIFO: *must be supported. sends them to a qeueu that are displayed in sync (vsync)
		//FIFO RELAXED:  like fifo but no vsync so screen tearing will happen if frames are coming in too slow
		//Mailbox: triple buffering
		//check if physical device supports swap chain and enable extension in logical device
		Logger::Log("-----SWAPCHAIN INFO-----\n");

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

		if (formatCount == 0 || presentCount == 0)
		{
			Logger::LogError("swap chain doesnt have formats or presentmodes");
			return false;
		}

		//Pick format and color space
		VkFormat desired_format = VK_FORMAT_R16G16B16A16_SFLOAT; //VK_FORMAT_R8G8B8A8_UINT
		VkColorSpaceKHR desired_colorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		VkSurfaceFormatKHR thesurfaceformat;

		// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED there is no preferred format
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		{
			thesurfaceformat.format = desired_format;
			thesurfaceformat.colorSpace = desired_colorspace;
		}
		else
		{
			bool resultfound = false;
			//look for desried format and color space
			for (int i = 0; i < formats.size(); i++)
			{
				if (formats[i].format == desired_format && formats[i].colorSpace == desired_colorspace)
				{
					thesurfaceformat = formats[i];
					resultfound = true;
					break;
				}
			}
			//look for just desired format
			if (!resultfound)
			{
				for (int i = 0; i < formats.size(); i++)
				{
					if (formats[i].format == desired_format)
					{
						thesurfaceformat = formats[i];
						resultfound = true;
						break;
					}
				}

				//just use first available one
				if (!resultfound)
				{
					thesurfaceformat = formats[0];
				}
			}
		}
		Logger::Log("desired format: ", desired_format, " format chosen: ", thesurfaceformat.format, " desired color space: ", desired_colorspace, " color space chosen: ", thesurfaceformat.colorSpace, "\n");

		//Pick present mode is just an enum about how the swap chain handles swapping the images, it can be vsync, triple buffer, single, etc
		VkPresentModeKHR desired_present_mode = VK_PRESENT_MODE_MAILBOX_KHR; //VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR
		VkPresentModeKHR thesurfacepresentmode;
		for (int i = 0; i < presentmodes.size(); i++)
		{
			if (presentmodes[i] == desired_present_mode)
			{
				thesurfacepresentmode = presentmodes[i];
				break;
			}

			thesurfacepresentmode = VK_PRESENT_MODE_FIFO_KHR; //FIFO must be present so use as default if we can't find desired present mode
		}
		Logger::Log("desired present mode: ", desired_present_mode, " present mode selected: ", thesurfacepresentmode, "\n");
		//Pick size of swapchain images
		//if its 0xFFFFFFFF that means the window is based off swapchain size. So tell swapchain what we want our size to be. Else size is just what we set the window to be.
		VkExtent2D the_extent;
		if (capabilities.currentExtent.width == 0xFFFFFFFF)
		{
			the_extent = window_extent;
			//clamp to min/max sizes
			if (the_extent.height < capabilities.minImageExtent.height) {
				the_extent.height = capabilities.minImageExtent.height;
			}
			else if (the_extent.height > capabilities.maxImageExtent.height) {
				the_extent.height = capabilities.maxImageExtent.height;
			}

			if (the_extent.width < capabilities.minImageExtent.width) {
				the_extent.width = capabilities.minImageExtent.width;
			}
			else if (the_extent.width > capabilities.maxImageExtent.width) {
				the_extent.width = capabilities.maxImageExtent.width;
			}
		}
		else
		{
			the_extent = capabilities.currentExtent;
		}
		window_extent.width = the_extent.width;
		window_extent.height = the_extent.height;
		Logger::Log("size of swapchain images: (", window_extent.width, ", ", window_extent.height, ")\n");

		//Pick how many images to create
		uint32_t desired_image_count = framesinflight;
		uint32_t minimum_image_count = capabilities.minImageCount + 1;
		if (minimum_image_count < desired_image_count && desired_image_count < capabilities.maxImageCount)
		{
			minimum_image_count = desired_image_count;
		}
		Logger::Log("image count: ", minimum_image_count, "\n");

		//Pick desired image usage. COLOR_ATTACMENT has to be here. Adding more can hurt performance so try and use only when needed
		VkImageUsageFlags desired_image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;// | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; //VK_IMAGE_USAGE_TRANSFER_DST_BIT for post-processing
		VkImageUsageFlags image_usage = 0;

		if (desired_image_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT && capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if (desired_image_usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT && capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		{
			image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (desired_image_usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT && capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		{
			image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if (desired_image_usage & VK_IMAGE_USAGE_STORAGE_BIT && capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
		{
			image_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
		}
		else
		{
			//Logger::LogError("swap chain format doesn't support storage bit\n");
		}

		//Pick desired transform
		VkSurfaceTransformFlagBitsKHR desired_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		VkSurfaceTransformFlagBitsKHR the_transform;
		if (capabilities.supportedTransforms & desired_transform)
		{
			the_transform = desired_transform;
		}
		else
		{
			the_transform = capabilities.currentTransform;
		}
		Logger::LogInfo("transformation: ", the_transform, "\n");

		// Pick a supported composite alpha format (not all devices support alpha opaque)
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		// Simply select the first composite alpha format available
		std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (auto& compositeAlphaFlag : compositeAlphaFlags) {
			if (capabilities.supportedCompositeAlpha & compositeAlphaFlag) {
				compositeAlpha = compositeAlphaFlag;
				break;
			};
		}

		VkSwapchainCreateInfoKHR swapinfo = {};
		swapinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapinfo.pNext = nullptr;
		swapinfo.flags = 0;
		swapinfo.surface = Surface;
		swapinfo.minImageCount = minimum_image_count;
		swapinfo.imageFormat = thesurfaceformat.format;
		swapinfo.imageColorSpace = thesurfaceformat.colorSpace;
		swapinfo.imageExtent = the_extent;
		swapinfo.imageArrayLayers = 1;
		swapinfo.imageUsage = image_usage;
		swapinfo.preTransform = the_transform;
		swapinfo.compositeAlpha = compositeAlpha;
		swapinfo.presentMode = thesurfacepresentmode;
		swapinfo.clipped = VK_TRUE;
		swapinfo.oldSwapchain = VK_NULL_HANDLE;

		//Pick queue families. We need to draw using graphics and present
		int graphics_family = PhysicalDeviceQuery::GetQueueFamily(PhysicalDevice, VK_QUEUE_GRAPHICS_BIT);
		int present_family = PhysicalDeviceQuery::GetPresentQueueFamily(PhysicalDevice, Surface);
		uint32_t queueFamilyIndices[] = { graphics_family, present_family };
		if (graphics_family != present_family) {
			swapinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapinfo.queueFamilyIndexCount = 2;
			swapinfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			swapinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		Logger::Log("swap chain family count: ", swapinfo.queueFamilyIndexCount, " imagesharingmode: ", swapinfo.imageSharingMode, "\n");

		VkResult result = vkCreateSwapchainKHR(LogicalDevice, &swapinfo, nullptr, &SwapChain);
		VULKAN_CHECK(result, "creating swapchain");
		if (result != VK_SUCCESS) {
			Logger::LogError("swapchain failed to create\n");
			return false;
		}

		//get the swap chain images
		uint32_t imagecount;
		vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &imagecount, nullptr);
		swapChainImages.resize(imagecount);
		vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &imagecount, swapChainImages.data());

		for (int i = 0; i < swapChainImages.size(); i++)
		{
			swapChainImageViews.push_back(CreateImageView(LogicalDevice, swapChainImages[i], swapinfo.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, VK_IMAGE_VIEW_TYPE_2D));
		}

		SwapChainFormat = thesurfaceformat.format;

		return true;
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

	/*
		Memory tips

		Render targets make sure to use device_local so it can be stored on gpu

		Immutable resources that change infrequently should first create staging buffer then transfer it to gpu. Even keep the staging buffer around for more transfers. use device_local

		Dynamic resources change every frame, put on cpu_to_gpu.

		readback use gpu_to_cpu.

		https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html more info

	*/
	void vkcoreDevice::BindData(VmaAllocation allocation, void* data, size_t size)
	{
		void* mappedData;
		VULKAN_CHECK(vmaMapMemory(Allocator, allocation, &mappedData), "mapping memory");
		memcpy(mappedData, data, size);
		vmaUnmapMemory(Allocator, allocation);
	}

	//data had to be created with VMA_ALLOCATION_CREATE_MAPPED_BIT flag! 
	void vkcoreDevice::BindDataAlwaysMapped(void* mapped_ptr, void* data, size_t size)
	{
		if (mapped_ptr != nullptr)
		{
			memcpy(mapped_ptr, data, size);
		}
	}

	void* vkcoreDevice::GetBufferData(VmaAllocation allocation, size_t size)
	{
		void* mappedData;
		VULKAN_CHECK(vmaMapMemory(Allocator, allocation, &mappedData), "mapping memory");
		vmaUnmapMemory(Allocator, allocation);
		return mappedData;
	}
	void vkcoreDevice::UnMapBuffer(VmaAllocation allocation)
	{
		vmaUnmapMemory(Allocator, allocation);
	}

	VmaAllocationInfo vkcoreDevice::GetAllocationInfo(VmaAllocation allocation)
	{
		VmaAllocationInfo info;
		vmaGetAllocationInfo(Allocator, allocation, &info);
		return info;
	}

	bool vkcoreDevice::CreateImage(VkImageType image_type, VkFormat Format, VkImageUsageFlags usage, VkSampleCountFlagBits samplecount, uint32_t width, uint32_t height, uint32_t depth,
		uint32_t mip_levels, uint32_t array_layers, VmaMemoryUsage memusage, VmaAllocationCreateFlags mapped_bit_flag, vkcoreImage& vkimage)
	{
		//check to see if image is supported
#ifdef _DEBUG
		VkImageFormatProperties d;
		if (vkGetPhysicalDeviceImageFormatProperties(GetPhysicalDevice(), Format, image_type, VK_IMAGE_TILING_OPTIMAL, usage,
			0, &d) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		{
			Logger::LogError("Image not supported by physical device!\n");
		}
#endif 


		//fill imagecreateinfo, fill vmamemoryusage, create image
		VkImageCreateInfo imageInfo = {};
		imageInfo.flags = (array_layers % 6 == 0) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = image_type;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = depth;
		imageInfo.mipLevels = mip_levels;
		imageInfo.arrayLayers = array_layers;
		imageInfo.format = Format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = samplecount;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocationinfo = {};
		allocationinfo.usage = memusage;
		allocationinfo.requiredFlags;
		allocationinfo.preferredFlags;
		allocationinfo.flags = mapped_bit_flag;
		allocationinfo.memoryTypeBits;

		VmaAllocationInfo mappedinfo;
		VkResult result = vmaCreateImage(Allocator, &imageInfo, &allocationinfo, &vkimage.image, &vkimage.allocation, &mappedinfo);
		if (allocationinfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
		{
			vkimage.mapped_data = mappedinfo.pMappedData;
		}
		VULKAN_CHECK(result, "creating image");
		if (result != VK_SUCCESS)
		{
			return false;
		}

		image_allocations++;
		return true;
	}

	void vkcoreDevice::DestroyImage(vkcoreImage& vkimage)
	{
		image_allocations--;
		vmaDestroyImage(Allocator, vkimage.image, vkimage.allocation);
	}

	bool vkcoreDevice::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memusage, VmaAllocationCreateFlags mapped_bit_flag, vkcoreBuffer& vkbuffer)
	{
		//first fill vkbuffercreateinfo, then fill vmaallocationcreateinfo, then createbuffer
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.size = size;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices;

		VmaAllocationCreateInfo allocationinfo = {};
		allocationinfo.usage = memusage;
		allocationinfo.requiredFlags;
		allocationinfo.preferredFlags;
		allocationinfo.flags = mapped_bit_flag;
		allocationinfo.memoryTypeBits;


		VmaAllocationInfo mappedinfo;
		VkResult result = vmaCreateBuffer(Allocator, &info, &allocationinfo, &vkbuffer.buffer, &vkbuffer.allocation, &mappedinfo);
		if (allocationinfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
		{
			vkbuffer.mapped_data = mappedinfo.pMappedData;
		}
		VULKAN_CHECK(result, "creating buffer");
		if (result != VK_SUCCESS)
		{
			return false;
		}

		buffer_allocations++;
		return true;
	}

	bool vkcoreDevice::CreateBufferStaged(VkDeviceSize size, void* data, VkBufferUsageFlags usage, VmaMemoryUsage memusage, vkcoreBuffer& vkbuffer,
		VkAccessFlagBits dstacces, VkPipelineStageFlagBits dststage)
	{
		bool valid = true;
		if (memusage != VMA_MEMORY_USAGE_GPU_ONLY)
		{
			Logger::LogWarning("Staging a buffer that isn't gpu_only?\n");
		}

		//create buffers
		vkcoreBuffer stagingbuffer;
		valid = valid && CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, stagingbuffer);
		valid = valid && CreateBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, memusage, 0, vkbuffer);
		
		//map and fill staging
		BindData(stagingbuffer.allocation, data, size);

		//copy over
		CopyBufferToBuffer(*this, stagingbuffer.buffer, vkbuffer.buffer, dstacces, dststage, 0, 0, size);

		//free staging buffer todo - maybe you want to store this if your doing every other frame?
		DestroyBuffer(stagingbuffer);

		buffer_allocations++;
		return valid;
	}

	void vkcoreDevice::DestroyBuffer(vkcoreBuffer& vkbuffer)
	{
		buffer_allocations--;
		vmaDestroyBuffer(Allocator, vkbuffer.buffer, vkbuffer.allocation);
	}


	VkCommandBuffer vkcoreDevice::beginSingleTimeCommands(POOL_FAMILY familyoperation)
	{
		if (cmdpoolCache)
		{
			VkCommandBuffer buffer;
			
			VkCommandBufferAllocateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			info.commandPool = cmdpoolCache->GetCommandPool(POOL_TYPE::HELPER, familyoperation);
			info.commandBufferCount = 1;

			VULKAN_CHECK(vkAllocateCommandBuffers(LogicalDevice, &info, &buffer), "allocating one time cmd");
			
			VkCommandBufferBeginInfo begininfo = {};
			begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			
			vkBeginCommandBuffer(buffer, &begininfo);
			return buffer;
		}
		return VK_NULL_HANDLE;
	}

	void vkcoreDevice::submitSingleTimeCommands(VkCommandBuffer buffer, POOL_FAMILY familyoperation)
	{
		if (cmdpoolCache)
		{
			VULKAN_CHECK(vkEndCommandBuffer(buffer), "ending one time command");

			VkSubmitInfo submitinfo = {};
			submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitinfo.pCommandBuffers;
			submitinfo.commandBufferCount = 1;
			submitinfo.pCommandBuffers = &buffer;

			VkQueue queue = VK_NULL_HANDLE;
			if (familyoperation == POOL_FAMILY::GRAPHICS)
			{
				queue = GraphicsQueue;
			}
			else if (familyoperation == POOL_FAMILY::TRANSFER)
			{
				queue = TransferQueue;
			}
			else if (familyoperation == POOL_FAMILY::COMPUTE)
			{
				queue = ComputeQueue;
			}
			else if (familyoperation == POOL_FAMILY::PRESENT)
			{
				queue = PresentQueue;
			}
			VkFence fence;
			VkFenceCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			vkCreateFence(LogicalDevice, &info, nullptr, &fence);

			VULKAN_CHECK(vkQueueSubmit(queue, 1, &submitinfo, fence), "submitting one time command");

			vkWaitForFences(LogicalDevice, 1, &fence, VK_TRUE, UINT32_MAX);
			vkDestroyFence(LogicalDevice, fence, nullptr);

			vkFreeCommandBuffers(LogicalDevice, cmdpoolCache->GetCommandPool(POOL_TYPE::HELPER, familyoperation), 1, &buffer);
		}
	}

}