#pragma once
#include "RenderObject.h"
#include "AssetManager.h"
#include "../Utilities/memorypractice.h"
#include <array>

namespace Gibo {
	/*
	Render Objects are the most important thing in the render engine. They describe all the objects were going to render and are the main dynamic aspect of the whole engine.
	Therefore we have to make sure they get held in the most efficient and useful data structures and are managed very well for the engine to run quick and get the needed information.
	This class holds all the renderobjects in our scene and also any multiple of data structures we will need. For example we might need a scene graph for spatial queries, but a quick contiguous
	array for looping through them, or one for sorting blended objects, maybe one structure holds dynamic or static objects, etc. So this class would hold all those data structures and they 
	would all have pointers to the objects (not shared pointers, these objects have a lot of memory and we can't afford pseudo-memory leaks, especially on its gpu resources). 
	There will be an insert object and delete object and every data structure will get updated based on that.

	notes for data structure:
		. they need a way to identify and remove render objects based off an id number or its memory
		. it should be made as quick as possible for adding and removing objects
		. Every data structure we have means more overhead every time we add and delete object so make sure the cost is worth it
		. the data structures hold a pointer to a renderobject and should obviously should not free them ever unless its chosen as the main one to handle the memory
	
	deletion is handled based off frames in flight. You need to set a bit to destroyed on the renderobject so the things looping through data structures know they are gone while things using
	them still have the memory. Then after k frames in flight you can finally delete it from the data structure.

	TODO- maybe just hold all the objects in a conitguous array as the source data, then everything grabs from that so if we need to loop trhough everything contiguously its in the arry.
	Check if looping through pointers is fine that point to contiguous memory
	*/

	class RenderObjectManager
	{
	public:
		enum BIN_TYPE : int { REGULAR, BLENDABLE };
		static const int BIN_SIZE = 2;
		static const int MAX_OBJECTS_ALLOWED = 200; //this is the number of max objects allowed so we can preallocate for some data-structures
		
		struct graveyardinfo 
		{
			RenderObject* object;
			int frame_count;

			graveyardinfo(RenderObject* a, int b) : object(a), frame_count(b) {};
		};
		//just a quick way to hand out id's. counter will increment and repeat at IDHELPERSIZE and the slots just represent if that id is taken or not
		//static constexpr int IDHELPERSIZE = 1000;
		struct idhelper
		{
			std::array<bool, MAX_OBJECTS_ALLOWED>slots;
			uint32_t counter;

			idhelper()
			{
				slots.fill(false);
				counter = 0;
			}

			uint32_t GetNextID()//assumes every slot will never all be true
			{
				while (slots[counter] == true)
				{
					counter = (counter + 1) % slots.size();
				}
				uint32_t returncounter = counter;
				slots[returncounter] = true;
				counter = (counter + 1) % slots.size();
				return returncounter;
			}

			void FreeID(uint32_t id)
			{
				slots[id] = false;
			}
		};

	public:
		RenderObjectManager(vkcoreDevice& device, MeshCache& Meshcache, int framesinflight) : deviceref(device), meshcache(Meshcache), maxframesinflight(framesinflight), 
			               object_pool(MAX_OBJECTS_ALLOWED)
		{
			graveyard.reserve(MAX_OBJECTS_ALLOWED); Object_Bin[0].reserve(MAX_OBJECTS_ALLOWED); Object_Bin[1].reserve(MAX_OBJECTS_ALLOWED); Object_vector.reserve(MAX_OBJECTS_ALLOWED);
		};
		~RenderObjectManager() = default;

		//void CreateRenderObject();
		//void DeleteRenderObject();
		RenderObject* CreateRenderObject(vkcoreDevice* device, vkcoreTexture defaulttexture)
		{
			RenderObject* a = new(object_pool.allocate()) RenderObject(device, defaulttexture, maxframesinflight);
			return a;
		}

		//ONLY call if you didn't add or remove this renderobject for some reason
		void DeleteRenderObject(RenderObject* object)
		{
			object->~RenderObject();//need to explicilty call deconstructor because deallocate doesn't destroy the memory
			object_pool.deallocate(object);
		}

		void AddRenderObject(RenderObject* object, BIN_TYPE type)
		{
			
			//give object an id
			object->descriptor_id = idmanager.GetNextID();

			//add render object to data-structures

			//BIN
			Object_Bin[type].push_back(object);

			//VECTOR
			Object_vector.push_back(object);

			//bounding volumes (Copy orginal bounding volume for mesh into new pointer, then transform it to current models model matrix)
			meshcache.GetOriginalMeshBV(object->GetMesh().mesh_name, Object_bvs[object->descriptor_id]);
			Object_bvs[object->descriptor_id]->Transform(object->internal_matrix);

			//other data structures
		}

