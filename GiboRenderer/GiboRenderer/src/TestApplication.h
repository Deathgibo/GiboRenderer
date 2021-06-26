#pragma once

#include "Renderer/RenderManager.h"

using namespace Gibo;

class TestApplication
{
public:
	int WIDTH = 800;
	int HEIGHT = 640;

	void run()
	{
		init();
		mainloop();
		cleanup();
	}

	void init()
	{
		Timer t1("Initialize time", true);
		if (!Renderer.InitializeRenderer())
		{
			throw std::runtime_error("Couldn't create Renderer!");
		}
	}

	void mainloop()
	{
		float fps = 60.0f;
		while (!Renderer.IsWindowOpen())
		{
			Timer t1("main", false);
			Renderer.Update();
			
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_A))
			{
				Logger::LogError("LOL\n");
			}
			
			Sleep(8);
			fps = 1 / (t1.GetTime() / 1000.0f);
			std::string title = "GiboRenderer (Rasterizer)  fps: " + std::to_string((int)fps) + "ms";
			Renderer.SetWindowTitle(title);
		}

	}

	void cleanup()
	{
		std::cout << "cleaning\n";
	}

private:
	RenderManager Renderer;
};