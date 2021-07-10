#pragma once
#include "AssetManager.h"
#include "Material.h"

namespace Gibo {

	class RenderObject
	{
	public:
		friend class RenderObjectManager;
		enum class ROTATE_DIMENSION : uint8_t {XANGLE,YANGLE,ZANGLE};
	public:
		RenderObject(vkcoreDevice* device, vkcoreTexture defaulttexture, int framesinflight) : material(device, defaulttexture), model_matrix(framesinflight){};
		~RenderObject() = default;
		 
		void SetMesh(MeshCache::Mesh Mesh) { mesh = Mesh; }

		void Update(int framecount);

		void SetTransformation(glm::vec3 position, glm::vec3 scale, ROTATE_DIMENSION dimension, float indegrees);

		Material& GetMaterial() { return material; }
		MeshCache::Mesh& GetMesh() { return mesh; }
		glm::mat4& GetMatrix(int frame_count) { return model_matrix[frame_count]; }
		glm::mat4& GetSyncedMatrix() { return internal_matrix; }
		uint32_t GetId() { return descriptor_id; }

	private:
		void NotifyUpdate() { needs_updated = true; frames_updated = 0; }
	private:
		std::vector<glm::mat4> model_matrix;
		glm::mat4 internal_matrix;
		Material material;
		MeshCache::Mesh mesh;

		uint32_t descriptor_id; //this is the id all the shaderprograms use when they add this renderobjects descriptor to its map
		bool needs_updated = false;
		int frames_updated = 0;
	};

}
