#include "stdafx_client.h"
#include "pragma/rendering/renderers/base_renderer.hpp"

using namespace pragma::rendering;

BaseRenderer::BaseRenderer(Scene &scene)
	: m_scene{scene}
{}
bool BaseRenderer::RenderScene(std::shared_ptr<prosper::PrimaryCommandBuffer> &drawCmd,std::shared_ptr<prosper::RenderTarget> &rt,FRender renderFlags)
{
	BeginRendering(drawCmd);
	return true;
}
void BaseRenderer::BeginRendering(std::shared_ptr<prosper::PrimaryCommandBuffer> &drawCmd)
{
	GetScene().UpdateBuffers(drawCmd);
}
void BaseRenderer::Resize(uint32_t width, uint32_t height) {}
void BaseRenderer::UpdateRenderSettings(pragma::RenderSettings &renderSettings) {}
void BaseRenderer::UpdateCameraData(pragma::CameraData &cameraData) {}
void BaseRenderer::OnEntityAddedToScene(CBaseEntity &ent) {}
bool BaseRenderer::IsRasterizationRenderer() const {return false;}
bool BaseRenderer::IsRayTracingRenderer() const {return false;}
const std::shared_ptr<prosper::Texture> &BaseRenderer::GetPresentationTexture() const {return GetSceneTexture();}

Scene &BaseRenderer::GetScene() const {return m_scene;}
