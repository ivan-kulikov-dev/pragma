#include "stdafx_client.h"
#include "pragma/clientstate/clientstate.h"
#include "textureinfo.h"
#include "pragma/console/c_cvar.h"
#include <texturemanager/texturemanager.h>
#include <cmaterialmanager.h>
#include <image/prosper_sampler.hpp>

extern DLLCENGINE CEngine *c_engine;
extern DLLCLIENT ClientState *client;

static void get_filter_mode(UInt32 filter,Anvil::Filter &minFilter,Anvil::Filter &magFilter,Anvil::SamplerMipmapMode &mipmapMode,UInt32 &anisotropy)
{
	anisotropy = 1; // 1 = off
	if(filter < 0)
		filter = 0;
	else if(filter > 6)
		filter = 6;
	if(filter == 0)
	{
		minFilter = Anvil::Filter::NEAREST;
		magFilter = Anvil::Filter::NEAREST;
		mipmapMode = Anvil::SamplerMipmapMode::NEAREST;
	}
	else if(filter == 1)
	{
		minFilter = Anvil::Filter::NEAREST;
		magFilter = Anvil::Filter::LINEAR;
		mipmapMode = Anvil::SamplerMipmapMode::NEAREST;
	}
	else
	{
		minFilter = Anvil::Filter::LINEAR;
		magFilter = Anvil::Filter::LINEAR;
		mipmapMode = Anvil::SamplerMipmapMode::LINEAR;
		if(filter >= 3)
		{
			filter -= 2;
			anisotropy = umath::pow(static_cast<uint32_t>(2),filter);
		}
	}
}

static CVar cvTextureQuality = GetClientConVar("cl_render_texture_quality");
static uint32_t get_quality_lod_offset()
{
	auto v = cvTextureQuality->GetInt();
	if(v >= 4)
		return 0;
	else if(v == 3)
		return 1;
	else if(v == 2)
		return 2;
	else if(v == 1)
		return 3;
	return 5;
}

static CVar cvTextureFiltering = GetClientConVar("cl_render_texture_filtering");
static void CVAR_CALLBACK_cl_render_texture_quality(NetworkState*,ConVar*,int,int)
{
	if(client == nullptr)
		return;
	c_engine->WaitIdle();
	Anvil::Filter minFilter,magFilter;
	Anvil::SamplerMipmapMode mipmapMode;
	UInt32 anisotropy;
	get_filter_mode(cvTextureFiltering->GetInt(),minFilter,magFilter,mipmapMode,anisotropy);
	auto lodOffset = get_quality_lod_offset();
	auto &materialManager = static_cast<CMaterialManager&>(client->GetMaterialManager());
	auto &textureManager = materialManager.GetTextureManager();
	auto &sampler = textureManager.GetTextureSampler();
	auto &customSamplers = textureManager.GetCustomSamplers();
	auto fUpdateSampler = [anisotropy,mipmapMode,minFilter,magFilter,lodOffset](prosper::Sampler &sampler) {
		sampler.SetMaxAnisotropy(static_cast<float>(anisotropy));
		sampler.SetMipmapMode(mipmapMode);
		sampler.SetMinFilter(minFilter);
		sampler.SetMagFilter(magFilter);
		sampler.SetMinLod(static_cast<float>(lodOffset));
		sampler.Update();
	};
	fUpdateSampler(*sampler);
	for(auto &sampler : customSamplers)
	{
		if(sampler.expired())
			continue;
		fUpdateSampler(*sampler.lock());
	}
	materialManager.ReloadMaterialShaders(); // Make sure to reload descriptor sets (So samplers are updated)
}
REGISTER_CONVAR_CALLBACK_CL(cl_render_texture_filtering,CVAR_CALLBACK_cl_render_texture_quality);
REGISTER_CONVAR_CALLBACK_CL(cl_render_texture_quality,CVAR_CALLBACK_cl_render_texture_quality);