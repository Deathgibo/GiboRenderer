#include "../pch.h"
#include "AssetManager.h"

namespace Gibo {

	//TODO - load in vertexdata, and indexdata directly instead of all these vectors. Also support no indexing.
	void MeshCache::LoadMeshFromFile(std::string filename)
	{
		if (meshCache.count(filename) != 0)
		{
			Logger::LogWarning("Loading mesh ", filename, " twice\n");
			return;
		}

		//load the vertex data from the file
		std::vector<std::vector<float>> vertexdata;
		std::vector<std::vector<unsigned int>> indexdata;

		ASSIMPLoader::Load(filename, vertexdata, indexdata);

		int numberofmeshes = vertexdata.size();
		if (numberofmeshes != 1)
		{
			Logger::LogWarning("Don't support models with multiple meshes yet: ", numberofmeshes, "\n"); //TODO- append to filename a number for the mesh doomguy0, doomguy1, doomguy2
			return;
		}
		size_t previous_size = total_buffer_size;
		for (int i = 0; i < numberofmeshes; i++)
		{
			LoadMesh(filename, vertexdata[i], indexdata[i]);
		}

		Logger::Log("model ", filename, ": ", total_buffer_size - previous_size, " bytes. number of meshes ", numberofmeshes, "\n");
	}

