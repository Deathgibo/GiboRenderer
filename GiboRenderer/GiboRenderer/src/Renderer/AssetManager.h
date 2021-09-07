#pragma once
#include "vkcore/vkcoreDevice.h"
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "BoundingVolumes.h"

namespace Gibo {

	static int Vertex_Attribute_Length = 14;//counts number of float in each vertex (position,normal,uv,T,B)
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
			std::string mesh_name;
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
		//Mesh GetMesh(std::string filename);
		void SetObjectMesh(std::string filename, MeshCache::Mesh& mesh);
		void GetOriginalMeshBV(std::string filename, BoundingVolume*& bv);
		void SetQuadMesh(MeshCache::Mesh& mesh);
	private:
		void LoadMesh(std::string filename, std::vector<float>& vertexdata, std::vector<unsigned int>& indexdata);
		struct Mesh_internal
		{
			vkcoreBuffer vbo;
			vkcoreBuffer ibo;
			uint32_t index_size;
			BoundingVolume* bv; //bounding volume in model space for this mesh
		};
	private:
		std::unordered_map<std::string, Mesh_internal> meshCache;
		Mesh_internal Quad_Mesh;
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

			bool checkvalidity()
			{
				int mesh_size = positions.size();
				if (normals.size() != mesh_size || texcoords.size() != mesh_size || tangents.size() != mesh_size || bitangents.size() != mesh_size || indices.size() != mesh_size)
				{
					Logger::LogWarning("model loaded properties don't agree on mesh number\n");
					return false;
				}
				for (int i = 0; i < mesh_size; i++)
				{
					int vertex_size = positions[i].size();
					if (normals[i].size() != vertex_size || texcoords[i].size() != vertex_size || tangents[i].size() != vertex_size || bitangents[i].size() != vertex_size)
					{
						Logger::LogWarning("model loaded properties don't match vertex count on mesh ", i, "\n");
						return false;
					}

					if (indices[i].size() == 0)
					{
						Logger::LogError("Don't support models with no indexing yet\n");
						return false;
					}
				}

				return true;
			}
		};
	public:
		static bool LoadQuad(std::vector<float>& vertexdata, std::vector<unsigned int>& indexdata);
		static bool Load(std::string path, std::vector<std::vector<float>>& vertexdata, std::vector<std::vector<unsigned int>>& indexdata);
	private:
		static void Process_Node(aiNode* node, const aiScene* scene, std::vector<std::vector<float>>& vertexdata, std::vector<std::vector<unsigned int>>& indexdata);

		static void Process_Mesh(aiMesh* mesh, const aiScene * scene, std::vector<float>& vertexdata, std::vector<unsigned int>& indexdata);

		static void calcAverageTangent(unsigned int * indices, unsigned int indiceCount, float * vertices, unsigned int verticeCount, unsigned int vLength, unsigned int tangentoffset);
	};
	
}

