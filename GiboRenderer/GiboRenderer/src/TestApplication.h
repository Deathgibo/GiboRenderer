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
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/cyber/0.stl");

		Renderer.GetMeshCache()->LoadMeshFromFile("Models/fence.fbx");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/fence2.fbx");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/mount.blend1.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/fence_01_obj.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/mountain.obj");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/Stair.fbx");
		Renderer.GetMeshCache()->LoadMeshFromFile("Models/house.obj");

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
		RenderObject* teapot = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), uvmap2texture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/teapot.fbx", teapot->GetMesh());
		teapot->GetMaterial().SetRoughness(1.0f);
		teapot->GetMaterial().SetAlbedo(glm::vec3(1.00, 0.71, 0.29));
		teapot->GetMaterial().SetNormalMap(hexnormaltexture);
		teapot->SetTransformation(glm::vec3(-2, 2, -3), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* teapot2 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), uvmap2texture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/teapot.fbx", teapot2->GetMesh());
		teapot2->GetMaterial().SetRoughness(1.0f);
		teapot2->GetMaterial().SetAlbedo(glm::vec3(1.00, 0.71, 0.29));
		teapot2->GetMaterial().SetNormalMap(hexnormaltexture);
		teapot2->SetTransformation(glm::vec3(-2, 4, -87), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);
		
		RenderObject* sphere = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", sphere->GetMesh());
		sphere->GetMaterial().SetAlbedoMap(concretetexture);
		sphere->GetMaterial().SetNormalMap(concretenormaltexture);
		sphere->GetMaterial().SetSpecularMap(concretespeculartexture);
		sphere->SetTransformation(glm::vec3(2, 2, -3), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);
		
		RenderObject* floor = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(floor->GetMesh());
		//floor->GetMaterial().SetAlbedoMap(bricktexture);
		//floor->GetMaterial().SetNormalMap(bricknormaltexture);
		floor->GetMaterial().SetReflectance(.4);
		floor->GetMaterial().SetRoughness(0.4f);
		floor->SetTransformation(glm::vec3(0, 0, 0), glm::vec3(100, 100, 100), RenderObject::ROTATE_DIMENSION::XANGLE, -90);

		RenderObject* plane1 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(plane1->GetMesh());
		plane1->GetMaterial().SetAlbedo(glm::vec3(.3, .3, .8));
		plane1->SetTransformation(glm::vec3(-40, 1, 0), glm::vec3(7, 7, 7), RenderObject::ROTATE_DIMENSION::XANGLE, -90);

		RenderObject* plane2 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(plane2->GetMesh());
		plane2->GetMaterial().SetAlbedo(glm::vec3(.3, .3, .8));
		plane2->SetTransformation(glm::vec3(-40, 13, 0), glm::vec3(7, 7, 7), RenderObject::ROTATE_DIMENSION::XANGLE, -20);

		RenderObject* plane3 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(plane3->GetMesh());
		plane3->GetMaterial().SetAlbedo(glm::vec3(.3, .3, .8));
		plane3->SetTransformation(glm::vec3(-40, 1, 20), glm::vec3(7, 7, 7), RenderObject::ROTATE_DIMENSION::XANGLE, -90);

		RenderObject* plane4 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(plane4->GetMesh());
		plane4->GetMaterial().SetAlbedo(glm::vec3(.3, .3, .8));
		plane4->SetTransformation(glm::vec3(-40, 5, 30), glm::vec3(7, 7, 7), RenderObject::ROTATE_DIMENSION::XANGLE, -10);

		RenderObject* spherebig = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", spherebig->GetMesh());
		spherebig->GetMaterial().SetAlbedoMap(stonetexture);
		spherebig->SetTransformation(glm::vec3(-180, 25, -100), glm::vec3(50, 50, 50), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* cyborg = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/cyborg/cyborg.obj", cyborg->GetMesh());
		cyborg->GetMaterial().SetNormalMap(cyborgnormal);
		cyborg->GetMaterial().SetSpecularMap(cyborgspecular);
		cyborg->GetMaterial().SetAlbedoMap(cyborgabledo);
		cyborg->SetTransformation(glm::vec3(0, 0, 6), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* fence1 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/fence.fbx", fence1->GetMesh());
		fence1->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		fence1->SetTransformation(glm::vec3(5, 2, 40), glm::vec3(10, 10, 10), RenderObject::ROTATE_DIMENSION::XANGLE, -70);


		RenderObject* fence2 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/fence2.fbx", fence2->GetMesh());
		fence2->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		fence2->SetTransformation(glm::vec3(50, 15, 35), glm::vec3(15, 15, 15), RenderObject::ROTATE_DIMENSION::XANGLE, -90);

		RenderObject* fence3 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/mount.blend1.obj", fence3->GetMesh());
		fence3->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		fence3->SetTransformation(glm::vec3(100, 5, -80), glm::vec3(15, 15, 15), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* fence4 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/fence_01_obj.obj", fence4->GetMesh());
		fence4->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		fence4->SetTransformation(glm::vec3(0, 10, 10), glm::vec3(.01, .01, .01), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* fence5 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/fence_01_obj.obj", fence5->GetMesh());
		fence5->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		fence5->SetTransformation(glm::vec3(-40, 0, 40), glm::vec3(.001, .001, .001), RenderObject::ROTATE_DIMENSION::YANGLE, 90);

		RenderObject* fence6 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/fence_01_obj.obj", fence6->GetMesh());
		fence6->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		fence6->SetTransformation(glm::vec3(-40, 0, 43), glm::vec3(.001, .001, .001), RenderObject::ROTATE_DIMENSION::YANGLE, 90);

		RenderObject* fence7 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/fence_01_obj.obj", fence7->GetMesh());
		fence7->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		fence7->SetTransformation(glm::vec3(-40, 0, 46), glm::vec3(.001, .001, .001), RenderObject::ROTATE_DIMENSION::YANGLE, 90);

		RenderObject* stairs = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/Stair.fbx", stairs->GetMesh());
		stairs->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		stairs->SetTransformation(glm::vec3(-60, .1, 40), glm::vec3(.01, 0.01, 0.01), RenderObject::ROTATE_DIMENSION::YANGLE, 180);

		RenderObject* stairs2 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/Stair.fbx", stairs2->GetMesh());
		stairs2->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		stairs2->SetTransformation(glm::vec3(-60, .1, 50), glm::vec3(.005, .005, .005), RenderObject::ROTATE_DIMENSION::YANGLE, 180);

		RenderObject* stairs3 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/Stair.fbx", stairs3->GetMesh());
		stairs3->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		stairs3->SetTransformation(glm::vec3(-60, .1, 30), glm::vec3(.04, .04, .04), RenderObject::ROTATE_DIMENSION::YANGLE, 180);
		
		RenderObject* mountain1 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/mountain.obj", mountain1->GetMesh());
		mountain1->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		mountain1->SetTransformation(glm::vec3(-60, 1, 70), glm::vec3(2, 2, 2), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_metal = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", sphere_metal->GetMesh());
		sphere_metal->GetMaterial().SetSilverMaterial();
		sphere_metal->SetTransformation(glm::vec3(-5, 14, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_stone = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", sphere_stone->GetMesh());
		sphere_stone->GetMaterial().SetAlbedoMap(stonetexture);
		sphere_stone->GetMaterial().SetNormalMap(stonenormaltexture);
		sphere_stone->SetTransformation(glm::vec3(5, 14, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_tri = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", sphere_tri->GetMesh());
		sphere_tri->GetMaterial().SetPlasticMaterial();
		sphere_tri->GetMaterial().SetNormalMap(trianglenormaltexture);
		sphere_tri->SetTransformation(glm::vec3(0, 14, -5), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_gold = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", sphere_gold->GetMesh());
		sphere_gold->GetMaterial().SetGoldMaterial();
		sphere_gold->GetMaterial().SetNormalMap(hexnormaltexture);
		sphere_gold->SetTransformation(glm::vec3(0, 14, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* cube = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/cube.obj", cube->GetMesh());
		cube->GetMaterial().SetSilverMaterial();
		cube->GetMaterial().SetNormalMap(patternnormaltexture);
		cube->SetTransformation(glm::vec3(0, 14, 5), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* bunny = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/stanford-bunny.obj", bunny->GetMesh());
		bunny->GetMaterial().SetSilverMaterial();
		bunny->SetTransformation(glm::vec3(4, 0, 2), glm::vec3(20, 20, 20), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* bucaneer = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(bucaneer->GetMesh());
		bucaneer->GetMaterial().SetAlbedoMap(bucaneertexture);
		bucaneer->SetTransformation(glm::vec3(-2, 1, 2), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* window1 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(window1->GetMesh());
		window1->GetMaterial().SetAlbedoMap(windowtexture);
		window1->SetTransformation(glm::vec3(5, 3, 8), glm::vec3(2, 2, 2), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* window2 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(window2->GetMesh());
		window2->GetMaterial().SetAlbedoMap(windowtexture);
		window2->SetTransformation(glm::vec3(5, 3, 5), glm::vec3(2, 2, 2), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* monkey = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/monkeyhead.obj", monkey->GetMesh());
		monkey->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		monkey->GetMaterial().SetAlpha(.7);
		monkey->SetTransformation(glm::vec3(10, 1, 10), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::YANGLE, 200);

		RenderObject* torus = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/torus.obj", torus->GetMesh());
		torus->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		torus->SetTransformation(glm::vec3(10, 3, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 10);

		RenderObject* sphere_white = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", sphere_white->GetMesh());
		sphere_white->SetTransformation(glm::vec3(-4, 2, -2), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_light = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", sphere_light->GetMesh());
		sphere_light->SetTransformation(glm::vec3(-4, 2, -2), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* sphere_white2 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/uvsphere.obj", sphere_white2->GetMesh());
		sphere_white2->GetMaterial().SetRoughness(.3);
		sphere_white2->SetTransformation(glm::vec3(-8, 2, 2), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

		RenderObject* grass1 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(grass1->GetMesh());
		grass1->GetMaterial().SetAlbedoMap(grasstexture);
		grass1->SetTransformation(glm::vec3(-10, 3, 15), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::YANGLE, 0);

		RenderObject* grass2 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(grass2->GetMesh());
		grass2->GetMaterial().SetAlbedoMap(grasstexture);
		grass2->SetTransformation(glm::vec3(-10, 3, 12), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::YANGLE, -5);

		RenderObject* grass3 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(grass3->GetMesh());
		grass3->GetMaterial().SetAlbedoMap(grasstexture);
		grass3->SetTransformation(glm::vec3(-11, 3, 14), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::YANGLE, 10);

		RenderObject* grass4 = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetQuadMesh(grass4->GetMesh());
		grass4->GetMaterial().SetAlbedoMap(grasstexture);
		grass4->SetTransformation(glm::vec3(-8, 3, 11), glm::vec3(3, 3, 3), RenderObject::ROTATE_DIMENSION::YANGLE, 5);

		RenderObject* tower = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/cyber/0.stl", tower->GetMesh());
		tower->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		tower->SetTransformation(glm::vec3(0, 3, -15), glm::vec3(15.008, 15.008, 15.008), RenderObject::ROTATE_DIMENSION::XANGLE, -90);

		RenderObject* house = Renderer.GetObjectManager()->CreateRenderObject(Renderer.GetDevice(), rocktexture);
		Renderer.GetMeshCache()->SetObjectMesh("Models/house.obj", house->GetMesh());
		house->GetMaterial().SetAlbedo(glm::vec3(.8, .8, .8));
		house->SetTransformation(glm::vec3(0, 0, -65), glm::vec3(0.05, 0.05, 0.05), RenderObject::ROTATE_DIMENSION::XANGLE, -90);
		
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
		Renderer.AddRenderObject(sphere_light, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(sphere_white2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(tower, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(fence1, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(fence2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(fence3, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(fence4, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(fence5, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(fence6, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(fence7, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(plane1, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(plane2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(plane3, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(plane4, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(spherebig, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(mountain1, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(stairs, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(stairs2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(stairs3, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(house, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.AddRenderObject(teapot2, RenderObjectManager::BIN_TYPE::REGULAR);

		//LIGHTS POINTS FIRST THEN SPOTS
		Light* light1 = Renderer.GetLightManager()->CreateLight();
		light1->setColor(glm::vec4(.3, .4f, 0.9f, 1)).setFallOff(10).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(150).setPosition(glm::vec4(4, 4, 2, 1)).
			setInnerAngle(15).setOuterAngle(45).setType(Light::light_type::SPOT);
		Renderer.GetLightManager()->AddLight(*light1);
		Renderer.GetLightManager()->SetShadowCaster(*light1, true);

		Light* light2 = Renderer.GetLightManager()->CreateLight();
		light2->setColor(glm::vec4(.8, .4f, 0.4f, 1)).setFallOff(10).setDirection(glm::vec4(1, -1, 0, 1)).setIntensity(1000).setPosition(glm::vec4(-46, 5, 40, 1)).
			setInnerAngle(15).setOuterAngle(30).setType(Light::light_type::SPOT);
		Renderer.GetLightManager()->AddLight(*light2);
		Renderer.GetLightManager()->SetShadowCaster(*light2, true);

		glm::vec4 light3_pos(5, 3, -85, 1);
		Light* light3 = Renderer.GetLightManager()->CreateLight();
		light3->setColor(glm::vec4(.3, .3f, 0.3f, 1)).setFallOff(30).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(400).setPosition(light3_pos).
			setInnerAngle(60).setOuterAngle(75).setType(Light::light_type::POINT);
		Renderer.GetLightManager()->AddLight(*light3);
		Renderer.GetLightManager()->SetShadowCaster(*light3, true);

		Light* lightlol = Renderer.GetLightManager()->CreateLight();
		lightlol->setColor(glm::vec4(.6, .1f, 0.6f, 1)).setFallOff(30).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(400).setPosition(glm::vec4(-30,10,10,1)).
			setInnerAngle(60).setOuterAngle(75).setType(Light::light_type::POINT);
		Renderer.GetLightManager()->AddLight(*lightlol);
		Renderer.GetLightManager()->SetShadowCaster(*lightlol, true);

		Light* light4 = Renderer.GetLightManager()->CreateLight();
		light4->setColor(glm::vec4(.4, .8f, 0.4f, 1)).setFallOff(50).setDirection(glm::vec4(1, -1, 0, 1)).setIntensity(1000).setPosition(glm::vec4(-46, 5, 45, 1)).
			setInnerAngle(15).setOuterAngle(30).setType(Light::light_type::SPOT);
		//Renderer.GetLightManager()->AddLight(*light4);

		Light* light5 = Renderer.GetLightManager()->CreateLight();
		light5->setColor(glm::vec4(.8, .8f, 0.4f, 1)).setFallOff(50).setDirection(glm::vec4(-1, -1, 0, 1)).setIntensity(1000).setPosition(glm::vec4(2, 5, -4, 1)).
			setInnerAngle(15).setOuterAngle(30).setType(Light::light_type::SPOT);
		//Renderer.GetLightManager()->AddLight(*light5);

		Light* light6 = Renderer.GetLightManager()->CreateLight();
		light6->setColor(glm::vec4(.4, .8f, 0.4f, 1)).setFallOff(50).setDirection(glm::vec4(-1, -1, 0, 1)).setIntensity(1000).setPosition(glm::vec4(-4, 5, 1, 1)).
			setInnerAngle(15).setOuterAngle(30).setType(Light::light_type::SPOT);
		//Renderer.GetLightManager()->AddLight(*light6);

		Light* light7 = Renderer.GetLightManager()->CreateLight();
		light7->setColor(glm::vec4(.4, .8f, 0.4f, 1)).setFallOff(50).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(1000).setPosition(glm::vec4(0, 7, 5, 1)).
			setInnerAngle(15).setOuterAngle(30).setType(Light::light_type::SPOT);
		//Renderer.GetLightManager()->AddLight(*light7);

		Light* light8 = Renderer.GetLightManager()->CreateLight();
		light8->setColor(glm::vec4(.1, .8f, 0.8f, 1)).setFallOff(50).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(1000).setPosition(glm::vec4(10, 7, -1, 1)).
			setInnerAngle(15).setOuterAngle(30).setType(Light::light_type::SPOT);
		//Renderer.GetLightManager()->AddLight(*light8);

		Light* light9 = Renderer.GetLightManager()->CreateLight();
		light9->setColor(glm::vec4(.1, .8f, 0.8f, 1)).setFallOff(50).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(1000).setPosition(glm::vec4(10, 7, -1, 1)).
			setInnerAngle(15).setOuterAngle(30).setType(Light::light_type::SPOT);


		Camera camera1(glm::vec3(0, 4, 5), glm::vec3(0, 1, 0), 270.0f, 90.0f);

		float sunphi = 110.0f; //0-180
		float suntheta = 0.0f; //0-360
		
		glm::vec3 sphere_position(2, 2, -3);
		int sphere_right = 1;
		float torus_angle = 10;
		int spawn_time = 0;

		const int ObjectCount = 50;
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


			spawn_time++;
			if (spawn_time == 400)
			{
				for (int i = 0; i < ObjectCount; i++)
				{
					//spawn and submit
					tmps_objects[i] = new RenderObject(Renderer.GetDevice(), rocktexture, Renderer.GetFramesInFlight());
					Renderer.GetMeshCache()->SetObjectMesh("Models/cyborg/cyborg.obj", tmps_objects[i]->GetMesh());
					tmps_objects[i]->GetMaterial().SetAlbedoMap(bricktexture);
					tmps_objects[i]->GetMaterial().SetRoughness(.3);
					tmps_objects[i]->SetTransformation(glm::vec3(i*2, 1, -10), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, 0);

					Renderer.AddRenderObject(tmps_objects[i], RenderObjectManager::BIN_TYPE::REGULAR);
					
					if (i < 49) {
						tmp_lights[i] = Renderer.GetLightManager()->CreateLight();
						tmp_lights[i]->setColor(glm::vec4(.1, .4f, 0.8f, 1)).setFallOff(10).setDirection(glm::vec4(0, -1, 0, 1)).setIntensity(150).setPosition(glm::vec4(-100 + (200/7)*(i%7),6,-100 + (200 / 7)*std::floor(i/7),1)).
							setInnerAngle(20).setOuterAngle(25).setType(Light::light_type::SPOT);
						Renderer.GetLightManager()->AddLight(*tmp_lights[i]);
					}
				}

				Renderer.GetLightManager()->SetShadowCaster(*light1, false);
			}

			Renderer.Update();
			
			//only update if you add/delete lights
			light2_color.x = ((int)((light2_color.x * 255) + 1) % 255) / 255.0f;
			light2_color.y = ((int)((light2_color.y * 255) + 1) % 255) / 255.0f;
			light2->setColor(light2_color);


			if (spawn_time == 800)
			{

				//remove
				for (int i = 0; i < ObjectCount; i++)
				{
					Renderer.RemoveRenderObject(tmps_objects[i], RenderObjectManager::BIN_TYPE::REGULAR);

					if (i < 49) {
						Renderer.GetLightManager()->RemoveLight(*tmp_lights[i]);
						Renderer.GetLightManager()->DestroyLight(tmp_lights[i]);
					}
				}
				spawn_time = 0;

				Renderer.GetLightManager()->SetShadowCaster(*light1, true);
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
			float speed = .1f;
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_I)) {
				light3_pos.z-= speed;
				light3->setPosition(light3_pos);

				//Renderer.GetLightManager()->AddLight(*light9);
				//Renderer.GetLightManager()->RemoveLight(*light9);
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_K)) {
				light3_pos.z+= speed;
				light3->setPosition(light3_pos);
				//Renderer.GetLightManager()->AddLight(*light9);
				//Renderer.GetLightManager()->RemoveLight(*light9);
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_J)) {
				light3_pos.x-= speed;
				light3->setPosition(light3_pos);
				//Renderer.GetLightManager()->AddLight(*light9);
				//Renderer.GetLightManager()->RemoveLight(*light9);
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_L)) {
				light3_pos.x+= speed;
				light3->setPosition(light3_pos);
				//Renderer.GetLightManager()->AddLight(*light9);
				//Renderer.GetLightManager()->RemoveLight(*light9);
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_U)) {
				light3_pos.y-= speed;
				light3->setPosition(light3_pos);
				//Renderer.GetLightManager()->AddLight(*light9);
				//Renderer.GetLightManager()->RemoveLight(*light9);
			}
			if (Renderer.GetInputManager().GetKeyPress(GLFW_KEY_O)) {
				light3_pos.y+= speed;
				light3->setPosition(light3_pos);
				//Renderer.GetLightManager()->AddLight(*light9);
				//Renderer.GetLightManager()->RemoveLight(*light9);
			}
			//sphere_light->SetTransformation(light3_pos, glm::vec3(light3->getParams().falloff, light3->getParams().falloff, light3->getParams().falloff), RenderObject::ROTATE_DIMENSION::XANGLE, 0);


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
			torus->SetTransformation(glm::vec3(10, 1.3, 0), glm::vec3(1, 1, 1), RenderObject::ROTATE_DIMENSION::XANGLE, torus_angle);

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
			for (int i = 0; i < ObjectCount; i++)
			{
				Renderer.RemoveRenderObject(tmps_objects[i], RenderObjectManager::BIN_TYPE::REGULAR);

				//Renderer.GetLightManager()->RemoveLight(*tmp_lights[i]);
				//Renderer.GetLightManager()->DestroyLight(tmp_lights[i]);
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
		Renderer.RemoveRenderObject(grass1, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(grass2, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(grass3, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(grass4, RenderObjectManager::BIN_TYPE::BLENDABLE);
		Renderer.RemoveRenderObject(torus, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_white, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_light, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(sphere_white2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(tower, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(fence1, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(fence2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(fence3, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(fence4, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(fence5, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(fence6, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(fence7, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(plane1, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(plane2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(plane3, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(plane4, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(spherebig, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(mountain1, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(stairs, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(stairs2, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(stairs3, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(house, RenderObjectManager::BIN_TYPE::REGULAR);
		Renderer.RemoveRenderObject(teapot2, RenderObjectManager::BIN_TYPE::REGULAR);

		Renderer.GetLightManager()->RemoveLight(*light1);
		Renderer.GetLightManager()->RemoveLight(*light2);
		Renderer.GetLightManager()->RemoveLight(*light3);
		Renderer.GetLightManager()->DestroyLight(light1);
		Renderer.GetLightManager()->DestroyLight(light2);
		Renderer.GetLightManager()->DestroyLight(light3);
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