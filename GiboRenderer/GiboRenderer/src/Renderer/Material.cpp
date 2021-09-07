#include "../pch.h"
#include "Material.h"
#include "vkcore/VulkanHelpers.h"

namespace Gibo {

	Material::~Material()
	{
		DestroyBuffers();
	}

	void Material::CreateBuffer()
	{
		//create staging buffer
		deviceref->CreateBuffer(sizeof(materialinfo), VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, stagingbuffer);

		//create gpu buffer
		deviceref->CreateBuffer(sizeof(materialinfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, material_buffer);
	}

	void Material::DestroyBuffers()
	{
		deviceref->DestroyBuffer(stagingbuffer);
		deviceref->DestroyBuffer(material_buffer);
	}

	void Material::BindBuffer()
	{
		//bind data to staging buffer
		deviceref->BindData(stagingbuffer.allocation, &material_info, sizeof(materialinfo));
		//copy over to gpu buffer
		CopyBufferToBuffer(*deviceref, stagingbuffer.buffer, material_buffer.buffer, VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, sizeof(materialinfo));
	}

	void Material::CreateMaps(vkcoreTexture defaulttexture)
	{
		albedo_map = defaulttexture;
		specular_map = defaulttexture;
		metal_map = defaulttexture;
		normal_map = defaulttexture;
	}

	void Material::SetBrickMaterial()
	{
		materialinfo mat;
		mat.albedo = glm::vec4(.2, .2, .2, 1);
		mat.reflectance = .4;
		mat.metal = 0;
		mat.roughness = 0.4;
		mat.anisotropy = 0.3;
		mat.specular_map = 0;

		SetMaterialInfo(mat);
		//SetNormalMap("Images/brick_normalup.png");
		//SetAlbedoMap("Images/brickwall.jpg");
	}

	void Material::SetDefaultMaterial()
	{
		materialinfo mat;
		mat.albedo = glm::vec4(.2, .2, .2, 1);
		mat.reflectance = .5;
		mat.metal = 0;
		mat.roughness = 0.2;
		mat.anisotropy = 0.3;

		SetMaterialInfo(mat);
	}

	void Material::SetGoldMaterial()
	{
		materialinfo mat;
		mat.albedo = glm::vec4(1.00, 0.71, 0.29, 1);
		mat.reflectance = .5;
		mat.metal = 1;
		mat.roughness = 0.3;
		mat.anisotropy = 0.4;
		//mat.albedo_map = 1;
		mat.anisotropy_path = 1.0;

		SetMaterialInfo(mat);
	}

	void Material::SetCopperMaterial()
	{
		materialinfo mat;
		mat.albedo = glm::vec4(.95, .64, .54, 1);
		mat.reflectance = .4;
		mat.metal = 1;
		mat.roughness = 0.3;
		mat.anisotropy = 0.4;
		mat.anisotropy_path = 1.0;

		SetMaterialInfo(mat);
	}

	void Material::SetPlasticMaterial()
	{
		materialinfo mat;
		mat.albedo = glm::vec4(.7, .7, .7, 1);
		mat.reflectance = .5;
		mat.metal = 0;
		mat.roughness = 0.3;
		mat.clearcoat_path = 1.0;
		mat.clearcoat = 1.0;
		mat.clearcoatroughness = 0.4;

		SetMaterialInfo(mat);
	}

	void Material::SetSilverMaterial()
	{
		materialinfo mat;
		mat.albedo = glm::vec4(0.97, 0.96, 0.91, 1);
		mat.reflectance = .5;
		mat.metal = 1;
		mat.roughness = 0.5;
		mat.anisotropy = 0.7;
		mat.anisotropy_path = 1.0;

		SetMaterialInfo(mat);
	}


}