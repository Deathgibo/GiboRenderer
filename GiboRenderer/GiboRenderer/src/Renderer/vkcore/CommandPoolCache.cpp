#include "../../pch.h"
#include "CommandPoolCache.h"
#include "PhysicalDeviceQuery.h"

namespace Gibo {

	CommandPoolCache::CommandPoolCache(VkDevice device, VkPhysicalDevice physicaldevice, VkSurfaceKHR surface) : deviceref(device)
	{
		family_indices[0] = PhysicalDeviceQuery::GetQueueFamily(physicaldevice, VK_QUEUE_GRAPHICS_BIT);
		family_indices[1] = PhysicalDeviceQuery::GetQueueFamily(physicaldevice, VK_QUEUE_TRANSFER_BIT);
		family_indices[2] = PhysicalDeviceQuery::GetQueueFamily(physicaldevice, VK_QUEUE_COMPUTE_BIT);
		family_indices[3] = PhysicalDeviceQuery::GetPresentQueueFamily(physicaldevice, surface);

		VkCommandPoolCreateInfo static_info;
		static_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		static_info.flags = 0;
		static_info.queueFamilyIndex;

		VkCommandPoolCreateInfo dynamic_info;
		dynamic_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		dynamic_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		dynamic_info.queueFamilyIndex;

		VkCommandPoolCreateInfo helper_info;
		helper_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		helper_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		helper_info.queueFamilyIndex;


		int counter = 0;
		//create pools sharing if they have same family so we don't waste pools
		for (int i = 0; i < 4; i++)
		{
			bool pool_found = false;
			for (int j = 0; j < i; j++)
			{
				if (family_indices[i] == family_indices[j])
				{
					Cache[0][i] = Cache[0][j];
					Cache[1][i] = Cache[1][j];
					Cache[2][i] = Cache[2][j];
					pool_found = true;
					break;
				}
			}
			if (pool_found == false)
			{
				//create pool
				static_info.queueFamilyIndex = family_indices[i];
				dynamic_info.queueFamilyIndex = family_indices[i];
				helper_info.queueFamilyIndex = family_indices[i];
				char a;
				std::cin >> a;
				std::cin >> a;
				//VULKAN_CHECK(vkCreateCommandPool(deviceref, &static_info, nullptr, &Cache[0][i]), "creating static command pool");
				std::cin >> a;
				//VULKAN_CHECK(vkCreateCommandPool(deviceref, &dynamic_info, nullptr, &Cache[1][i]), "creating dynamic command pool");
				std::cin >> a;
				//VULKAN_CHECK(vkCreateCommandPool(deviceref, &helper_info, nullptr, &Cache[2][i]), "creating helper command pool");

				vkCreateCommandPool(deviceref, &dynamic_info, nullptr, &Cache[0][i]);
				vkCreateCommandPool(deviceref, &dynamic_info, nullptr, &Cache[1][i]);
				vkCreateCommandPool(deviceref, &dynamic_info, nullptr, &Cache[2][i]);
			}
		}

	}

	void CommandPoolCache::Cleanup()
	{
		for (int i = 0; i < 4; i++)
		{
			bool pool_found = false;
			for (int j = 0; j < i; j++)
			{
				//pool was shared and already delete
				if (family_indices[i] == family_indices[j])
				{
					pool_found = true;
					break;
				}
			}
			if (pool_found == false)
			{
				//delete pool
				vkDestroyCommandPool(deviceref, Cache[0][0], nullptr);
				vkDestroyCommandPool(deviceref, Cache[1][0], nullptr);
				vkDestroyCommandPool(deviceref, Cache[2][0], nullptr);
			}
		}
	}

	void CommandPoolCache::PrintInfo() const
	{
		int unique_pools = 0;
		for (int i = 0; i < 4; i++)
		{
			bool pool_found = false;
			for (int j = 0; j < i; j++)
			{
				if (family_indices[i] == family_indices[j])
				{
					pool_found = true;
					break;
				}
			}
			if (pool_found == false)
			{
				unique_pools += 3;
			}
		}
		Logger::Log("-----CommandPoolCache-----\n", "pools created: ", unique_pools, "\n");
	}

	VkCommandPool CommandPoolCache::GetCommandPool(POOL_TYPE pooltype, POOL_FAMILY familyqueue) 
	{
		return Cache[(int32_t)pooltype][(int32_t)familyqueue];
	}
}