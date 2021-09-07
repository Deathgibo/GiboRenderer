#pragma once

#include "Renderer/RenderManager.h"

using namespace Gibo;

class TestApplication2
{
public:

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
		//preload mesh and texture data
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/uvsphere.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/teapot.fbx");
	
		vkcoreTexture defaulttexture = Renderer.GetTextureCache()->Get2DTexture("Images/missingtexture.png", true);
		vkcoreTexture uvmap2texture = Renderer.GetTextureCache()->Get2DTexture("Images/uvmap2.png", true);
		
		//OBJECTS
		RenderObject* teapot = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), uvmap2texture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/teapot.fbx", teapot->GetMesh());
		teapot->SetTransformation(glm::vec3(5, 2, -3), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 30);

		RenderObject* teapot2 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), uvmap2texture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", teapot2->GetMesh());
		teapot2->SetTransformation(glm::vec3(1, 5, -3), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::XANGLE, 50);

		Renderer.AddRenderObject(teapot, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(teapot2, RenderObjectManager::BIN_TYPE::REGULAR);
		
		//LIGHTS POINTS FIRST THEN SPOTS
		glm::vec4 light3_pos(5, 3, -85, 1);
		Light* light3 = Renderer.GetLightManager()->CreateLight();
		light3->setColor(glm::vec4(.3, .3f, 0.3f, 1)).setFallOff(30).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(400).setPosition(light3_pos).
			setInnerAngle(60).setOuterAngle(75).setType(Light::light_type::POINT);
		Renderer.GetLightManager()->AddLight(*light3);
		Renderer.GetLightManager()->SetShadowCaster(*light3, true);

	
		Camera camera1(glm::vec3(0, 4, 5), glm::vec3(0, 1, 0), 270.0f, 90.0f);

		float sunphi = 110.0f; //0-180
		float suntheta = 0.0f; //0-360

		glm::vec3 sphere_position(2, 2, -3);
		int sphere_right = 1;
		float torus_angle = 10;
		int spawn_time = 0;

		const int ObjectCount = 40;
		RenderObject* tmps_objects[ObjectCount];
		Light* tmp_lights[ObjectCount];
		int teapot_right = 1;
		float teapot_roughness = 1.0f;
		int metal_right = 1;
		float metal_anisotropy = .4;
		glm::vec4 light2_color(.4, .7, .4, 1);

		float fps = 60.0f;
		while (!Renderer.IsWindowOpen())
		{
			Timer t1("main", false);

			Renderer.Update();


			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_ESCAPE))
			{
				glfwSetWindowShouldClose(Renderer.GetWindowManager().Getwindow(), GLFW_TRUE);
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_KP_7)) {
				sunphi -= .1;
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_KP_8)) {
				sunphi += .1;
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_KP_4)) {
				suntheta -= .2;
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_KP_5)) {
				suntheta += .2;
			}

			camera1.handlekeyinput(Renderer.GetInputManager().GetKeys(), fps);
			camera1.handlemouseinput(Renderer.GetInputManager().GetKeys(), fps);
			Renderer.SetCameraInfo(camera1.CreateLookAtFunction(), camera1.GetPosition());

			glm::vec3 sunpos;
			sunpos.x = cos(glm::radians(suntheta))*sin(glm::radians(sunphi));
			sunpos.z = sin(glm::radians(sunphi))*sin(glm::radians(suntheta));
			sunpos.y = cos(glm::radians(sunphi));
			sunpos = glm::normalize(sunpos);

			Renderer.UpdateSunPosition(glm::vec4(sunpos, 1));

			fps = 1 / (t1.GetTime() / 1000.0f);
			std::string title = "GiboRenderer (Rasterizer)  fps: " + std::to_string((int)fps) + "ms";
			Renderer.SetWindowTitle(title);
		}

		Renderer.RemoveRenderObject(teapot, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(teapot2, RenderObjectManager::BIN_TYPE::REGULAR);
		
		Renderer.GetLightManager()->RemoveLight(*light3);
	}

	void cleanup()
	{
		std::cout << "cleaning...\n";

		Renderer.ShutDownRenderer();

		std::cout << "cleaning done\n";
	}

private:
	RenderManager Renderer;
};