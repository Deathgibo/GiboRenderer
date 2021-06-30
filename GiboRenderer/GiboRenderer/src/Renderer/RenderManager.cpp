#include "../pch.h"
#include "RenderManager.h"



namespace Gibo {

	//GLFW CALLBACK FUNCTIONS. Make sure glfwSetWindowUserPointer(WindowManager.Getwindow(), reinterpret_cast<void*>(this)) is called after creating window.
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		RenderManager * handler = reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window));
		handler->Recreateswapchain();
	}
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window))->GetInputManager().key_callback(window, key, scancode, action, mods);
	}
	static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
	{
		reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window))->GetInputManager().cursor_position_callback(window, xpos, ypos);
	}
	static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
	{
		reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window))->GetInputManager().mouse_button_callback(window, button, action, mods);
	}
	static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
	{
		reinterpret_cast<RenderManager *>(glfwGetWindowUserPointer(window))->GetInputManager().scroll_callback(window, xoffset, yoffset);
	}


	RenderManager::RenderManager()
	{
	}

	RenderManager::~RenderManager()
	{
		WindowManager.Terminate();
		Device.DestroyDevice();
	}

	bool RenderManager::InitializeRenderer()
	{
		bool valid = true;
		window_extent.width = 800;
		window_extent.height = 600;

		valid = valid && WindowManager.InitializeVulkan(window_extent.width, window_extent.height, &framebuffer_size_callback, key_callback, cursor_position_callback, mouse_button_callback, scroll_callback);
		//set the callbacks to come back to this instance of rendermanager
		glfwSetWindowUserPointer(WindowManager.Getwindow(), reinterpret_cast<void*>(this));

		valid = valid && Device.CreateDevice("GiboRenderer", WindowManager.Getwindow(), window_extent);
		
		return valid;
	}

	void RenderManager::Update()
	{
		glfwPollEvents();

		vkcoreBuffer buffer1;
		vkcoreImage image1;

		Device.CreateImage(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT, 100, 100, 1, 1, 1, VMA_MEMORY_USAGE_GPU_ONLY, 0, image1);
		Device.CreateBuffer(sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, buffer1);

		Device.DestroyBuffer(buffer1);
		Device.DestroyImage(image1);



		ShaderProgram program;
		std::vector<descriptorinfo> global_descriptors = {
			{"ViewProjBuffer", 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
		};
		std::vector<descriptorinfo> local_descriptors = {
				{"UniformBufferObject", 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
		};
		std::vector<shadersinfo> shaderinformation{
			{"Shaders/spv/pbrvert.spv", VK_SHADER_STAGE_VERTEX_BIT},
			{"Shaders/spv/pbrfrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
		};

		program.Create(Device.GetDevice(), 1, shaderinformation.data(), shaderinformation.size(), global_descriptors.data(), global_descriptors.size(),
			local_descriptors.data(), local_descriptors.size(), 100);

		std::vector<vkcoreBuffer> viewbuffer(1);
		std::vector<vkcoreBuffer> uniformbuffer(1);

		for (int i = 0; i < viewbuffer.size(); i++)
		{
			Device.CreateBuffer(sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, viewbuffer[i]);
			Device.CreateBuffer(sizeof(double), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, uniformbuffer[i]);
		}

		std::vector<std::vector<vkcoreBuffer>> buffers = {
			viewbuffer,
			uniformbuffer
		};

		std::vector<std::vector<uint64_t>> sizes = {
			{sizeof(float)},
			{sizeof(double)}
		};

		DescriptorHelper descriptor1(1);
		descriptor1.uniformbuffers = buffers;
		descriptor1.buffersizes = sizes;

		uint32_t id = 5;
		program.AddLocalDescriptor(id, descriptor1.uniformbuffers, descriptor1.buffersizes, descriptor1.imageviews, descriptor1.samplers, descriptor1.bufferviews);
		program.AddLocalDescriptor(id, descriptor1.uniformbuffers, descriptor1.buffersizes, descriptor1.imageviews, descriptor1.samplers, descriptor1.bufferviews);
		program.RemoveLocalDescriptor(id);
		
		for (int i = 0; i < viewbuffer.size(); i++)
		{
			Device.DestroyBuffer(viewbuffer[i]);
			Device.DestroyBuffer(uniformbuffer[i]);
		}

		program.CleanUp();
	}

	bool RenderManager::IsWindowOpen() const
	{
		return glfwWindowShouldClose(WindowManager.Getwindow());
	}

	void RenderManager::SetWindowTitle(std::string title) const
	{
		WindowManager.SetWindowTitle(title);
	}

}