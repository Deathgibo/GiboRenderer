#pragma once
#include "vkcore/vkcoreDevice.h"
#include "TextureCache.h"

namespace Gibo {
	//notes
	/*
	. albedo - diffuse color, the color that doesnt get absorbed. This is either an rgb triplet or a map. Pretty much just color of object like a texture. Either rgb triplet or a texture.

	. (gone) fresnel - specular color, the color that gets reflected, either is a rgb triplet or a texture. Usually for metals it'll be a color, maybe default to white.

	. reflectance - specular intensity, for dieletrics this will always be white, but more metals the specular color will tend towards its albedo color, can also load from a map grayscale values

	. Metalness - metals have no diffuse and higher specular, which specific specular colors and anisotropic. Either can be binary, just switches equations to anisotropic, or it could be a scalar linearly interpolating between regular and metal which would be more expensive. This is just a number, or a texture saying what part is metal or not.

	. roughness - this is just a variable used in the lighting calculations giving how distributed the specular direction is, so just a scalar or a map

	. anisotrpy variable - this only matters if your doing an anisotropic shading, its just used as a variable in the shaders, pretty much a value or a texture but probably not a texture

	. normals - normals of geometry already given in the vertex, or a normal map.

	. clearcoat strength of the clear coat layer, really it strengthens the specular value of the fresnel term 0-1

	. clearcoat roughness is the new roughness value for the clearcoat specifically 0-1

	. shirley - just a binary that switches specular/diffuse equations
	*/

	class Material
	{
	public:
		struct materialinfo {
			glm::vec4 albedo;    //0..1

			float reflectance;
			float metal; //0..1
			float roughness; //0..1
			float anisotropy; //-1..1

			float clearcoat; //0 1
			float clearcoatroughness; // 0 1
			float anisotropy_path = 0.0;
			float clearcoat_path = 0.0;

			int albedo_map = 0;
			int specular_map = 0;
			int metal_map = 0;
			int normal_map = 0;
		}; //64 bytes
	public:
		//TODO right now you need to create textures to add to descriptors. This is wasteful because were allocating memory even tho we might not need these
		//however I don't know how to deal with this for now, its too advance to recreate all descriptors if you just set a texture, Though I could have a global default normalmap everything gets linked too.
		//but then when you want to change it you will still need to change descriptor sets. its annoying.
		Material(vkcoreDevice* device, vkcoreTexture defaulttexture) : deviceref(device) { CreateBuffer(); CreateMaps(defaulttexture); SetDefaultMaterial(); }
		~Material();

		void DestroyBuffers();

		void SetDefaultMaterial();
		void SetBrickMaterial();
		void SetGoldMaterial();
		void SetCopperMaterial();
		void SetPlasticMaterial();
		void SetSilverMaterial();

		inline void SetMaterialInfo(materialinfo info) { material_info = info; BindBuffer(); }
		inline void SetAlbedo(glm::vec3 val) { material_info.albedo.x = val.x; material_info.albedo.y = val.y; material_info.albedo.z = val.z; BindBuffer(); }
		inline void SetAlpha(float val) { material_info.albedo.a = val; BindBuffer(); }
		inline void SetReflectance(float val) { material_info.reflectance = val; BindBuffer(); }
		inline void SetMetal(float val) { material_info.metal = val; BindBuffer(); }
		inline void SetRoughness(float val) { material_info.roughness = val; BindBuffer(); }
		inline void SetAnisotropy(float val) { material_info.anisotropy = val; BindBuffer(); }
		//inline void SetShirley(float val) { material_info.shirley = val; BindBuffer(); }
		inline void SetClearCoat(float val) { material_info.clearcoat = val; BindBuffer(); }
		inline void SetClearCoatRoughness(float val) { material_info.clearcoatroughness = val; BindBuffer(); }
		inline void SetRenderPath(float Anisotropy, float ClearCoat) { material_info.anisotropy_path = Anisotropy; material_info.clearcoat_path = ClearCoat; BindBuffer(); }

		inline void ToggleAlbedoMap(bool val) { material_info.albedo_map = val; BindBuffer(); }
		inline void ToggleSpecularMap(bool val) { material_info.specular_map = val; BindBuffer(); }
		inline void ToggleMetalMap(bool val) { material_info.metal_map = val; BindBuffer(); }
		inline void ToggleNormalMap(bool val) { material_info.normal_map = val; BindBuffer(); }

		void SetAlbedoMap(vkcoreTexture texture) { albedo_map = texture; ToggleAlbedoMap(true); }
		void SetSpecularMap(vkcoreTexture texture) { specular_map = texture; ToggleSpecularMap(true); }
		void SetMetalMap(vkcoreTexture texture) { metal_map = texture; ToggleMetalMap(true); }
		void SetNormalMap(vkcoreTexture texture) { normal_map = texture; ToggleNormalMap(true); }

		inline materialinfo GetMaterialInfo() { return material_info; }
		inline vkcoreBuffer GetBuffer() { return material_buffer; }
		inline vkcoreTexture GetAlbedoMap() { return albedo_map; }
		inline vkcoreTexture GetSpecularMap() { return specular_map; }
		inline vkcoreTexture GetMetalMap() { return metal_map; }
		inline vkcoreTexture GetNormalMap() { return normal_map; }

	protected:
		void CreateBuffer();
		void BindBuffer();
		void CreateMaps(vkcoreTexture defaulttexture);
	private:
		//cpu data
		materialinfo material_info;

		//gpu data
		vkcoreBuffer material_buffer;
		vkcoreBuffer stagingbuffer;

		vkcoreTexture albedo_map;
		vkcoreTexture specular_map;
		vkcoreTexture metal_map;
		vkcoreTexture normal_map;

		vkcoreDevice* deviceref;
	};

}