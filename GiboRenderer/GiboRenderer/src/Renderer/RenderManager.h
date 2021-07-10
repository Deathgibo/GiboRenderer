#pragma once
#include "vkcore/vkcoreDevice.h"
#include "vkcore/ShaderProgram.h"
#include "glfw_handler.h"
#include "../Utilities/Input.h"
#include "AssetManager.h"
#include "TextureCache.h"
#include "Atmosphere.h"
#include "LightManager.h"
#include "RenderObjectManager.h"

namespace Gibo {

	class RenderManager
	{
		enum SAMPLE_COUNT { ONE, TWO, FOUR, EIGHT, SIXTEEN, THIRTYTWO };
		enum RESOLUTION { THREESIXTYP, FOUREIGHTYP, SEVENTWENTYP, TENEIGHTYP, FOURTEENFORTYP, FOURKP };
		VkSampleCountFlagBits ConvertSampleCount(SAMPLE_COUNT x)
		{
			switch (x)
			{
			case ONE: return VK_SAMPLE_COUNT_1_BIT; break;
			case TWO: return VK_SAMPLE_COUNT_2_BIT; break;
			case FOUR: return VK_SAMPLE_COUNT_4_BIT; break;
			case EIGHT: return VK_SAMPLE_COUNT_8_BIT; break;
			case SIXTEEN: return VK_SAMPLE_COUNT_32_BIT; break;
			case THIRTYTWO: return VK_SAMPLE_COUNT_64_BIT; break;
			}

			return VK_SAMPLE_COUNT_1_BIT;
		}

		void ConvertResolution(RESOLUTION res, int& width, int& height)
		{
			switch (res)
			{
			case THREESIXTYP:	 width = 640;  height = 360;  break;
			case FOUREIGHTYP:	 width = 854;  height = 480;  break;
			case SEVENTWENTYP:   width = 1280; height = 720;  break;
			case TENEIGHTYP:	 width = 1920; height = 1080; break; 
			case FOURTEENFORTYP: width = 2560; height = 1440; break;
			case FOURKP:		 width = 3840; height = 2160; break;
			}
		}

	public:
		struct descriptorgraveinfo
		{
			uint32_t id;
			int frame_count;

			descriptorgraveinfo(uint32_t id_, int frame_count_) : id(id_), frame_count(frame_count_) {};
		};
	public:
		RenderManager();
		~RenderManager();

		bool InitializeRenderer();
		void ShutDownRenderer();

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) = delete;
		RenderManager& operator=(RenderManager const&) = delete;
		RenderManager& operator=(RenderManager&&) = delete;
		

		void AddRenderObject(RenderObject* object, RenderObjectManager::BIN_TYPE type);
		void RemoveRenderObject(RenderObject* object, RenderObjectManager::BIN_TYPE type);

		void Update();
		void Render();
		void Recreateswapchain();
		void SetMultisampling(SAMPLE_COUNT count); 
		void SetResolution(RESOLUTION res);
		void SetFullScreen(bool val) { WindowManager.SetFullScreen(1920, 1080); };
		void SetCameraInfo(glm::mat4 cammatrix, glm::vec3 campos) {
			cam_matrix = cammatrix;
			atmosphere->UpdateCamPosition(glm::vec4(campos, 1));
		}

		void UpdateSunPosition(glm::vec4 pos) {
			atmosphere->UpdateSunPosition(pos);
		}

		bool IsWindowOpen() const;
		void SetWindowTitle(std::string title) const;
		Input& GetInputManager() { return InputManager; }
		glfw_handler GetWindowManager() { return WindowManager; }
		vkcoreDevice* GetDevice() { return &Device; }
		int GetFramesInFlight() { return FRAMES_IN_FLIGHT; }
		MeshCache* GetMeshCache() { return meshCache; }
		TextureCache* GetTextureCache() { return textureCache; }
		LightManager* GetLightManager() { return lightmanager; }
		RenderObjectManager* GetObjectManager() { return objectmanager; }
	private:
		void CreatePBR();
		void createPBRfinal();
		void CleanUpPBR();
		void PBRdeleteimagedata();
		void PBRcreateimagedata();
		void CleanUpCompute();
		void CleanUpGeneral();
		void CreateAtmosphere();
		void CreateCompute();
		void RecordComputeCmd(int current_frame, int current_imageindex);
		void RecordPBRCmd(int current_frame);
		void UpdateDescriptorGraveYard();
		void SortBlendedObjects();
		void SetProjectionMatrix();
		void SetCameraMatrix();
		void startImGui();
		void updateImGui(int current_frame, int imageindex);
		void closeImGui();

	private:
		Input InputManager; //1030 bytes
		vkcoreDevice Device; //8 bytes
		glfw_handler WindowManager; //16 bytes
		MeshCache* meshCache;
		TextureCache* textureCache;
		Atmosphere* atmosphere;
		LightManager* lightmanager;
		RenderObjectManager* objectmanager;

		bool enable_imgui = true;
		VkExtent2D window_extent{ 800, 640 };
		VkSampleCountFlagBits multisampling_count = VK_SAMPLE_COUNT_1_BIT;
		VkExtent2D Resolution = { 800, 640 };
		float FOV = 45.0f;
		float near_plane = 0.01f;
		float far_plane = 1000.0f;

		//pbr
		VkRenderPass renderpass_pbr;

		std::vector<vkcoreImage> color_attachment;
		std::vector<VkImageView> color_attachmentview;
		std::vector<vkcoreImage> depth_attachment;
		std::vector<VkImageView> depth_view;
		std::vector<vkcoreImage> resolve_attachment;
		std::vector<VkImageView> resolve_attachmentview;

		std::vector<VkFramebuffer> framebuffer_pbr;
		ShaderProgram program_pbr;
		vkcorePipeline pipeline_pbr;
		std::vector<VkCommandBuffer> cmdbuffer_pbr;

		//post-processing
		ShaderProgram program_compute;
		vkcorePipeline pipeline_compute;
		std::vector<VkCommandBuffer> cmdbuffer_compute;

		//imgui
		VkDescriptorPool imguipool;
		VkRenderPass renderpass_imgui;
		std::vector<VkCommandBuffer> cmdbuffer_imgui;
		std::vector<VkFramebuffer> framebuffer_imgui;


		glm::mat4 proj_matrix;
		glm::mat4 cam_matrix;
		glm::vec3 cam_position;
		std::vector<vkcoreBuffer> pv_uniform;

		//submission synchronization
		std::vector<VkFence> inFlightFences; 
		std::vector<VkFence> swapimageFences;
		std::vector<VkSemaphore> semaphore_imagefetch;
		std::vector<VkSemaphore> semaphore_colorpass;
		std::vector<VkSemaphore> semaphore_computepass;
		std::vector<VkSemaphore> semaphore_imgui;

		int FRAMES_IN_FLIGHT = 3;
		int current_frame_in_flight = 0;

		std::vector<descriptorgraveinfo> descriptorgraveyard;

	};
}

