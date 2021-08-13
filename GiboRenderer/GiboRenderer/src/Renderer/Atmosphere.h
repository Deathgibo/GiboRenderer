#pragma once
#include "vkcore/vkcoreDevice.h"
#include "AssetManager.h"
#include "RenderObject.h"
#include "vkcore/ShaderProgram.h"

namespace Gibo {

	class Atmosphere
	{
	public:
		struct atmosphere_info
		{
			glm::vec3 earth_center;
			float image_width;
			glm::vec3 sun_intensity;
			float image_height;

			float image_depth;
			float atmosphere_height;
			float earth_radius; //surface height from center of earth
		};

		struct atmosphere_shader
		{
			glm::vec4 campos;
			glm::vec4 camdirection;
			glm::vec4 earth_center;
			glm::vec4 lightdir;
			glm::vec4 farplane;
			float earth_radius;
			float atmosphere_height;
		};

	public:
		Atmosphere(vkcoreDevice& device, vkcoreTexture texture, int framesinflight) : deviceref(device), Sky_Mesh(&device, texture, framesinflight), gatheredLUT(K), gatheredView(K), MiegatheredLUT(K), MiegatheredView(K) {};
		~Atmosphere() = default;

		void CleanUp();
		void Create(VkExtent2D proj_extent, VkExtent2D pipeline_extent, float fov, VkRenderPass renderpass, MeshCache& mcache, int framesinflight, std::vector<vkcoreBuffer> pv_uniform, VkSampleCountFlagBits sample_count);
		void FillLUT();
		void Draw(VkCommandBuffer cmdbuffer, int current_frame);

		//let me do system wehre you have an update function and each of these just make it dirty
		void Update(int framecount);
		void UpdateCamPosition(glm::vec4 pos) { shader_info.campos = pos; NotifyUpdate(); }
		void UpdateSunPosition(glm::vec4 pos) { shader_info.lightdir = pos; NotifyUpdate();}
		void UpdateDebug(glm::vec4 pos) { shader_info.camdirection = pos; NotifyUpdate(); }
		void UpdateFarPlane(VkExtent2D window_extent, float fov);

		void SwapChainRecreate(VkExtent2D pipeline_extent, VkExtent2D proj_extent, float fov, VkRenderPass renderpass, VkSampleCountFlagBits sample_count);

		glm::vec3 GetSunDirection() { return shader_info.lightdir; }
		VkImageView GetAmbientView() { return ambientview; }
		VkImageView GetTransmittanceView() { return transmittanceview; }
		VkSampler GetSampler() { return atmospheresampler; }
		vkcoreBuffer Getshaderinfobuffer(int x) { return shaderinfo_buffer[x]; }
	private:
		void BindAtmosphereBuffer(int x);
		void NotifyUpdate() { needs_updated = true; frames_updated = 0; }
		void CreateSwapChainData(VkExtent2D pipeline_extent, VkRenderPass renderpass, VkSampleCountFlagBits sample_count);

	private:
		//cpu data
		atmosphere_info info;
		atmosphere_shader shader_info;
		
		//gpu data
		std::vector<vkcoreBuffer> shaderinfo_buffer;
		vkcoreBuffer info_buffer;
		vkcoreBuffer skymatrix_buffer;

		//variables
		static constexpr int K = 3;
		int ambient_width = 256;
		int ambient_height = 256;
		int lut_width = 32;
		int lut_height = 256;
		int lut_depth = 32;
		glm::ivec3 WORKGROUP_SIZE = glm::ivec3(1, 256, 1);
		glm::ivec3 WORKGROUP_SIZE_AMBIENT = glm::ivec3(32, 32, 1);

		//vulkan handles
		RenderObject Sky_Mesh;

		vkcoreImage atmosphereLUT;
		VkImageView atmosphereview;
		VkSampler atmospheresampler;

		vkcoreImage MieLUT;
		VkImageView Mieview;

		std::vector<vkcoreImage> gatheredLUT;
		std::vector<VkImageView> gatheredView;
		std::vector<vkcoreImage> MiegatheredLUT;
		std::vector<VkImageView> MiegatheredView;

		vkcoreImage ambientLUT;
		VkImageView ambientview;

		vkcoreImage transmittanceLUT;
		VkImageView transmittanceview;

		ShaderProgram program_sky;
		ShaderProgram program_singlescatter;
		ShaderProgram program_multiscatter;
		ShaderProgram program_combine;
		ShaderProgram program_ambient;

		vkcorePipeline pipeline_sky;
		vkcorePipeline pipeline_singlescatter;
		vkcorePipeline pipeline_multiscatter;
		vkcorePipeline pipeline_combine;
		vkcorePipeline pipeline_ambient;

		VkCommandBuffer cmdbuffer_singlescatter;
		VkCommandBuffer cmdbuffer_multiscatter;
		VkCommandBuffer cmdbuffer_combine;
		VkCommandBuffer cmdbuffer_ambient;

		vkcoreDevice& deviceref;

		bool needs_updated = false;
		int frames_updated = 0;
	};


}


