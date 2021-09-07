#pragma once
#include "Logger.h"
#include <stack>

namespace Gibo {
	/*
	 This data structure is a quick and efficient way if you need to insert and remove objects knowing a max size.
	 The the way it works is every node has the data and an int holding an index into another slot in the array (they all start out pointing to the one ahead of it).
	 Theres also a head_index which points to the next available index into the array. When you insert it gives you back the index into the array where your inserted data went.
	 So now when you delete you pass in that index and it will find it right away.

	 Inserting is (1) because the head knows exactly where to next insert
	 Deleting is (1) because you pass in the exact index to delete
	 The total memory used is sizeof(T) * Size and its all malloc'd on constructor, and all deleted on destructor

	 *TODO- find a way to loop through memory. going to need three heads, one for next free slot, one for beggining of loop, and one for end. each node will need 2 pointers.
	 Find a logical way to make this work and this should be faster than linked lists because theres no pointer jumping.
	*/

	template<typename T, size_t Size>
	class QuickArray
	{
	private:
		struct node
		{
			T t;
			int next;
		};

	public:
		QuickArray()
		{
			//Allocate contiguous array of nodes and set all their indexes pointing to index in front of it
			data = reinterpret_cast<node*>(malloc(sizeof(node) * Size));

			node* node_walker = data;
			for (int i = 0; i < Size - 1; i++)
			{
				node_walker->next = i + 1;
				node_walker = node_walker + 1;
			}
			node_walker->next = -1;

			head_index = 0;
		}

		~QuickArray()
		{
			free(data);
		}

		//get element at head_index, insert the data into it, move head along to next one and return the index of inserted element
		int Insert(T element)
		{
	#ifdef _DEBUG 
			if (current_size == Size) { Logger::LogError("quick array insertion exceeded array size error\n"); }
			current_size++;
	#endif 
			int index = head_index;
			node free_node = data[head_index];
			free_node.t = element;

			head_index = data[head_index].next;

			return index;
		}

		// head now points to the element you just deleted, and that points to where the head used to be
		void Remove(int index)
		{
	#ifdef _DEBUG 
			if (index < 0 || index > Size - 1)
			{
				Logger::LogError("quick array removing index outside memory bounds\n");
			}
	#endif 
			data[index].next = head_index;
			head_index = index;

	#ifdef _DEBUG 
			current_size--;
	#endif 
		}

	private:
		int head_index;
		node* data;

		int current_size = 0;
	};

	/*//pretty much this data structure is used if you have large data and want a vector. For a vector if you delete an item you might have to copy N times which is too expensive.
	//This gets rid of that and also preallocated all memory so theres no memory allocation. The downside is that its static memory and theres cpu overrhead which pays off if your
	//using large structures because we don't copy anymore. This data structure is an array that holds available slots and gives you an array of currently used indices you can loop through
	template<typename T, size_t Size>
	class SmartArray
	{
	public:
		SmartArray()
		{
			;
		}
		//SmartArray()
		//{
			//data_valid.reserve(Size);
			//for (int i = data.size() - 1; i >= 0; i--)
			//{
			//	stack.push(i);
			//}
		//}

		~SmartArray() = default;

		void Insert(T val)
		{	
#ifdef _DEBUG 
			if (stack.empty()) { Logger::LogError("StaticArray out of room and your trying to insert!\n"); }
#endif
			int index = stack.top();
			data[index] = val;
			data_valid.push_back(index);
			
			//go to next avaible slot
			stack.pop();
		}

		void Remove(T val)
		{
			for (int i = 0; i < data.size(); i++)
			{
				if (val == data[i])
				{
					for (int j = 0; j < data_valid.size(); j++)
					{
						if (data_valid[j] == i)
						{
							data_valid.erase(data_valid.begin() + j);
						}
					}
					stack.push(i);
				}
			}
		}

		T& operator[](uint32_t index)
		{
			return data[index];
		}
		
		std::vector<uint32_t>& GetValidData() { return data_valid; }

	private:
		std::array<T, Size> data;
		std::vector<uint32_t> data_valid;
		std::stack<uint32_t> stack;
	};*/
}