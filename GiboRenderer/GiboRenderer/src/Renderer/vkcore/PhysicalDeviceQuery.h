#pragma once
#include "../../Utilities/Logger.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Gibo {

	namespace PhysicalDeviceQuery {
		/*
			Holds Functions to query about physical device information like: format support, feature support, device limits, queuefamily info, and memory info
		*/

		/* Common uses for feature checking
		if you want to create a VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT for depth attachment this checks whether it can be made with this format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT.
		if you want to create a VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT for color attachment this checks whether it can be made with this format, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT.
		if you want to create a VK_DESCRIPTOR_TYPE_STORAGE_IMAGE this checks whether it can be made with this format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT.
		if you want to create a VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER this checks whether it can be made with this format, VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT.
		if you want to create a VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER this checks whether it can be made with this format, VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT.
		*/
		static bool CheckImageOptimalFormat(VkPhysicalDevice physicaldevice, VkFormat format, VkFormatFeatureFlagBits flagbits)
		{
			VkFormatProperties format_properties;
			vkGetPhysicalDeviceFormatProperties(physicaldevice, format, &format_properties);
			if (!(format_properties.optimalTilingFeatures & flagbits))
			{
				Logger::LogWarning(format, " format is not supported for an optimal Image.\n");
				return false;
			}
			return true;
		}

		static bool CheckImageLinearFormat(VkPhysicalDevice physicaldevice, VkFormat format, VkFormatFeatureFlagBits flagbits)
		{
			VkFormatProperties format_properties;
			vkGetPhysicalDeviceFormatProperties(physicaldevice, format, &format_properties);
			if (!(format_properties.linearTilingFeatures & flagbits))
			{
				Logger::LogWarning(format, " format is not supported for a linear Image.\n");
				return false;
			}
			return true;
		}

		static bool CheckBufferFormat(VkPhysicalDevice physicaldevice, VkFormat format, VkFormatFeatureFlagBits flagbits)
		{
			VkFormatProperties format_properties;
			vkGetPhysicalDeviceFormatProperties(physicaldevice, format, &format_properties);
			if (!(format_properties.bufferFeatures & flagbits))
			{
				Logger::LogWarning(format, " format is not supported for a buffer.\n");
				return false;
			}
			return true;
		}

		static VkPhysicalDeviceFeatures GetDeviceFeatures(VkPhysicalDevice physicaldevice)
		{
			VkPhysicalDeviceFeatures device_features;
			vkGetPhysicalDeviceFeatures(physicaldevice, &device_features);
			return device_features;
		}

		static VkPhysicalDeviceLimits GetDeviceLimits(VkPhysicalDevice physicaldevice)
		{
			VkPhysicalDeviceProperties device_properties;
			vkGetPhysicalDeviceProperties(physicaldevice, &device_properties);

			return device_properties.limits;
		}

		static int GetQueueFamily(VkPhysicalDevice device, VkQueueFlags flags)
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

		static int GetPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface)
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

		//memory
	}
}