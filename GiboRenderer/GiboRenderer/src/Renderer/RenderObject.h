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
		RenderObject(vkcoreDevice* device, vkcoreTexture defaulttexture) : material(device, defaulttexture), model_matrix(glm::mat4(1.0)) {};
		~RenderObject() = default;
		 
		void SetMesh(MeshCache::Mesh Mesh) { mesh = Mesh; }
		void SetTexture(vkcoreTexture Texture) { texture = Texture; }

		void SetTransformation(glm::vec3 position, glm::vec3 scale, ROTATE_DIMENSION dimension, float indegrees);

		Material& GetMaterial() { return material; }
		MeshCache::Mesh& GetMesh() { return mesh; }
		vkcoreTexture& GetTexture() { return texture; }
		glm::mat4& GetMatrix() { return model_matrix; }

	private:
		Material material;
		MeshCache::Mesh mesh;
		vkcoreTexture texture;
		glm::mat4 model_matrix;
		
		//TODO - maybe look to remove some of these to keep the size of this class as small as possible
		bool destroyed = false;
	};

}
