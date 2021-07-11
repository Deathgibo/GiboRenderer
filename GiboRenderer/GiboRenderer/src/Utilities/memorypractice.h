#pragma once
#include <iostream>

class MyStackAllocator
{
public:

	void Initialize(int size)
	{
		N = size;
		data = (int*)malloc(sizeof(int) * N);
		stackpointer = 0;
	}

	void Destroy()
	{
		free(data);
	}

	int* Allocate()
	{
		if (stackpointer < N)
		{
			stackpointer++;
			return &data[stackpointer - 1];
		}
		else
		{
			return nullptr;
		}
	}

	void Free()
	{
		if (stackpointer > 0)
		{
			stackpointer--;
		}
	}

private:
	int* data;
	int stackpointer;
	int N;

};

template<class T>
class PoolAllocator
{
public:
	struct Chunk
	{

		Chunk* next;
	};

	PoolAllocator(size_t chunksPerBlock) : mChunksPerBlock(chunksPerBlock) {
		std::cout << "chunks per block: " << chunksPerBlock << " sizeof objects: " << sizeof(T) << std::endl;
		mAlloc = allocateBlock(sizeof(T));
	}

	~PoolAllocator()
	{
		for (int i = 0; i < chunk_ptrs.size(); i++)
		{
			free(chunk_ptrs[i]);
		}
		chunk_ptrs.clear();
	}

	T* allocate()
	{
		if (mAlloc == nullptr)
		{
			mAlloc = allocateBlock(sizeof(T));
		}

		Chunk* freeChunk = mAlloc;

		mAlloc = mAlloc->next;

		return reinterpret_cast<T*>(freeChunk);
	}

	void deallocate(void* ptr)
	{
		reinterpret_cast<Chunk*>(ptr)->next = mAlloc;

		mAlloc = reinterpret_cast<Chunk*>(ptr);
	}

private:
	size_t mChunksPerBlock;

	Chunk* mAlloc = nullptr;

	std::vector<Chunk*> chunk_ptrs;

	Chunk* allocateBlock(size_t chunksize)
	{
		//std::cout << "Allocating Block of size: " << mChunksPerBlock * chunksize << std::endl;
		size_t blocksize = mChunksPerBlock * chunksize;
		Chunk* blockBegin = reinterpret_cast<Chunk*>(malloc(blocksize));
		chunk_ptrs.push_back(blockBegin);

		Chunk* chunk = blockBegin;
		for (int i = 0; i < mChunksPerBlock - 1; i++)
		{
			chunk->next = reinterpret_cast<Chunk*>(reinterpret_cast<char*>(chunk) + chunksize);

			chunk = chunk->next;
		}
		chunk->next = nullptr;

		return blockBegin;
	}
};
