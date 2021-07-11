#include "../../pch.h"
#include "QueryManager.h"
#include "PhysicalDeviceQuery.h"

namespace Gibo {

	QueryManager::QueryManager(VkDevice device, VkPhysicalDevice physicaldevice, int framesinflight) : deviceref(device)
	{
		if (PhysicalDeviceQuery::GetDeviceLimits(physicaldevice).timestampComputeAndGraphics != VK_TRUE)
		{
			Logger::LogWarning("physical device does not support time stamps\n");
			return;
		}
		timestampperiod = PhysicalDeviceQuery::GetDeviceLimits(physicaldevice).timestampPeriod;
		Logger::Log("Time stamp period is: ", timestampperiod, "\n");
		
		int query_max = 20;
		VkQueryPoolCreateInfo poolinfo = {};
		poolinfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		poolinfo.flags;
		poolinfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		poolinfo.queryCount = query_max;
		poolinfo.pipelineStatistics;

		querypool_timestamps.resize(framesinflight);
		for (int i = 0; i < framesinflight; i++)
		{
			vkCreateQueryPool(device, &poolinfo, nullptr, &querypool_timestamps[i]);
		}

		//initialize all queries for all types
		for (int i = 0; i < QUERY_NAME::QUERY_COUNT; i++)
		{
			query_times[(QUERY_NAME)i].resize(framesinflight);
		}
		//give each query unique names within its pool
		for (int i = 0; i < framesinflight; i++)
		{
			uint32_t unique_id = 0;
			for (int j = 0; j < QUERY_NAME::QUERY_COUNT; j++)
			{
				query_times[(QUERY_NAME)j][i][0] = unique_id;
				unique_id++;
				query_times[(QUERY_NAME)j][i][1] = unique_id;
				unique_id++;
			}
		}

		/* keep here for debugging
		for (int i = 0; i < QUERY_NAME::QUERY_COUNT; i++)
		{
			for (int j = 0; j < framesinflight; j++)
			{
				Logger::LogError("BIN: ", i, " frame: ", j, " index: ", 0, " value: ", query_times[(QUERY_NAME)i][j][0], "\n");
				Logger::LogError("BIN: ", i, " frame: ", j, " index: ", 1, " value: ", query_times[(QUERY_NAME)i][j][1], "\n");
			}
		}
		*/
	}

	void QueryManager::CleanUp()
	{
		for (int i = 0; i < querypool_timestamps.size(); i++)
		{
			vkDestroyQueryPool(deviceref, querypool_timestamps[i], nullptr);
		}
	}

	void QueryManager::WriteTimeStamp(VkCommandBuffer cmd, VkPipelineStageFlagBits stage, int frame, QUERY_NAME name, bool first_query)
	{
		std::array<uint32_t, 2> query_times = GetQueryPair(name, frame);
		vkCmdWriteTimestamp(cmd, stage, querypool_timestamps[frame], (first_query) ? query_times[0] : query_times[1]);
	}

	uint64_t QueryManager::GetNanoseconds(int frame, QUERY_NAME name)
	{
		std::array<uint32_t, 2> query_times = GetQueryPair(name, frame);
		std::array<uint64_t, 2> query_data;
	
		VULKAN_CHECK(vkGetQueryPoolResults(deviceref, querypool_timestamps[frame], query_times[0], 2, sizeof(uint64_t) * 2, query_data.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT), "getting time_stamp result");

		uint64_t timestamp_increments = query_data[1] - query_data[0];
		uint64_t timestamp_nanoseconds = timestamp_increments * timestampperiod;

		return timestamp_nanoseconds;
	}

	void QueryManager::ResetQueries(VkCommandBuffer cmd, int frame, QUERY_NAME name)
	{
		std::array<uint32_t, 2> query_times = GetQueryPair(name, frame);
		vkCmdResetQueryPool(cmd, querypool_timestamps[frame], query_times[0], 1);
		vkCmdResetQueryPool(cmd, querypool_timestamps[frame], query_times[1], 1);
	}
}