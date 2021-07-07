#include "../pch.h"
#include "AssetManager.h"

namespace Gibo {
	
	//TODO - load in vertexdata, and indexdata directly instead of all these vectors. Also support no indexing.
	void MeshCache::LoadMeshFromFile(std::string filename)
	{
		//load the vertex data from the file
		ASSIMPLoader::LoaderHelper vertexdata;
		ASSIMPLoader::Load(filename, vertexdata.positions, vertexdata.normals, vertexdata.texcoords, vertexdata.indices, vertexdata.tangents, vertexdata.bitangents);

		if (vertexdata.positions.size() != vertexdata.normals.size() || vertexdata.positions.size() != vertexdata.texcoords.size() || vertexdata.positions.size() != vertexdata.tangents.size()
			|| vertexdata.positions.size() != vertexdata.bitangents.size()) 
		{
			Logger::LogWarning(filename, " vertices, texcoords, and normals dont match size\n");
		}

		if (vertexdata.indices.size() == 0)
		{
			Logger::LogError(filename, " Don't support models with no indexing yet\n");
		}

		int numberofmeshes = vertexdata.positions.size();
		Logger::Log(filename, " number of meshes ", vertexdata.positions.size(), "\n");

		for (int i = 0; i < numberofmeshes; i++)
		{
			LoadMesh(filename, vertexdata.positions[i], vertexdata.normals[i], vertexdata.texcoords[i], vertexdata.tangents[i], vertexdata.bitangents[i], vertexdata.indices[i]);
		}

	}

