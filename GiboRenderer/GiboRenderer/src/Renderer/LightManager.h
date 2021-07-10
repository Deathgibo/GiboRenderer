#pragma once
#include "Light.h"
#include "vkcore/vkcoreDevice.h"

namespace Gibo {

	/*
		Responsible for handling all the gpu memory for lights used by shader. For lights you create your own lights which holds its own cpu data.
		Then you have to submit light and the lightmanager holds a pointer to that lights cpu data. You have to make sure you remove light from manager
		before deleting though to avoid invalid pointer.
		The light manager holds 1 buffer for all the light data and 1 of those for each frame. It also does the same for the light count buffer.

		adding and removing lights works dynamically and uses a staging buffer. *it is important to note that lights have to be static, if you change the info on light
		it won't update (they will update if you add or remove a light). If I want dynamic lights I can just host map the memory.
	*/

	class LightManager
	{
	public:
		static constexpr int MAX_LIGHTS = 120; //this has to match what is in the pbr shader. TODO - any way to make this dynamic?

		LightManager(vkcoreDevice& device, int framesinflight) : deviceref(device) { CreateBuffers(framesinflight); };
		~LightManager() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		LightManager(LightManager const&) = delete;
		LightManager(LightManager&&) = delete;
		LightManager& operator=(LightManager const&) = delete;
		LightManager& operator=(LightManager&&) = delete;

		void CleanUp();
		void Update(int framecount);
		void PrintInfo();

		void AddLight(Light& light);
		void RemoveLight(Light& light);

		vkcoreBuffer GetLightBuffer(int framecount) const { return light_buffer[framecount]; }
		vkcoreBuffer GetLightCountBuffer(int framecount) const { return lightcounter_buffer[framecount]; }

	private:
		void CreateBuffers(int framesinflight);
		void UpdateBuffers(int framecount);
		void NotifyUpdate() { needs_updated = true; frames_updated = 0; }
	private:
		std::unordered_map<int, Light::lightparams*> light_map;
		int id_count = 0; // TODO - make more robust

		//gpu buffer
		std::vector<vkcoreBuffer> light_buffer; //you have 1 buffer that stores all the light data, and 1 of these for each frame in flight.
		std::vector<vkcoreBuffer> lightcounter_buffer; //one for each frame in flight
		vkcoreBuffer light_stagingbuffer;
		vkcoreBuffer lightcounter_stagingbuffer;

		vkcoreDevice& deviceref;

		bool needs_updated = false;
		int frames_updated = 0;
		size_t gpumemory_usage = 0;
	};

}

