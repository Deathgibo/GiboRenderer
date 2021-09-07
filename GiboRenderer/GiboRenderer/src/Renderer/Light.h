#pragma once
#include "glm/vec4.hpp"
#include "glm/vec3.hpp"
#include "glm/trigonometric.hpp"

namespace Gibo {

	/*
		class for holding light cpu data. It doesn't have any gpu data or any knowledge of the engine really.
		To use you have to make a light and submit it to the light manager. Then make sure* you remove it before deleting.
	*/

	class Light
	{
	public:
		friend class LightManager;

		struct lightparams
		{
			glm::vec4 position; //for spotlight and pointlight
			glm::vec4 color; //for all
			glm::vec4 direction; //for spotlight and directionallight

			float intensity; //for all
			float innerangle; //for spotlight
			float outerangle; //for spotlight
			float falloff; //for pointlight and spotlight

			float type;
			float cast_shadow = 0.0f;
			float atlas_index = -1;
			float a3;
		};

		enum class light_type : uint8_t { POINT, SPOT, DIRECTIONAL, FOCUSED_SPOT };
		static float convert_type_to_float(light_type val)
		{
			return (float)val;
		}
		static light_type convert_float_to_type(float val)
		{
			uint8_t tmp = (uint8_t)val;
			return (light_type)tmp;
		}
	public:
		Light() : lightmanager_id(-1){};
		Light(light_type val) : lightmanager_id(-1) { info.type = convert_type_to_float(val); }
		Light(lightparams params) : info(params), lightmanager_id(-1) {}
		~Light() { if (isSubmitted()) { Logger::LogError("You need to remove light from manager before deleting. enjoy your corrupted pixels!\n"); } }

		//Set for each parameter 
		Light& setPosition(glm::vec4 pos) { info.position = pos;   return *this; };
		Light& setColor(glm::vec4 col) { info.color = col;      return *this; };
		Light& setDirection(glm::vec4 dir) { info.direction = dir;  return *this; };
		Light& setIntensity(float val) { info.intensity = val;  return *this; };
		Light& setInnerAngle(float val) { info.innerangle = glm::radians(val); return *this; };
		Light& setOuterAngle(float val) { info.outerangle = glm::radians(val); return *this; };
		Light& setFallOff(float val) { info.falloff = val;     return *this; };
		Light& setType(light_type val) { info.type = convert_type_to_float(val);  return *this; };
		//Light& setCastShadow(bool val) { info.cast_shadow = (val) ? 1.0f : 0.0f;  return *this; };

		void Move(glm::vec3 val) { info.position += glm::vec4(val.x, val.y, val.z, 0.0); }

		//Get
		inline lightparams getParams() const { return info; }
		lightparams* getParamsPtr() { return &info; }
		inline bool isSubmitted() const { return (lightmanager_id == -1) ? false : true; }
	private:
	private:
		lightparams info;
		int lightmanager_id; //this is changed in the lightmanager class. It wil lbe -1 if its removed, and some other number if its in
	};

}