	void MeshCache::LoadMesh(std::string filename, std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, std::vector<glm::vec2>& texcoords, std::vector<glm::vec3>& tangents, std::vector<glm::vec3>& bitangents,
		std::vector<unsigned int>& indices)
	{
		//create vbo and maybe ibo with it
		std::vector<float> vertices;
		for (int i = 0; i < positions.size(); i++)
		{
			vertices.push_back(positions[i].x); vertices.push_back(positions[i].y); vertices.push_back(positions[i].z);

			vertices.push_back(normals[i].x); vertices.push_back(normals[i].y); vertices.push_back(normals[i].z);

			vertices.push_back(texcoords[i].x); vertices.push_back(texcoords[i].y);

			vertices.push_back(tangents[i].x); vertices.push_back(tangents[i].y); vertices.push_back(tangents[i].z);

			vertices.push_back(bitangents[i].x); vertices.push_back(bitangents[i].y); vertices.push_back(bitangents[i].z);
		}

		//create gpu buffers and store in cache
		Mesh_internal& mesh = meshCache[filename];
		deviceref.CreateBufferStaged(sizeof(float)*vertices.size(), vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mesh.vbo, 
			                         VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
		deviceref.CreateBufferStaged(sizeof(unsigned int) * indices.size(), indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mesh.ibo, 
			                          VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
		mesh.index_size = indices.size();

		//count up memory total_buffer_size
		total_buffer_size += sizeof(float) * vertices.size();
		total_buffer_size += sizeof(unsigned int) * indices.size();
	}

	void MeshCache::CleanUp()
	{
		for (auto& mesh : meshCache)
		{
			deviceref.DestroyBuffer(mesh.second.vbo);
			deviceref.DestroyBuffer(mesh.second.ibo);
		}
		meshCache.clear();
	}

	void MeshCache::PrintMemory() const
	{
		Logger::Log("Total Mesh Cache buffer size: ", total_buffer_size, " bytes\n");
	}

	MeshCache::Mesh MeshCache::GetMesh(std::string filename) 
	{
		Mesh mesh;
		if (meshCache.count(filename) == 1)
		{
			mesh.vbo = meshCache[filename].vbo.buffer;
			mesh.ibo = meshCache[filename].ibo.buffer;
			mesh.index_size = meshCache[filename].index_size;
		}
		else
		{
			Logger::LogError("Mesh: ", filename, " does not exist in the meshcache. Load it first.\n");
			mesh.vbo = VK_NULL_HANDLE;
			mesh.ibo = VK_NULL_HANDLE;
			mesh.index_size = 0;
		}
		return mesh;
	}


	
	bool ASSIMPLoader::Load(std::string path, std::vector<std::vector<glm::vec3>>& positions, std::vector<std::vector<glm::vec3>>& normals,
		std::vector<std::vector<glm::vec2>>& texcoords, std::vector<std::vector<unsigned int>>& indices, std::vector<std::vector<glm::vec3>>& tangents, std::vector<std::vector<glm::vec3>>& bitangents)
	{
		Assimp::Importer importer;
		//importer.GetErrorString();
		//importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace);

		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace);

		if (!scene) {
			Logger::LogInfo(importer.GetErrorString(), "\n");
			return false;
		}
		aiNode* node = scene->mRootNode;
		Process_Node(scene->mRootNode, scene, positions, normals, texcoords, indices, tangents, bitangents);
		//scene holds materials and you can loop through materials
		//scene->mMaterials[0]->GetTexture(aiTextureType::aiTextureType_NORMALS, 0);
		Logger::LogInfo("Loaded	" + path + " Successfully\n");

		return true;
	}

	void ASSIMPLoader::Process_Node(aiNode* node, const aiScene* scene, std::vector<std::vector<glm::vec3>>& positions, std::vector<std::vector<glm::vec3>>& normals,
		std::vector<std::vector<glm::vec2>>& texcoords, std::vector<std::vector<unsigned int>>& indices, std::vector<std::vector<glm::vec3>>& tangents, std::vector<std::vector<glm::vec3>>& bitangents)
	{
		for (int x = 0; x < node->mNumMeshes; x++) {
			positions.push_back(std::vector<glm::vec3>());
			normals.push_back(std::vector<glm::vec3>());
			texcoords.push_back(std::vector<glm::vec2>());
			indices.push_back(std::vector<unsigned int>());
			tangents.push_back(std::vector<glm::vec3>());
			bitangents.push_back(std::vector<glm::vec3>());

			aiMesh* mesh = scene->mMeshes[node->mMeshes[x]];

			Process_Mesh(mesh, scene, positions[positions.size() - 1], normals[normals.size() - 1], texcoords[texcoords.size() - 1], indices[indices.size() - 1], tangents[tangents.size() - 1], bitangents[bitangents.size() - 1]);
		}

		for (int i = 0; i < node->mNumChildren; i++) {
			Process_Node(node->mChildren[i], scene, positions, normals, texcoords, indices, tangents, bitangents);
		}

	}

	void ASSIMPLoader::Process_Mesh(aiMesh* mesh, const aiScene * scene, std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals,
		std::vector<glm::vec2>& texcoords, std::vector<unsigned int>& indices, std::vector<glm::vec3>& tangents, std::vector<glm::vec3>& bitangents)
	{

		//go through every vertex and get: position, normal, and texcoords	
		//std::cout << mesh->mNumVertices << std::endl;
		for (int i = 0; i < mesh->mNumVertices; i++) 
		{	
			glm::vec3 position(0, 0, 0);
			if (mesh->HasPositions()) {
				position.x = mesh->mVertices[i].x;
				position.y = mesh->mVertices[i].y;
				position.z = mesh->mVertices[i].z;
			}

			glm::vec3 normal(0, 0, 0);
			if (mesh->HasNormals()) {
				normal.x = mesh->mNormals[i].x;
				normal.y = mesh->mNormals[i].y;
				normal.z = mesh->mNormals[i].z;
			}

			glm::vec2 texcoord(0, 0);
			if (mesh->mTextureCoords[0]) {
				texcoord.x = mesh->mTextureCoords[0][i].x;
				texcoord.y = mesh->mTextureCoords[0][i].y;
			}

			glm::vec3 tangent(0, 0, 0);
			glm::vec3 bitangent(0, 0, 0);
			if (mesh->HasTangentsAndBitangents())
			{
				tangent = { mesh->mTangents[i].x,mesh->mTangents[i].y,mesh->mTangents[i].z };
				bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
			}

			positions.push_back(position);
			normals.push_back(normal);
			texcoords.push_back(texcoord);
			tangents.push_back(tangent);
			bitangents.push_back(bitangent);

		}

		if (tangents.size() == 0)
		{
			Logger::LogWarning("mesh doesn't have tangents and bitangents\n");
		}
		if (normals.size() == 0)
		{
			Logger::LogWarning("mesh doesn't have normals\n");
		}
		if (texcoords.size() == 0)
		{
			Logger::LogWarning("mesh doesn't have uv\n");
		}
		if (positions.size() == 0)
		{
			Logger::LogWarning("mesh doesn't have positions\n");
		}

		//go through every face and get indices
		for (int i = 0; i < mesh->mNumFaces; i++) {
			aiFace face = mesh->mFaces[i];
			for (int j = 0; j < face.mNumIndices; j++) {
				indices.push_back(face.mIndices[j]);
			}
		}

	}
}