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
		//preload mesh and texture data
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/uvsphere.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/teapot.fbx");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/cyborg/cyborg.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/cube.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/stanford-bunny.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/monkeyhead.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/torus.obj");

		vkcoreTexture defaulttexture = Renderer.GetTextureCache()->Get2DTexture("Images/missingtexture.png", true);
		vkcoreTexture uvmap2texture = Renderer.GetTextureCache()->Get2DTexture("Images/uvmap2.png", true);
		vkcoreTexture hexnormaltexture = Renderer.GetTextureCache()->Get2DTexture("Images/HexNormal.png", false);
		vkcoreTexture rocktexture = Renderer.GetTextureCache()->Get2DTexture("Images/Rocks.png", true);
		vkcoreTexture rocknormaltexture = Renderer.GetTextureCache()->Get2DTexture("Images/RocksHeight.png", false);
		vkcoreTexture bricktexture = Renderer.GetTextureCache()->Get2DTexture("Images/brickwall.jpg", true);
		vkcoreTexture bricknormaltexture = Renderer.GetTextureCache()->Get2DTexture("Images/brick_normalup.png", false);
		vkcoreTexture stonetexture = Renderer.GetTextureCache()->Get2DTexture("Images/stones.png", true);
		vkcoreTexture stonenormaltexture = Renderer.GetTextureCache()->Get2DTexture("Images/stones_NM_height.png", false);
		vkcoreTexture trianglenormaltexture = Renderer.GetTextureCache()->Get2DTexture("Images/Triangles.png", false);
		vkcoreTexture patternnormaltexture = Renderer.GetTextureCache()->Get2DTexture("Images/Pattern.png", false);
		vkcoreTexture concretetexture = Renderer.GetTextureCache()->Get2DTexture("Images/concrete_d.jpg", true);
		vkcoreTexture concretenormaltexture = Renderer.GetTextureCache()->Get2DTexture("Images/concrete_n.jpg", false);
		vkcoreTexture concretespeculartexture = Renderer.GetTextureCache()->Get2DTexture("Images/concrete_s.jpg", false);
		vkcoreTexture bucaneertexture = Renderer.GetTextureCache()->Get2DTexture("Images/bucaneers.png", true);
		vkcoreTexture windowtexture = Renderer.GetTextureCache()->Get2DTexture("Images/window1.png", true);
		vkcoreTexture grasstexture = Renderer.GetTextureCache()->Get2DTexture("Images/grass1.png", true);

		vkcoreTexture cyborgnormal = Renderer.GetTextureCache()->Get2DTexture("Models/cyborg/cyborg_normal.png", false);
		vkcoreTexture cyborgabledo = Renderer.GetTextureCache()->Get2DTexture("Models/cyborg/cyborg_diffuse.png", false);
		vkcoreTexture cyborgspecular = Renderer.GetTextureCache()->Get2DTexture("Models/cyborg/cyborg_specular.png", false);

		//OBJECTS
		RenderObject* teapot = new RenderObject(Renderer.GetDevice(), uvmap2texture, Renderer.GetFramesInFlight());
		teapot->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/teapot.fbx"));
		teapot->GetMaterial().SetRoughness(1.0f);
		teapot->GetMaterial().SetAlbedo(glm::vec3(1.00, 0.71, 0.29));
		teapot->GetMaterial().SetNormalMap(hexnormaltexture);
		teapot->SetTransformation(glm::vec3(-2, 2, -3), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);
		
		RenderObject* sphere = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		sphere->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/uvsphere.obj"));
		sphere->GetMaterial().SetAlbedoMap(concretetexture);
		sphere->GetMaterial().SetNormalMap(concretenormaltexture);
		sphere->GetMaterial().SetSpecularMap(concretespeculartexture);
		sphere->SetTransformation(glm::vec3(2, 2, -3), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);
		
		RenderObject* floor = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		floor->SetMesh(Renderer.GetMeshCache()->GetQuadMesh());
		floor->GetMaterial().SetAlbedoMap(bricktexture);
		floor->GetMaterial().SetNormalMap(bricknormaltexture);
		floor->GetMaterial().SetReflectance(.4);
		floor->GetMaterial().SetRoughness(0.4f);
		floor->SetTransformation(glm::vec3(0, 0, 0), glm::vec3(20, 20, 20), RenderObject::ROTATE_DIMENSION::XANGLE, -90);

		RenderObject* cyborg = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		cyborg->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/cyborg/cyborg.obj"));
		cyborg->GetMaterial().SetNormalMap(cyborgnormal);
		cyborg->GetMaterial().SetSpecularMap(cyborgspecular);
		cyborg->GetMaterial().SetAlbedoMap(cyborgabledo);
		cyborg->SetTransformation(glm::vec3(0, 0, 6), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);
		
		RenderObject* sphere_metal = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		sphere_metal->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/uvsphere.obj"));
		sphere_metal->GetMaterial().SetSilverMaterial();
		sphere_metal->SetTransformation(glm::vec3(-5, 14, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_stone = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		sphere_stone->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/uvsphere.obj"));
		sphere_stone->GetMaterial().SetAlbedoMap(stonetexture);
		sphere_stone->GetMaterial().SetNormalMap(stonenormaltexture);
		sphere_stone->SetTransformation(glm::vec3(5, 14, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_tri = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		sphere_tri->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/uvsphere.obj"));
		sphere_tri->GetMaterial().SetPlasticMaterial();
		sphere_tri->GetMaterial().SetNormalMap(trianglenormaltexture);
		sphere_tri->SetTransformation(glm::vec3(0, 14, -5), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_gold = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		sphere_gold->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/uvsphere.obj"));
		sphere_gold->GetMaterial().SetGoldMaterial();
		sphere_gold->GetMaterial().SetNormalMap(hexnormaltexture);
		sphere_gold->SetTransformation(glm::vec3(0, 14, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* cube = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		cube->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/cube.obj"));
		cube->GetMaterial().SetSilverMaterial();
		cube->GetMaterial().SetNormalMap(patternnormaltexture);
		cube->SetTransformation(glm::vec3(0, 14, 5), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* bunny = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		bunny->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/stanford-bunny.obj"));
		bunny->GetMaterial().SetSilverMaterial();
		bunny->SetTransformation(glm::vec3(4, 0, 2), glm::vec3(20, 20, 20), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* bucaneer = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		bucaneer->SetMesh(Renderer.GetMeshCache()->GetQuadMesh());
		bucaneer->GetMaterial().SetAlbedoMap(bucaneertexture);
		bucaneer->SetTransformation(glm::vec3(-2, 1, 2), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* window1 = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		window1->SetMesh(Renderer.GetMeshCache()->GetQuadMesh());
		window1->GetMaterial().SetAlbedoMap(windowtexture);
		window1->SetTransformation(glm::vec3(5, 3, 8), glm::vec3(2, 2, 2), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* window2 = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		window2->SetMesh(Renderer.GetMeshCache()->GetQuadMesh());
		window2->GetMaterial().SetAlbedoMap(windowtexture);
		window2->SetTransformation(glm::vec3(5, 3, 5), glm::vec3(2, 2, 2), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* monkey = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		monkey->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/monkeyhead.obj"));
		monkey->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		monkey->GetMaterial().SetAlpha(.7);
		monkey->SetTransformation(glm::vec3(10, 1, 10), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::YANGLE, 200);

		RenderObject* torus = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		torus->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/torus.obj"));
		torus->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		torus->SetTransformation(glm::vec3(10, 1, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 10);

		RenderObject* sphere_white = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		sphere_white->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/uvsphere.obj"));
		sphere_white->SetTransformation(glm::vec3(-4, 2, -2), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_white2 = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		sphere_white2->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/uvsphere.obj"));
		sphere_white2->GetMaterial().SetRoughness(.3);
		sphere_white2->SetTransformation(glm::vec3(-8, 2, 2), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* grass1 = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		grass1->SetMesh(Renderer.GetMeshCache()->GetQuadMesh());
		grass1->GetMaterial().SetAlbedoMap(grasstexture);
		grass1->SetTransformation(glm::vec3(-10, 3, 15), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::YANGLE, 0);

		RenderObject* grass2 = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		grass2->SetMesh(Renderer.GetMeshCache()->GetQuadMesh());
		grass2->GetMaterial().SetAlbedoMap(grasstexture);
		grass2->SetTransformation(glm::vec3(-10, 3, 12), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::YANGLE, -5);

		RenderObject* grass3 = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		grass3->SetMesh(Renderer.GetMeshCache()->GetQuadMesh());
		grass3->GetMaterial().SetAlbedoMap(grasstexture);
		grass3->SetTransformation(glm::vec3(-11, 3, 14), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::YANGLE, 10);

		RenderObject* grass4 = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
		grass4->SetMesh(Renderer.GetMeshCache()->GetQuadMesh());
		grass4->GetMaterial().SetAlbedoMap(grasstexture);
		grass4->SetTransformation(glm::vec3(-8, 3, 11), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::YANGLE, 5);

		Renderer.AddRenderObject(teapot, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(sphere, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(floor, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(cyborg, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(sphere_gold, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(sphere_tri, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(sphere_stone, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(sphere_metal, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(cube, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(bunny, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(bucaneer, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.AddRenderObject(window1, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.AddRenderObject(window2, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.AddRenderObject(monkey, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.AddRenderObject(grass1, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.AddRenderObject(grass2, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.AddRenderObject(grass3, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.AddRenderObject(grass4, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.AddRenderObject(torus, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(sphere_white, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(sphere_white2, RenderObjectManager::BIN_TYPE::REGULAR);

		//LIGHTS
		Light light1;
		light1.setColor(glm::vec4(.9, .4f, 0.4f, 1)).setFallOff(50).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(50).setPosition(glm::vec4(0, 6, 0, 1)).
			setInnerAngle(60).setOuterAngle(75).setType(Light::light_type::SPOT);
		Renderer.GetLightManager()->AddLight(light1);

		Light light2;
		light2.setColor(glm::vec4(.3, .7f, 0.4f, 1)).setFallOff(50).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(50).setPosition(glm::vec4(10, 6, 10, 1)).
			setInnerAngle(60).setOuterAngle(75).setType(Light::light_type::SPOT);
		Renderer.GetLightManager()->AddLight(light2);

		Camera camera1(glm::vec3(0, 4, 5), glm::vec3(0, 1, 0), 270.0f, 90.0f);

		float sunphi = 110.0f; //0-180
		float suntheta = 0.0f; //0-360
		
		glm::vec3 sphere_position(2, 2, -3);
		int sphere_right = 1;
		float torus_angle = 10;
		int spawn_time = 0;
		RenderObject* tmps_objects[10];
		Light* tmp_lights[10];
		int teapot_right = 1;
		float teapot_roughness = 1.0f;
		int metal_right = 1;
		float metal_anisotropy = .4;
		glm::vec4 light2_color(.4, .7, .4, 1);

		float fps = 60.0f;
		while (!Renderer.IsWindowOpen())
		{
			Timer t1("main", false);


			spawn_time++;
			if (spawn_time == 400)
			{
				for (int i = 0; i < 10; i++)
				{
					//spawn and submit
					tmps_objects[i] = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
					tmps_objects[i]->SetMesh(Renderer.GetMeshCache()->GetMesh("Models/cyborg/cyborg.obj"));
					tmps_objects[i]->GetMaterial().SetAlbedoMap(bricktexture);
					tmps_objects[i]->GetMaterial().SetRoughness(.3);
					tmps_objects[i]->SetTransformation(glm::vec3(i*2, 1, -10), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

					Renderer.AddRenderObject(tmps_objects[i], RenderObjectManager::BIN_TYPE::BLENDABLE);
					
					tmp_lights[i] = new Light();
					tmp_lights[i]->setColor(glm::vec4(.1, .4f, 0.8f, 1)).setFallOff(50).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(150).setPosition(glm::vec4(i * 2, 6, -10, 1)).
						setInnerAngle(20).setOuterAngle(25).setType(Light::light_type::SPOT);
					Renderer.GetLightManager()->AddLight(*tmp_lights[i]);
				}
			}

			Renderer.Update();
			

			if (spawn_time == 800)
			{
				//remove
				for (int i = 0; i < 10; i++)
				{
					Renderer.RemoveRenderObject(tmps_objects[i], RenderObjectManager::BIN_TYPE::BLENDABLE);

					Renderer.GetLightManager()->RemoveLight(*tmp_lights[i]);
					delete tmp_lights[i];
				}
				spawn_time = 0;
			}

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

			teapot_roughness += .005*teapot_right;
			if (teapot_roughness > 1.0) {
				teapot_right = -1;
			}
			if (teapot_roughness < 0.0) {
				teapot_right = 1;
			}
			sphere_white->GetMaterial().SetRoughness(teapot_roughness);
			sphere_white2->GetMaterial().SetReflectance(teapot_roughness);

			metal_anisotropy += .005*metal_right;
			if (metal_anisotropy > 1) {
				metal_right = -1;
			}
			if (metal_anisotropy < -1) {
				metal_right = 1;
			}
			sphere_metal->GetMaterial().SetAnisotropy(metal_anisotropy);

			//only update if you add/delete lights
			light2_color.x = ((int)((light2_color.x*255) + 1) % 255) / 255.0f;
			light2_color.y = ((int)((light2_color.y * 255) + 3) % 255) / 255.0f;
			light2.setColor(light2_color);

			sphere_position.x += .01*sphere_right;
			if (sphere_position.x > 15)
			{
				sphere_right = -1;
			}
			if (sphere_position.x < 0)
			{
				sphere_right = 1;
			}
			sphere->SetTransformation(sphere_position, glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

			torus_angle += .1;
			torus->SetTransformation(glm::vec3(10, 1, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, torus_angle);

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

		if (spawn_time >= 400)
		{
			//remove
			for (int i = 0; i < 10; i++)
			{
				Renderer.RemoveRenderObject(tmps_objects[i], RenderObjectManager::BIN_TYPE::BLENDABLE);

				Renderer.GetLightManager()->RemoveLight(*tmp_lights[i]);
				delete tmp_lights[i];
			}
			spawn_time = 0;
		}

		Renderer.RemoveRenderObject(teapot, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(floor, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(cyborg, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_gold, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_tri, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_stone, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_metal, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(cube, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(bunny, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(bucaneer, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(window1, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(window2, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(monkey, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(torus, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_white, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_white2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(grass1, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(grass2, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(grass3, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(grass4, RenderObjectManager::BIN_TYPE::BLENDABLE);

		Renderer.GetLightManager()->RemoveLight(light1);
		Renderer.GetLightManager()->RemoveLight(light2);
	}

	void cleanup()
	{
		std::cout << "cleaning\n";

		Renderer.ShutDownRenderer();
	}

private:
	RenderManager Renderer;
};