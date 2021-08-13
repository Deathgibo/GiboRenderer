#pragma once
#include "../../pch.h"

namespace Gibo {

	/*
		Holds useful functions for printing out vulkan information
	*/

	void PrintVulkanAPIVersion(uint32_t curr_version)
	{
		uint32_t apiversion;
		if (vkEnumerateInstanceVersion(&apiversion) == VK_SUCCESS) {
			Logger::LogInfo("Vulkan Version Installed: ", VK_VERSION_MAJOR(apiversion), ".", VK_VERSION_MINOR(apiversion), ".", VK_VERSION_PATCH(apiversion), '\n');
			Logger::LogInfo("Vulkan Version Used: ", VK_VERSION_MAJOR(curr_version), ".", VK_VERSION_MINOR(curr_version), ".", VK_VERSION_PATCH(curr_version), '\n');
		}
	}

	void PrintVulkanExtentions()
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		Logger::Log("available vulkan extensions:\n");
		for (const auto& extension : extensions) {
			Logger::Log('\t', extension.extensionName, '\n');
		}
		Logger::Log('\n');
	}

	void PrintVulkanLayers()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		Logger::Log("available vulkan layers:\n");
		for (const auto& layerProperties : availableLayers)
		{
			Logger::Log('\t', layerProperties.layerName, '\n');
		}
	}

	void PrintPhysicalDeviceProperties(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);
		Logger::Log("-----Physical Device Properties-----\n");
		Logger::Log("apiVersion Supported: ", VK_VERSION_MAJOR(properties.apiVersion), ".", VK_VERSION_MINOR(properties.apiVersion), ".", VK_VERSION_PATCH(properties.apiVersion), " | ");
		Logger::Log("deviceID: ", properties.deviceID, " | ");
		Logger::Log("deviceName: ", properties.deviceName, " | ");

		std::string deviceType = " ";
		switch (properties.deviceType) {
		case 0: deviceType = "Other"; break;
		case 1: deviceType = "Integrated_GPU"; break;
		case 2: deviceType = "Discrete_GPU"; break;
		case 3: deviceType = "Virtual_GPU"; break;
		case 4: deviceType = "CPU"; break;
		}

		Logger::Log("deviceType: ", deviceType, " | ");
		Logger::Log("driverVersion: ", properties.driverVersion, " | ");
		Logger::Log("vendorID: ", properties.vendorID, " | ");
		Logger::Log("depth sample: ", properties.limits.framebufferDepthSampleCounts, " | ");
		Logger::Log("color sample: ", properties.limits.framebufferColorSampleCounts, " | ");
		Logger::Log("compute max local group count[3]: (", properties.limits.maxComputeWorkGroupSize[0],", ", properties.limits.maxComputeWorkGroupSize[1],", ", 
			         properties.limits.maxComputeWorkGroupSize[2], ") | "); //this is the max local xyz specified in compute shader
		Logger::Log("compute group invocations (x*y*z): ", properties.limits.maxComputeWorkGroupInvocations, " | "); //this is the max local size of x*y*z in shader
		Logger::Log("compute max dispatch workgroup count[3]: (", properties.limits.maxComputeWorkGroupCount[0],", ", properties.limits.maxComputeWorkGroupCount[1],", ", 
			        properties.limits.maxComputeWorkGroupCount[2], ") | "); //this is max local that can be dispatched for each x, y, z
		Logger::Log("compute shared memory size: ", properties.limits.maxComputeSharedMemorySize, " | "); 
		Logger::Log("max pushconstant size: ", properties.limits.maxPushConstantsSize, " | ");
		Logger::Log("max image array layers: ", properties.limits.maxImageArrayLayers, " | ");
		Logger::Log("max framebuffer dimensions: (", properties.limits.maxFramebufferWidth, ", ", properties.limits.maxFramebufferHeight, ") | ");
		Logger::Log("max image dimension: ", properties.limits.maxImageDimension2D, "\n");
		//theres a lot more properties.limits!
	}

	void PrintPhysicalDeviceFeatures(VkPhysicalDevice device)
	{
		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(device, &features);
		features.samplerAnisotropy;
		features.sampleRateShading;
		Logger::Log("-----Physical Device Features-----\n");
		Logger::Log("geometry shader:", features.geometryShader, " | ");
		Logger::Log("tesselation shader: ", features.tessellationShader, " | ");
		Logger::Log("sampler Anisotropy:", features.samplerAnisotropy, " | ");
		Logger::Log("sampler Rate Shading:", features.sampleRateShading, "\n");
	}

	void PrintPhysicalDeviceExtensions(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
		
		Logger::Log("Physical Device Extensions: \n");
		for (const auto& extension : extensions)
		{
			Logger::Log("\t", extension.extensionName, '\n');
		}
	}

	void PrintPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		uint32_t familycount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &familycount, nullptr);

		std::vector<VkQueueFamilyProperties> familyproperties(familycount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &familycount, familyproperties.data());
		Logger::Log("-----Physical Device Queue Family Properties-----\n");
		for (int p = 0; p < familyproperties.size(); p++) {
			Logger::Log("family property: ", p, " queues allowed: ", familyproperties[p].queueCount, ": ");
			if (familyproperties[p].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				Logger::Log(" graphics supported, ");
			}
			if (familyproperties[p].queueFlags & VK_QUEUE_COMPUTE_BIT) {
				Logger::Log(" compute supported, ");
			}
			if (familyproperties[p].queueFlags & VK_QUEUE_TRANSFER_BIT) {
				Logger::Log(" transfer supported, ");
			}
			if (familyproperties[p].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
				Logger::Log(" sparse binding supported, ");
			}
			if (familyproperties[p].queueFlags & VK_QUEUE_PROTECTED_BIT) {
				Logger::Log(" protected supported, ");
			}
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, p, surface, &presentSupport);
			if (presentSupport) {
				Logger::Log(" present supported, ");
			}
			Logger::Log("\n");
		}
	}

	void PrintPhysicalDeviceGroups(VkInstance Instance)
	{
		/*uint32_t groupcount = 0;
		vkEnumeratePhysicalDeviceGroups(Instance, &groupcount, nullptr);
		std::vector<VkPhysicalDeviceGroupProperties> groups(groupcount);
		vkEnumeratePhysicalDeviceGroups(Instance, &groupcount, groups.data());
		Logger::Log("-----Physical Device Groups-----\n");
		for (const auto& group : groups)
		{
			Logger::Log("group physical device count: ", group.physicalDeviceCount, ", ");
			Logger::Log("\n");
		}*/
	}

	//HEAPS are vram or dram. stored directly on gpu or on computer dram.
	void PrintPhysicalDeviceMemoryProperties(VkPhysicalDevice device)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

		Logger::Log("-----Physical Device Memory Properties-----\n");
		for (int i = 0; i < memProperties.memoryHeapCount; i++)
		{
			VkMemoryHeap heap = memProperties.memoryHeaps[i];
			
			Logger::Log("heap ", i, " ", heap.size, " bytes (");
			if (heap.flags & 0x00000001) { Logger::Log("VK_MEMORY_HEAP_DEVICE_LOCAL_BIT | "); }
			if (heap.flags & 0x00000002) { Logger::Log("VK_MEMORY_HEAP_MULTI_INSTANCE_BIT | "); }
			Logger::Log(")\n");
		}

		for (int i = 0; i < memProperties.memoryTypeCount; i++)
		{
			VkMemoryType type = memProperties.memoryTypes[i];
			Logger::Log("type ", i, " heap index ", type.heapIndex, " (");

			if (type.propertyFlags & 0x00000001) { Logger::Log("VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | "); }
			if (type.propertyFlags & 0x00000002) { Logger::Log("VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | "); }
			if (type.propertyFlags & 0x00000004) { Logger::Log("VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | "); }
			if (type.propertyFlags & 0x00000008) { Logger::Log("VK_MEMORY_PROPERTY_HOST_CACHED_BIT | "); }
			if (type.propertyFlags & 0x00000010) { Logger::Log("VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | "); }
			if (type.propertyFlags & 0x00000020) { Logger::Log("VK_MEMORY_PROPERTY_PROTECTED_BIT | "); }
			if (type.propertyFlags & 0x00000040) { Logger::Log("VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD | "); }
			if (type.propertyFlags & 0x00000080) { Logger::Log("VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD |"); }
			Logger::Log(")\n");
		}
	}
}