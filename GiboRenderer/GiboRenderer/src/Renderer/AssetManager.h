#pragma once
#include "vkcore/vkcoreDevice.h"
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

namespace Gibo {

	/*
	The point of these classes are to create the meshes and textures at start up and just cache them for when other renderobjects need them.
	This method is very nice as long as we don't have so much data that we can't store it all on VRAM which is fine for now. But later I will
	have to implement some dynamic gpu memory system where I load in the gpu buffers that are needed and free the ones that aren't.
	*/

	class MeshCache
	{
	public:
		struct Mesh
		{
			VkBuffer vbo;
			VkBuffer ibo;
			uint32_t index_size;
		};

	public:
		MeshCache(vkcoreDevice& device) : deviceref(device), total_buffer_size(0) {};
		~MeshCache() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		MeshCache(MeshCache const&) = delete;
		MeshCache(MeshCache&&) = delete;
		MeshCache& operator=(MeshCache const&) = delete;
		MeshCache& operator=(MeshCache&&) = delete;

		void LoadMeshFromFile(std::string filename);
		void CleanUp();
		void PrintMemory() const; 
		Mesh GetMesh(std::string filename);
	private:
		void LoadMesh(std::string filename, std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, std::vector<glm::vec2>& texcoords, std::vector<glm::vec3>& tangents, std::vector<glm::vec3>& bitangents,
						std::vector<unsigned int>& indices);
		struct Mesh_internal
		{
			vkcoreBuffer vbo;
			vkcoreBuffer ibo;
			uint32_t index_size;
		};
	private:
		std::unordered_map<std::string, Mesh_internal> meshCache;
		size_t total_buffer_size;
		vkcoreDevice& deviceref;
	};


	/*ASSIMP NOTES
	https://assimp-docs.readthedocs.io/en/latest/index.html

	 has a logger if you want to use it
	 has a coordinate system you can change, texture coordinate system, and winding order
	 you have nodes with have children, and they just hold references to the meshes, scene has all the meshes
	 *if im working with heirarchy docs have an example of creating nodes with the translation matrices
	 meshes only have 1 material
	 AI_SUCCESS
	*/

	//TODO - implement multithreading when loading, even during runtime?
	class ASSIMPLoader
	{
	public:
		struct LoaderHelper
		{
			std::vector<std::vector<glm::vec3>> positions;
			std::vector<std::vector<glm::vec3>> normals;
			std::vector<std::vector<glm::vec2>> texcoords;
			std::vector<std::vector<unsigned int>> indices;
			std::vector<std::vector<glm::vec3>> tangents;
			std::vector<std::vector<glm::vec3>> bitangents;
		};
	public:
		static bool Load(std::string path, std::vector<std::vector<glm::vec3>>& positions, std::vector<std::vector<glm::vec3>>& normals,
			std::vector<std::vector<glm::vec2>>& texcoords, std::vector<std::vector<unsigned int>>& indices, std::vector<std::vector<glm::vec3>>& tangent, std::vector<std::vector<glm::vec3>>& bitangent);
	private:
		static void Process_Node(aiNode* node, const aiScene* scene, std::vector<std::vector<glm::vec3>>& positions, std::vector<std::vector<glm::vec3>>& normals,
			std::vector<std::vector<glm::vec2>>& texcoords, std::vector<std::vector<unsigned int>>& indices, std::vector<std::vector<glm::vec3>>& tangent, std::vector<std::vector<glm::vec3>>& bitangent);

		static void Process_Mesh(aiMesh* mesh, const aiScene * scene, std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
			std::vector<glm::vec2>& texcoords, std::vector<unsigned int>& indices, std::vector<glm::vec3>& tangent, std::vector<glm::vec3>& bitangent);
	};
	
}