	void MeshCache::LoadMesh(std::string filename, std::vector<float>& vertexdata, std::vector<unsigned int>& indexdata)
	{
		//create gpu buffers and store in cache
		Mesh_internal& mesh = meshCache[filename];
		deviceref.CreateBufferStaged(sizeof(float)*vertexdata.size(), vertexdata.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mesh.vbo,
			                         VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
		deviceref.CreateBufferStaged(sizeof(unsigned int) * indexdata.size(), indexdata.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mesh.ibo,
			                          VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
		mesh.index_size = indexdata.size();

		mesh.bv = new Sphere(); //AABB Sphere
		mesh.bv->Construct(vertexdata, Vertex_Attribute_Length);

		//count up memory total_buffer_size
		total_buffer_size += sizeof(float) * vertexdata.size();
		total_buffer_size += sizeof(unsigned int) * indexdata.size();
	}

	void MeshCache::CleanUp()
	{
		for (auto& mesh : meshCache)
		{
			delete mesh.second.bv;
			deviceref.DestroyBuffer(mesh.second.vbo);
			deviceref.DestroyBuffer(mesh.second.ibo);
		}
		meshCache.clear();

		if (Quad_Mesh.vbo.buffer != VK_NULL_HANDLE)
		{
			delete Quad_Mesh.bv;
			deviceref.DestroyBuffer(Quad_Mesh.vbo);
			deviceref.DestroyBuffer(Quad_Mesh.ibo);
		}
	}

	void MeshCache::PrintMemory() const
	{
		Logger::Log("Total Mesh Cache buffer size: ", total_buffer_size, " bytes Total Meshes stored: ", meshCache.size(), "\n");
	}

	//copies the contents of the original generate bounding volume into a new pointer
	void MeshCache::GetOriginalMeshBV(std::string filename, BoundingVolume*& bv)
	{
		if (filename == "Quad")
		{
			bv = Quad_Mesh.bv->clone();
		}
		else
		{
#ifdef _DEBUG
			if (meshCache.count(filename) == 0) { Logger::LogWarning("Called GetMeshBV but filename isn't in cache!\n"); }
#endif 
			bv = meshCache[filename].bv->clone();
		}
	}

	//copies contents directly into mesh
	void MeshCache::SetObjectMesh(std::string filename, MeshCache::Mesh& mesh) 
	{
		if (meshCache.count(filename) == 1)
		{
			mesh.vbo = meshCache[filename].vbo.buffer;
			mesh.ibo = meshCache[filename].ibo.buffer;
			mesh.index_size = meshCache[filename].index_size;
			mesh.mesh_name = filename;
		}
		else
		{
			Logger::LogError("Mesh: ", filename, " does not exist in the meshcache. Load it first.\n");
			mesh.vbo = VK_NULL_HANDLE;
			mesh.ibo = VK_NULL_HANDLE;
			mesh.index_size = 0;
			mesh.mesh_name = "NA";
		}
	}

	void MeshCache::SetQuadMesh(MeshCache::Mesh& mesh)
	{
		if (Quad_Mesh.vbo.buffer == VK_NULL_HANDLE)
		{
			std::vector<float> vertexdata;
			std::vector<unsigned int> indexdata;
			ASSIMPLoader::LoadQuad(vertexdata, indexdata);

			deviceref.CreateBufferStaged(sizeof(float)*vertexdata.size(), vertexdata.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, Quad_Mesh.vbo,
				VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
			deviceref.CreateBufferStaged(sizeof(unsigned int) * indexdata.size(), indexdata.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, Quad_Mesh.ibo,
				VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
			Quad_Mesh.index_size = indexdata.size();
			Quad_Mesh.bv = new Sphere(); //AABB Sphere
			Quad_Mesh.bv->Construct(vertexdata, Vertex_Attribute_Length);

			//count up memory total_buffer_size
			total_buffer_size += sizeof(float) * vertexdata.size();
			total_buffer_size += sizeof(unsigned int) * indexdata.size();
		}

		mesh.vbo = Quad_Mesh.vbo.buffer;
		mesh.ibo = Quad_Mesh.ibo.buffer;
		mesh.index_size = Quad_Mesh.index_size;
		mesh.mesh_name = "Quad";
	}
	
	bool ASSIMPLoader::LoadQuad(std::vector<float>& vertexdata, std::vector<unsigned int>& indexdata)
	{
		//screen space
		std::vector<float>vertexdata2{
			-1.0f,1.0f,0.0f,	0.0f, 0.0f, 1.0f,	0,1, 1,0,0, 0,1,0, //tl
			-1.0f,-1.0f,0.0f,	0.0f, 0.0f, 1.0f,	0,0, 1,0,0, 0,1,0, //bl
			1.0f,-1.0f,0.0f,	0.0f, 0.0f, 1.0f,	1,0, 1,0,0, 0,1,0, //br
			1.0f,1.0f,0.0f,		0.0f, 0.0f, 1.0f,	1,1, 1,0,0, 0,1,0 //tr
		};

		std::vector<unsigned int> indexdata2{
			0,1,3,
			3,1,2
		};

		//calcAverageTangent(indexdata2.data(), indexdata2.size(), vertexdata2.data(), vertexdata2.size(), 14, 8);
		
		vertexdata = vertexdata2;
		indexdata = indexdata2;
		
		return true;
	}

	bool ASSIMPLoader::Load(std::string path, std::vector<std::vector<float>>& vertexdata, std::vector<std::vector<unsigned int>>& indexdata)
	{
		Assimp::Importer importer;
		//importer.GetErrorString();
		//importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace);

		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenNormals);

		if (!scene) {
			Logger::LogInfo(importer.GetErrorString(), "\n");
			return false;
		}
		aiNode* node = scene->mRootNode;
		Process_Node(scene->mRootNode, scene, vertexdata, indexdata);
		//scene holds materials and you can loop through materials
		//scene->mMaterials[0]->GetTexture(aiTextureType::aiTextureType_NORMALS, 0);
		Logger::LogInfo("Loaded	" + path + " Successfully\n");

		return true;
	}

	void ASSIMPLoader::Process_Node(aiNode* node, const aiScene* scene, std::vector<std::vector<float>>& vertexdata, std::vector<std::vector<unsigned int>>& indexdata)
	{
		for (int x = 0; x < node->mNumMeshes; x++) 
		{
			vertexdata.resize(vertexdata.size() + 1);
			vertexdata[vertexdata.size() - 1].reserve(10000);
			
			indexdata.resize(indexdata.size() + 1);
			indexdata[indexdata.size() - 1].reserve(10000);

			aiMesh* mesh = scene->mMeshes[node->mMeshes[x]];
			Process_Mesh(mesh, scene, vertexdata[vertexdata.size() - 1], indexdata[indexdata.size() - 1]);
		}

		for (int i = 0; i < node->mNumChildren; i++) {
			Process_Node(node->mChildren[i], scene, vertexdata, indexdata);
		}

	}

	void ASSIMPLoader::Process_Mesh(aiMesh* mesh, const aiScene * scene, std::vector<float>& vertexdata, std::vector<unsigned int>& indexdata)
	{

		//go through every vertex and get: position, normal, and texcoords	
		//position, normals, texcoords, tangent, bitangent 
		bool positions = true;
		bool normals = true;
		bool texcoords = true;
		bool tangentbi = true;
		for (int i = 0; i < mesh->mNumVertices; i++) 
		{	
			if (mesh->HasPositions()) 
			{
				vertexdata.push_back(mesh->mVertices[i].x);
				vertexdata.push_back(mesh->mVertices[i].y);
				vertexdata.push_back(mesh->mVertices[i].z);
			}
			else
			{
				positions = false;
			}

			if (mesh->HasNormals())
			{
				vertexdata.push_back(mesh->mNormals[i].x);
				vertexdata.push_back(mesh->mNormals[i].y);
				vertexdata.push_back(mesh->mNormals[i].z);
			}
			else
			{
				vertexdata.push_back(0);
				vertexdata.push_back(0);
				vertexdata.push_back(0);
				normals = false;
			}

			if (mesh->mTextureCoords[0]) 
			{
				vertexdata.push_back(mesh->mTextureCoords[0][i].x);
				vertexdata.push_back(mesh->mTextureCoords[0][i].y);
			}
			else
			{
				vertexdata.push_back(0);
				vertexdata.push_back(0);
				texcoords = false;
			}

			if (mesh->HasTangentsAndBitangents())
			{
				vertexdata.push_back(mesh->mTangents[i].x);
				vertexdata.push_back(mesh->mTangents[i].y);
				vertexdata.push_back(mesh->mTangents[i].z);

				vertexdata.push_back(mesh->mBitangents[i].x);
				vertexdata.push_back(mesh->mBitangents[i].y);
				vertexdata.push_back(mesh->mBitangents[i].z);
			}
			else
			{
				vertexdata.push_back(0);
				vertexdata.push_back(0);
				vertexdata.push_back(0);

				vertexdata.push_back(0);
				vertexdata.push_back(0);
				vertexdata.push_back(0);
				tangentbi = false;
			}

		}

		if (!tangentbi)
		{
			Logger::LogWarning("mesh doesn't have tangents and bitangents\n");
		}
		if (!normals)
		{
			Logger::LogWarning("mesh doesn't have normals\n");
		}
		if (!texcoords)
		{
			Logger::LogWarning("mesh doesn't have uv\n");
		}
		if (!positions)
		{
			Logger::LogWarning("mesh doesn't have positions\n");
		}

		//go through every face and get indices
		for (int i = 0; i < mesh->mNumFaces; i++) 
		{
			aiFace face = mesh->mFaces[i];
			for (int j = 0; j < face.mNumIndices; j++) 
			{
				indexdata.push_back(face.mIndices[j]);
			}
		}

	}

	void ASSIMPLoader::calcAverageTangent(unsigned int * indices, unsigned int indiceCount, float * vertices, unsigned int verticeCount, unsigned int vLength, unsigned int tangentoffset)
	{
		for (size_t i = 0; i < indiceCount; i += 3)
		{
			unsigned int in0 = indices[i] * vLength;
			unsigned int in1 = indices[i + 1] * vLength;
			unsigned int in2 = indices[i + 2] * vLength;

			//difference in positions
			glm::vec3 edge1(vertices[in1] - vertices[in0], vertices[in1 + 1] - vertices[in0 + 1], vertices[in1 + 2] - vertices[in0 + 2]);
			glm::vec3 edge2(vertices[in2] - vertices[in0], vertices[in2 + 1] - vertices[in0 + 1], vertices[in2 + 2] - vertices[in0 + 2]);

			//tex uv coords are positions 9-10 difference in tex coords
			glm::vec2 duv1(vertices[in1 + 6] - vertices[in0 + 6], vertices[in1 + 7] - vertices[in0 + 7]);
			glm::vec2 duv2(vertices[in2 + 6] - vertices[in0 + 6], vertices[in2 + 7] - vertices[in0 + 7]);

			float f = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);
			glm::vec3 tangent(f * (duv2.y*edge1.x - duv1.y*edge2.x), f * (duv2.y*edge1.y - duv1.y*edge2.y), f * (duv2.y*edge1.z - duv1.y*edge2.z));
			glm::vec3 bitangent(f * (-duv2.x*edge1.x + duv1.x*edge2.x), f * (-duv2.x*edge1.y + duv1.x*edge2.y), f * (-duv2.x*edge1.z + duv1.x*edge2.z));

			//tangent offset should be 11
			in0 += tangentoffset; in1 += tangentoffset; in2 += tangentoffset;
			vertices[in0] += tangent.x; vertices[in0 + 1] += tangent.y; vertices[in0 + 2] += tangent.z;
			vertices[in1] += tangent.x; vertices[in1 + 1] += tangent.y; vertices[in1 + 2] += tangent.z;
			vertices[in2] += tangent.x; vertices[in2 + 1] += tangent.y; vertices[in2 + 2] += tangent.z;

			vertices[in0 + 3] += bitangent.x; vertices[in0 + 4] += bitangent.y; vertices[in0 + 5] += bitangent.z;
			vertices[in1 + 3] += bitangent.x; vertices[in1 + 4] += bitangent.y; vertices[in1 + 5] += bitangent.z;
			vertices[in2 + 3] += bitangent.x; vertices[in2 + 4] += bitangent.y; vertices[in2 + 5] += bitangent.z;
		}

		for (size_t i = 0; i < verticeCount / vLength; i++)
		{
			//normalize bitangent and tangent for each vertex.
			unsigned int nOffset = i * vLength + tangentoffset;

			glm::vec3 tangent = glm::normalize(glm::vec3(vertices[nOffset], vertices[nOffset + 1], vertices[nOffset + 2]));
			vertices[nOffset] = tangent.x; vertices[nOffset + 1] = tangent.y; vertices[nOffset + 2] = tangent.z;

			glm::vec3 bitangent = glm::normalize(glm::vec3(vertices[nOffset + 3], vertices[nOffset + 4], vertices[nOffset + 5]));
			vertices[nOffset + 3] = bitangent.x; vertices[nOffset + 4] = bitangent.y; vertices[nOffset + 5] = bitangent.z;
		}
	}










}