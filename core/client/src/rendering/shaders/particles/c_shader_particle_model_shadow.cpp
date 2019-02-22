#include "stdafx_client.h"
 // prosper TODO
#if 0
#include "pragma/c_engine.h"
#include "pragma/game/c_game.h"
#include "pragma/rendering/shaders/particles/c_shader_particle_model_shadow.hpp"
#include "pragma/model/c_modelmesh.h"

using namespace Shader;

LINK_SHADER_TO_CLASS(ParticleModelShadow,particlemodelshadow);

extern DLLCENGINE CEngine *c_engine;
extern DLLCLIENT CGame *c_game;

ParticleModelShadow::ParticleModelShadow()
	: Shadow("particlemodelshadow","particles/model/vs_particle_model_shadow","particles/model/fs_particle_model_shadow")
{}

bool ParticleModelShadow::BeginDrawTest(Vulkan::BufferObject *particleBuffer,Vulkan::BufferObject *rotBuffer,Vulkan::CommandBufferObject *cmdBuffer,CLightBase *light,uint32_t w,uint32_t h)
{
	auto r = Shadow::BeginDrawTest(light,w,h);
	if(r == false)
		return r;
	cmdBuffer->BindVertexBuffer(umath::to_integral(Binding::Xyzs),{particleBuffer,rotBuffer});
	return r;
}

void ParticleModelShadow::BindInstanceDescriptorSet(Vulkan::CommandBufferObject *cmdBuffer,const Vulkan::DescriptorSetObject *descSetInstance,const Mat4 &depthMvp)
{
	Shadow::BindInstanceDescriptorSet(cmdBuffer,descSetInstance);
	BindDepthMatrix(cmdBuffer,depthMvp);
}

void ParticleModelShadow::DrawTest(CModelSubMesh *mesh,uint32_t instanceCount)
{
	auto &context = c_engine->GetRenderContext();
	Shadow::DrawTest(context.GetDrawCmd(),mesh,[instanceCount](const Vulkan::CommandBufferObject *drawCmd,uint32_t triangleVertexCount) {
		drawCmd->DrawIndexed(0,triangleVertexCount,0,instanceCount);
	});
}

void ParticleModelShadow::InitializeVertexDescriptions(std::vector<vk::VertexInputBindingDescription> &vertexBindingDescriptions,std::vector<vk::VertexInputAttributeDescription> &vertexAttributeDescriptions)
{
	Shadow::InitializeVertexDescriptions(vertexBindingDescriptions,vertexAttributeDescriptions);
	vertexBindingDescriptions.push_back({
		umath::to_integral(Binding::Xyzs),
		CParticleSystem::PARTICLE_DATA_SIZE,
		Anvil::VertexInputRate::INSTANCE
	});
	vertexBindingDescriptions.push_back({
		umath::to_integral(Binding::Rotation),
		sizeof(Quat),
		Anvil::VertexInputRate::INSTANCE
	});

	vertexAttributeDescriptions.push_back({
		umath::to_integral(Location::Xyzs),
		umath::to_integral(Binding::Xyzs),
		Anvil::Format::R32G32B32A32_SFLOAT,0
	});
	vertexAttributeDescriptions.push_back({
		umath::to_integral(Location::Rotation),
		umath::to_integral(Binding::Rotation),
		Anvil::Format::R32G32B32A32_SFLOAT,0
	});
}
#endif