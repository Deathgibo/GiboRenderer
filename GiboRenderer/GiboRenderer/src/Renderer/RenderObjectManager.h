#pragma once
#include "RenderObject.h"
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
	*/

	class RenderObjectManager
	{
	public:
		enum BIN_TYPE : int { REGULAR, BLENDABLE };
		static const int BIN_SIZE = 2;
		static const int MAX_OBJECTS_ALLOWED = 50; //this is the number of max objects allowed so we can preallocate for some data-structures
		
		struct graveyardinfo {
			RenderObject* object;
			int frame_count;
			BIN_TYPE type;

			graveyardinfo(RenderObject* a, int b, BIN_TYPE c) : object(a), frame_count(b), type(c) {};
		};
	public:
		RenderObjectManager(vkcoreDevice& device, int framesinflight) : deviceref(device), maxframesinflight(framesinflight)
		{
			graveyard.reserve(MAX_OBJECTS_ALLOWED); Object_Bin[0].reserve(MAX_OBJECTS_ALLOWED); Object_Bin[1].reserve(MAX_OBJECTS_ALLOWED); Object_vector.reserve(MAX_OBJECTS_ALLOWED);
		};
		~RenderObjectManager() = default;

		//void CreateRenderObject();
		//void DeleteRenderObject();

		void AddRenderObject(RenderObject* object, BIN_TYPE type)
		{
			//add render object to data-structures

			//BIN
			Object_Bin[type].push_back(object);

			//VECTOR
			Object_vector.push_back(object);

			//other data structures
		}

		//when you pass this in you surrender the memory you must not use renderobject anymore. The memory can't just be released because frames are in flight.
		void RemoveRenderObject(RenderObject* object, BIN_TYPE type)
		{
			graveyard.push_back(graveyardinfo(object, 0, type));
			object->destroyed = true;
		}

		void Update()
		{
			//loop through graveyard objects and increment timer once it gets to frames in flight you can safely remove it from all data structures
			for (int i = 0; i < graveyard.size(); i++)
			{
				graveyard[i].frame_count++;

				///if you update at cpu no dependency stage you need framesinflight + 1, else if you wait for image it can just be framesinflight
				if (graveyard[i].frame_count > maxframesinflight)
				{
					DeleteObject(graveyard[i].object, graveyard[i].type);
					graveyard.erase(graveyard.begin() + i);
				}
			}
		}

		void CleanUp()
		{
			//cleanup data structures and anything left in the graveyard
			for (int i = 0; i < graveyard.size(); i++)
			{
				DeleteObject(graveyard[i].object, graveyard[i].type);
			}

			graveyard.clear();

			Object_Bin[0].clear();
			Object_Bin[1].clear();

			Object_vector.clear();
		}

		std::array<std::vector<RenderObject*>, BIN_SIZE>& GetBin() { return Object_Bin; };
		std::vector<RenderObject*>& GetVector() { return Object_vector; }

	private:
		void DeleteObject(RenderObject* object, BIN_TYPE type)
		{
			//remove from every data-structure and delete

			//BIN
			for (int i = 0; i < Object_Bin[type].size(); i++)
			{
				if (Object_Bin[type][i] == object)
				{
					Object_Bin[type].erase(Object_Bin[type].begin() + i);
				}
			}

			//VECTOR
			Object_vector.clear();


			delete object;
			object = nullptr;
		}

	private:
		/*
			This is a data structure for holding objects in different bins that need to be rendered differently. It holds all elements in each bin in a contiguous array for quick looping,
			and makes it a nice way to groups things and render them all in the same graphics state. like blendable objects, maybe ones that need culling, different geometry modes,etc. 
			Its implemented an array with a predefined number of bins each holding a  vector of renderobjects. The vector will be a data structure that creates a maxmimum object size conitugous array. 
			Then it uses a quick way to add and	remove objects without actually deleting the memory and moving all elements over. It acts like a memory pool.
		*/
		std::vector<graveyardinfo> graveyard;
		std::array<std::vector<RenderObject*>, BIN_SIZE> Object_Bin;
		std::vector<RenderObject*> Object_vector;

		vkcoreDevice& deviceref;
		int maxframesinflight;
	};

}


/*
insert(renderobject*)
findanddelete(renderobject*)


when you insert you look at the head and put the memory there and then return an offset key
then when you delete you give the offset so it immeditalely finds it and removes it and sets the node pointers.

*/