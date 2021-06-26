#pragma once
#include "vkcore/vkcoreDevice.h"
#include "glfw_handler.h"
#include "../Utilities/Input.h"

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
		
		void Update();
		void Recreateswapchain() { Logger::Log("recreate swapchain\n"); }

		bool IsWindowOpen() const;
		void SetWindowTitle(std::string title) const;
		Input& GetInputManager() { return InputManager; }
	private:

	private:
		Input InputManager; //1030 bytes
		glfw_handler WindowManager; //16 bytes
		vkcoreDevice Device; //8 bytes
	};
}

