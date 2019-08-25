#include "stdafx_client.h"
#include "pragma/rendering/shaders/world/c_shader_pbr.hpp"
#include "cmaterialmanager.h"
#include "pragma/entities/environment/c_env_reflection_probe.hpp"
#include "pragma/rendering/renderers/rasterization_renderer.hpp"
#include <pragma/entities/entity_iterator.hpp>
#include <pragma/entities/entity_component_system_t.hpp>
#include <image/prosper_sampler.hpp>
#include <prosper_descriptor_set_group.hpp>
#include <texture_type.h>

extern DLLCLIENT CGame *c_game;
extern DLLCLIENT ClientState *client;
extern DLLCENGINE CEngine *c_engine;

using namespace pragma;

#pragma optimize("",off)
decltype(ShaderPBR::DESCRIPTOR_SET_MATERIAL) ShaderPBR::DESCRIPTOR_SET_MATERIAL = {
	{
		prosper::Shader::DescriptorSetInfo::Binding { // Albedo Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		},
		prosper::Shader::DescriptorSetInfo::Binding { // Normal Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		},
		prosper::Shader::DescriptorSetInfo::Binding { // Ambient occlusion Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		},
		prosper::Shader::DescriptorSetInfo::Binding { // Metallic Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		},
		prosper::Shader::DescriptorSetInfo::Binding { // Roughness Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		},
		prosper::Shader::DescriptorSetInfo::Binding { // Emission Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		},
		prosper::Shader::DescriptorSetInfo::Binding { // Parallax Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		}
	}
};
decltype(ShaderPBR::DESCRIPTOR_SET_PBR) ShaderPBR::DESCRIPTOR_SET_PBR = {
	{
		prosper::Shader::DescriptorSetInfo::Binding { // Irradiance Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		},
		prosper::Shader::DescriptorSetInfo::Binding { // Prefilter Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		},
		prosper::Shader::DescriptorSetInfo::Binding { // BRDF Map
			Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER,
			Anvil::ShaderStageFlagBits::FRAGMENT_BIT
		}
	}
};
ShaderPBR::ShaderPBR(prosper::Context &context,const std::string &identifier)
	: ShaderTextured3DBase{context,identifier,"world/vs_textured","world/fs_pbr"}
{
	SetPipelineCount(umath::to_integral(Pipeline::Count));
}
bool ShaderPBR::BindMaterialParameters(CMaterial &mat)
{
	return ShaderTextured3DBase::BindMaterialParameters(mat);
}
prosper::Shader::DescriptorSetInfo &ShaderPBR::GetMaterialDescriptorSetInfo() const {return DESCRIPTOR_SET_MATERIAL;}
void ShaderPBR::SetForceNonIBLMode(bool b) {m_bNonIBLMode = b;}
bool ShaderPBR::BeginDraw(
	const std::shared_ptr<prosper::PrimaryCommandBuffer> &cmdBuffer,const Vector4 &clipPlane,Pipeline pipelineIdx,
	RecordFlags recordFlags
)
{
	m_extMatFlags = MaterialFlags::None;
	return ShaderTextured3DBase::BeginDraw(cmdBuffer,clipPlane,pipelineIdx,recordFlags);
}
bool ShaderPBR::BindSceneCamera(const rendering::RasterizationRenderer &renderer,bool bView)
{
	if(ShaderTextured3DBase::BindSceneCamera(renderer,bView) == false)
		return false;
	auto &scene = renderer.GetScene();
	auto hCam = scene.GetActiveCamera();
	if(hCam.expired())
		return false;
	auto pos = hCam->GetEntity().GetPosition();
	if(m_bNonIBLMode == false)
	{
		// Find closest reflection probe to camera position
		EntityIterator entIt {*c_game};
		entIt.AttachFilter<TEntityIteratorFilterComponent<pragma::CReflectionProbeComponent>>();
		auto dClosest = std::numeric_limits<float>::max();
		BaseEntity *entClosest = nullptr;
		for(auto *ent : entIt)
		{
			auto posEnt = ent->GetPosition();
			auto d = uvec::distance_sqr(pos,posEnt);
			if(d >= dClosest)
				continue;
			dClosest = d;
			entClosest = ent;
		}
		if(entClosest)
		{
			auto &reflectionProbeC = *entClosest->GetComponent<pragma::CReflectionProbeComponent>();
			auto *ds = reflectionProbeC.GetIBLDescriptorSet();
			if(ds)
				return RecordBindDescriptorSet(*ds,DESCRIPTOR_SET_PBR.setIndex);
		}
	}

	// No reflection probe and therefore no IBL available. Fallback to non-IBL rendering.
	m_extMatFlags |= MaterialFlags::NoIBL;
	return true;
}
void ShaderPBR::ApplyMaterialFlags(CMaterial &mat,MaterialFlags &outFlags) const
{
	ShaderTextured3DBase::ApplyMaterialFlags(mat,outFlags);
	outFlags |= m_extMatFlags;
}
void ShaderPBR::InitializeGfxPipelineDescriptorSets(Anvil::GraphicsPipelineCreateInfo &pipelineInfo,uint32_t pipelineIdx)
{
	ShaderTextured3DBase::InitializeGfxPipelineDescriptorSets(pipelineInfo,pipelineIdx);
	AddDescriptorSetGroup(pipelineInfo,DESCRIPTOR_SET_PBR);
}

static bool bind_texture(Material &mat,Anvil::DescriptorSet &ds,const std::string &textureIdentifier,uint32_t bindingIndex,const std::string &defaultTexName="")
{
	auto &matManager = static_cast<CMaterialManager&>(client->GetMaterialManager());
	auto &texManager = matManager.GetTextureManager();

	TextureManager::LoadInfo loadInfo {};
	loadInfo.flags = TextureLoadFlags::LoadInstantly;
	loadInfo.mipmapLoadMode = TextureMipmapMode::Load;

	std::shared_ptr<Texture> tex = nullptr;
	auto *pMap = mat.GetTextureInfo(textureIdentifier);
	if(pMap && pMap->texture)
		tex = std::static_pointer_cast<Texture>(pMap->texture);
	else if(defaultTexName.empty())
		return false;
	else
	{
		std::shared_ptr<void> ptrTex = nullptr;
		if(texManager.Load(*c_engine,defaultTexName,loadInfo,&ptrTex) == false)
			return false;
		tex = std::static_pointer_cast<Texture>(ptrTex);
	}
	if(tex && tex->texture != nullptr)
		prosper::util::set_descriptor_set_binding_texture(ds,*tex->texture,bindingIndex);
	return true;
}
std::shared_ptr<prosper::DescriptorSetGroup> ShaderPBR::InitializeMaterialDescriptorSet(CMaterial &mat,const prosper::Shader::DescriptorSetInfo &descSetInfo)
{
	auto &dev = c_engine->GetDevice();
	auto *albedoMap = mat.GetTextureInfo("albedo_map");
	if(albedoMap == nullptr || albedoMap->texture == nullptr)
		return nullptr;

	auto albedoTexture = std::static_pointer_cast<Texture>(albedoMap->texture);
	if(albedoTexture->texture == nullptr)
		return nullptr;
	auto descSetGroup = prosper::util::create_descriptor_set_group(dev,descSetInfo);
	mat.SetDescriptorSetGroup(*this,descSetGroup);
	auto descSet = (*descSetGroup)->get_descriptor_set(0u);
	prosper::util::set_descriptor_set_binding_texture(*descSet,*albedoTexture->texture,umath::to_integral(MaterialBinding::AlbedoMap));

	// TODO: Don't use normal map if not available
	if(bind_texture(mat,*descSet,"normal_map",umath::to_integral(MaterialBinding::NormalMap),"black") == false)
		return false;

	if(
		bind_texture(mat,*descSet,"ambient_occlusion_map",umath::to_integral(MaterialBinding::AmbientOcclusionMap)) == false &&
		bind_texture(mat,*descSet,"ao_map",umath::to_integral(MaterialBinding::AmbientOcclusionMap),"white") == false
	)
		return false;

	if(bind_texture(mat,*descSet,"metalness_map",umath::to_integral(MaterialBinding::MetallicMap),"black") == false)
		return false;

	if(bind_texture(mat,*descSet,"roughness_map",umath::to_integral(MaterialBinding::RoughnessMap),"white") == false)
		return false;

	if(bind_texture(mat,*descSet,"glowmap",umath::to_integral(MaterialBinding::EmissionMap)) == false)
		bind_texture(mat,*descSet,"emission_map",umath::to_integral(MaterialBinding::EmissionMap));

	if(bind_texture(mat,*descSet,"parallax_map",umath::to_integral(MaterialBinding::ParallaxMap),"black") == false)
		return false;
	return descSetGroup;
}
std::shared_ptr<prosper::DescriptorSetGroup> ShaderPBR::InitializeMaterialDescriptorSet(CMaterial &mat)
{
	return InitializeMaterialDescriptorSet(mat,DESCRIPTOR_SET_MATERIAL);
}
#pragma optimize("",on)