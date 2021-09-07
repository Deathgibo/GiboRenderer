#pragma once

namespace Gibo {

	/* QUERY NOTES
	VkPhysicalDeviceLimits::timestampComputeAndGraphics is a boolean telling you whether the device supports these or not
	VkPhysicalDeviceLimits::timestampPeriod is the number of nanoseconds required for a timestamp query to be incremented by 1

	VkQueryType: VK_QUERY_TYPE_OCCLUSION - is for occlusion testing, I think it tells you how many fragments get ran or overwritten or something
	VK_QUERY_TYPE_PIPELINE_STATISTICS - this tells you how many invocations each pipeline stage gets ran like fragment, vertex. seems useful
	VK_QUERY_TYPE_TIMESTAMP - this is the one that gives you the time gpu takes.

	QueryResultsFlags: VK_QUERY_RESULT_64_BIT - recommended to avoid overflow because it records nanoseconds
	VK_QUERY_RESULT_WAIT_BIT - cpu waits for them to all get written, this isn't bad for big compute shaders
	VK_QUERY_RESULT_WITH_AVAILABILITY_BIT - it will see if its available for host giving you back a value
	VK_QUERY_RESULT_PARTIAL_BIT - not used for timestamps queries

	for frames in flight we can just wait until the fence for that frame is done, then we know the gpu is done and we can query from the previous current frame
	for queries it gives you back time_stamp increment values. Then the timestampPeriod gives you nano_seconds per increment. so nanoseconds are increments*timestampPeroid.

	For vkCmdTimestamp the timer value will not be written until all previously submitted commands for that commandbuffer reached the specified stage. It might not be 100% correct
	if you call it between multiple calls.
	VK_PIPELINE_STAGE_TOP_BIT
	VK_PIPELINE_STAGE_BOTTOM_BIT
	*/

	/*
	  This is a simple class for handling vulkan queries. Right now it only supports time_stamp queries. On creation it creates one querypool for each frame in flight,
	  it gives you a nice interface to name your query_names just add to the enum, and a nice function to get nanoseconds back.
	*/
	class QueryManager
	{
	public:
		enum QUERY_NAME : int { MAIN_PASS, POST_PROCESS, QUAD, GUI, SHADOW, DEPTH, REDUCE, CLUSTER, QUERY_COUNT };
	public:

		QueryManager(VkDevice device, VkPhysicalDevice physicaldevice, int framesinflight);
		~QueryManager() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		QueryManager(QueryManager const&) = delete;
		QueryManager(QueryManager&&) = delete;
		QueryManager& operator=(QueryManager const&) = delete;
		QueryManager& operator=(QueryManager&&) = delete;

		void CleanUp();
		void WriteTimeStamp(VkCommandBuffer cmd, VkPipelineStageFlagBits stage, int frame, QUERY_NAME name, bool first_query);
		void ResetQueries(VkCommandBuffer cmd, int frame, QUERY_NAME name);
		uint64_t GetNanoseconds(int frame, QUERY_NAME name);

		VkQueryPool GetQueryPool(int frame) { return querypool_timestamps[frame]; }
		std::array<uint32_t, 2> GetQueryPair(QUERY_NAME name, int frame) { return query_times[name][frame]; }
	private:
		std::vector<VkQueryPool> querypool_timestamps;
		std::unordered_map<QUERY_NAME, std::vector<std::array<uint32_t, 2>>> query_times; //maps query_name to pair of queries, one for each frame in flight

		float timestampperiod; //number of nanoseconds that pass for every timestamp tick
		VkDevice deviceref;
	};

}

