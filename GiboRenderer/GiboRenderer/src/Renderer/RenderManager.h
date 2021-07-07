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
	public:
		RenderManager();
		~RenderManager();

		bool InitializeRenderer();

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) = delete;
		RenderManager& operator=(RenderManager const&) = delete;
		RenderManager& operator=(RenderManager&&) = delete;
		

		//addobjects puts it in renderobjectmanager, adds it to descriptor sets thats not in use
		//remove objects, removes it from renderobject, and removes it from descriptor set after its not in use
		void Update();
		void Render();
		void Recreateswapchain() { Logger::Log("recreate swapchain\n"); }

		bool IsWindowOpen() const;
		void SetWindowTitle(std::string title) const;
		Input& GetInputManager() { return InputManager; }
	private:
		bool InitializeVulkan();
		void CreatePBR();
		void createPBRfinal();
		void CreateAtmosphere();
		void CreateCompute();
		void ReSubmitComputeCmd(int current_frame, int current_imageindex);

	private:
		Input InputManager; //1030 bytes
		vkcoreDevice Device; //8 bytes
		glfw_handler WindowManager; //16 bytes
		MeshCache* meshCache;
		TextureCache* textureCache;
		Atmosphere* atmosphere;
		LightManager* lightmanager;
		RenderObjectManager* objectmanager;

		VkExtent2D window_extent;

		//pbr
		VkRenderPass renderpass_pbr;
		RenderObject* teapotobjectpbr;
		RenderObject* spherepbr;
		Light light1;
		std::vector<vkcoreImage> color_attachment;
		std::vector<VkImageView> color_attachmentview;
		std::vector<vkcoreImage> depth_attachment;
		std::vector<VkImageView> depth_view;
		std::vector<VkFramebuffer> framebuffer_pbr;
		ShaderProgram program_pbr;
		vkcorePipeline pipeline_pbr;
		std::vector<VkCommandBuffer> cmdbuffer_pbr;

		//post-processing
		ShaderProgram program_compute;
		vkcorePipeline pipeline_compute;
		std::vector<VkCommandBuffer> cmdbuffer_compute;

		
		std::vector<vkcoreBuffer> pv_uniform;

		//submission synchronization
		std::vector<VkFence> inFlightFences; 
		std::vector<VkFence> swapimageFences;
		std::vector<VkSemaphore> semaphore_imagefetch;
		std::vector<VkSemaphore> semaphore_colorpass;
		std::vector<VkSemaphore> semaphore_computepass;

		int FRAMES_IN_FLIGHT = 3;
		int current_frame_in_flight = 0;
	};
}

