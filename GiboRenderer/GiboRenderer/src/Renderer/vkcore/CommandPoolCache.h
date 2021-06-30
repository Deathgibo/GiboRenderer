#pragma once
#include "../../pch.h"

namespace Gibo {
	/* 
		This class creates all the command pools we need and handles all of its lifetime memory.
		For example you can command buffers that will be created and used once (HELPER), reset every frame (DYNAMIC), or never change (STATIC).
		This class creates 3 seperate pools for these and also for each familyqueue. If the family queues are the shared it also shares those queues.
		Its now easy to query for a pre-allocated pool specifically for your need. 

		also on helper command buffers use VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT when you begin submitting cmd buffer
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT specifies that command buffers allocated from the pool will be short-lived, meaning that they will be reset or freed in a relatively short timeframe.
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows any command buffer allocated from a pool to be individually reset to the initial state

		helper pools are created with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
		dynamic pools are created with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT and VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
		static pools are created with no flags
	*/
	
	enum class POOL_TYPE : int32_t { STATIC, DYNAMIC, HELPER };
	enum class POOL_FAMILY : int32_t { GRAPHICS, TRANSFER, COMPUTE, PRESENT };

	class CommandPoolCache
	{
	public:
		CommandPoolCache(VkDevice device, VkPhysicalDevice physicaldevice, VkSurfaceKHR surface);
		~CommandPoolCache() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		CommandPoolCache(CommandPoolCache const&) = delete;
		CommandPoolCache(CommandPoolCache&&) = delete;
		CommandPoolCache& operator=(CommandPoolCache const&) = delete;
		CommandPoolCache& operator=(CommandPoolCache&&) = delete;

		void Cleanup();
		void PrintInfo() const;
		VkCommandPool GetCommandPool(POOL_TYPE pooltype, POOL_FAMILY familyqueue);
	private:
		std::array<std::array<VkCommandPool, 4>, 3> Cache;
		std::array<uint32_t, 4> family_indices;
		VkDevice deviceref;
	};
}