		//when you pass this in you surrender the memory you must not use renderobject anymore. The memory can't just be released because frames are in flight. but we can remove them from data structure
		void RemoveRenderObject(RenderObject* object, BIN_TYPE type)
		{
			graveyard.push_back(graveyardinfo(object, 0));

			//remove from every data-structure
			//BIN
			for (int i = 0; i < Object_Bin[type].size(); i++)
			{
				if (Object_Bin[type][i] == object)
				{
					Object_Bin[type].erase(Object_Bin[type].begin() + i);
				}
			}

			//VECTOR
			for (int i = 0; i < Object_vector.size(); i++)
			{
				if (object == Object_vector[i])
				{
					Object_vector.erase(Object_vector.begin() + i);
				}
			}

			//bounding volume (remove bounding volume pointer and erase from data structure)
			delete Object_bvs[object->descriptor_id];
			Object_bvs.erase(object->descriptor_id);
		}

		void Update()
		{
			//check to see if any renderobject has moved so we can delete volume, get original, move it to new spot
			for (int i = 0; i < Object_vector.size(); i++)
			{
				if (Object_vector[i]->Moved())
				{
					delete Object_bvs[Object_vector[i]->descriptor_id];
					meshcache.GetOriginalMeshBV(Object_vector[i]->GetMesh().mesh_name, Object_bvs[Object_vector[i]->descriptor_id]);
					Object_bvs[Object_vector[i]->descriptor_id]->Transform(Object_vector[i]->internal_matrix);
				}
			}

			//loop through graveyard objects and increment timer once it gets to frames in flight you can safely remove it from all data structures
			for (int i = 0; i < graveyard.size(); i++)
			{
				graveyard[i].frame_count++;

				///if you update at cpu no dependency stage you need framesinflight + 1, else if you wait for image it can just be framesinflight
				if (graveyard[i].frame_count > maxframesinflight)
				{
					DeleteObject(graveyard[i].object);
					graveyard.erase(graveyard.begin() + i);
				}
			}
		}

		void CleanUp()
		{
#ifdef _DEBUG
			if (Object_vector.size() != 0) { Logger::LogError("Not all renderobjects were removed from renderobject manager!\n"); }
#endif
			//cleanup data structures and anything left in the graveyard
			for (int i = 0; i < graveyard.size(); i++)
			{
				DeleteObject(graveyard[i].object);
			}

			graveyard.clear();

			Object_Bin[0].clear();
			Object_Bin[1].clear();

			Object_vector.clear();

			for (auto boundingvolume : Object_bvs)
			{
				delete boundingvolume.second;
			}
			Object_bvs.clear();
		}

		std::array<std::vector<RenderObject*>, BIN_SIZE>& GetBin() { return Object_Bin; };
		std::vector<RenderObject*>& GetVector() { return Object_vector; }
		std::unordered_map<uint32_t, BoundingVolume*>& GetBoundingVolumes() { return Object_bvs; }

	private:
		void DeleteObject(RenderObject* object)
		{
			//free id
			idmanager.FreeID(object->descriptor_id);

			//gpu is not using memory anymore so we are free to delete it
			object->~RenderObject();//need to explicilty call deconstructor because deallocate doesn't destroy the memory
			object_pool.deallocate(object);

			//delete object;
			//object = nullptr;
		}

	private:
		std::vector<graveyardinfo> graveyard; //data-structure for holding removed objects. since frames are in flight it must wait x amount of frames before actually deleting the memory
		std::array<std::vector<RenderObject*>, BIN_SIZE> Object_Bin; //a bin structure with holds vectors in each bin, and bins are used for different rendering purposes to group objects
		std::vector<RenderObject*> Object_vector; //a simple contiguous data structure if you need to loop through every object quickly
		std::unordered_map<uint32_t, BoundingVolume*> Object_bvs; //holds bounding volumes for renderobjects. I didn't want to store this data in the main renderobject because memory coherency.

		vkcoreDevice& deviceref;
		MeshCache& meshcache;
		int maxframesinflight;

		PoolAllocator<RenderObject> object_pool;

		idhelper idmanager; //dishes out ids to renderobjects submitted and frees id when object is delete.
	};

}
