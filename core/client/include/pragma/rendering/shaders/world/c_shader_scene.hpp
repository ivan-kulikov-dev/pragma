#ifndef __C_SHADER_SCENE_HPP__
#define __C_SHADER_SCENE_HPP__

#include "pragma/clientdefinitions.h"
#include "pragma/rendering/shaders/c_shader_3d.hpp"
#include "pragma/rendering/shaders/world/c_shader_textured_base.hpp"
#include <shader/prosper_shader.hpp>

namespace pragma
{
	class DLLCLIENT ShaderScene
		: public Shader3DBase,
		public ShaderTexturedBase
	{
	public:
		enum class Pipeline : uint32_t
		{
			Regular = 0u,
			MultiSample,
			//LightMap,

			Count
		};
		static prosper::Shader::DescriptorSetInfo DESCRIPTOR_SET_RENDER_SETTINGS;
		static prosper::Shader::DescriptorSetInfo DESCRIPTOR_SET_CAMERA;

		static Anvil::Format RENDER_PASS_FORMAT;
		static Anvil::Format RENDER_PASS_DEPTH_FORMAT;

		static Anvil::SampleCountFlagBits RENDER_PASS_SAMPLES;
		static void SetRenderPassSampleCount(Anvil::SampleCountFlagBits samples);

		enum class CameraBinding : uint32_t
		{
			Camera = 0u,
			RenderSettings,
			SSAOMap,
			LightMap
		};

		enum class RenderSettingsBinding : uint32_t
		{
			Debug = 0u,
			Time,
			CSMData
		};

		enum class DebugFlags : uint32_t
		{
			None = 0u,
			LightShowCascades = 1u,
			LightShowShadowMapDepth = LightShowCascades<<1u,
			LightShowFragmentDepthShadowSpace = LightShowShadowMapDepth<<1u,

			ForwardPlusHeatmap = LightShowFragmentDepthShadowSpace<<1u
		};

#pragma pack(push,1)
		struct TimeData
		{
			float curTime;
			float deltaTime;
			float realTime;
			float deltaRealTime;
		};
		struct DebugData
		{
			uint32_t flags;
		};
#pragma pack(pop)

		virtual bool BindSceneCamera(const Scene &scene,bool bView);
		virtual bool BindRenderSettings(Anvil::DescriptorSet &descSetRenderSettings);
	protected:
		ShaderScene(prosper::Context &context,const std::string &identifier,const std::string &vsShader,const std::string &fsShader,const std::string &gsShader="");
		Anvil::SampleCountFlagBits GetSampleCount(uint32_t pipelineIdx) const;
		virtual bool ShouldInitializePipeline(uint32_t pipelineIdx) override;
		virtual void InitializeRenderPass(std::shared_ptr<prosper::RenderPass> &outRenderPass,uint32_t pipelineIdx) override;
		virtual uint32_t GetRenderSettingsDescriptorSetIndex() const=0;
		virtual uint32_t GetCameraDescriptorSetIndex() const=0;
	};

	/////////////////////

	class DLLCLIENT ShaderSceneLit
		: public ShaderScene
	{
	public:
		static prosper::Shader::DescriptorSetInfo DESCRIPTOR_SET_LIGHTS;
		static prosper::Shader::DescriptorSetInfo DESCRIPTOR_SET_CSM;
		static prosper::Shader::DescriptorSetInfo DESCRIPTOR_SET_SHADOWS;

		enum class LightBinding : uint32_t
		{
			LightBuffers = 0u,
			TileVisLightIndexBuffer,
			ShadowData
		};

		enum class ShadowBinding : uint32_t
		{
			ShadowMaps = 0u,
			ShadowCubeMaps
		};

#pragma pack(push,1)
		struct CSMData
		{
			std::array<Mat4,4> VP;
			Vector4 fard;
			int32_t count;
		};
#pragma pack(pop)

		virtual bool BindLights(Anvil::DescriptorSet &descSetShadowMaps,Anvil::DescriptorSet &descSetLightSources);
		virtual bool BindScene(const Scene &scene,bool bView);
	protected:
		ShaderSceneLit(prosper::Context &context,const std::string &identifier,const std::string &vsShader,const std::string &fsShader,const std::string &gsShader="");
		virtual uint32_t GetLightDescriptorSetIndex() const=0;
	};

	/////////////////////

	class DLLCLIENT ShaderEntity
		: public ShaderSceneLit
	{
	public:
		static prosper::ShaderGraphics::VertexBinding VERTEX_BINDING_BONE_WEIGHT;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_BONE_WEIGHT_ID;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_BONE_WEIGHT;

		static prosper::ShaderGraphics::VertexBinding VERTEX_BINDING_VERTEX;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_POSITION;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_UV;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_NORMAL;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_TANGENT;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_BI_TANGENT;

		static prosper::ShaderGraphics::VertexBinding VERTEX_BINDING_LIGHTMAP;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_LIGHTMAP_UV;

		static prosper::Shader::DescriptorSetInfo DESCRIPTOR_SET_INSTANCE;

#pragma pack(push,1)
		struct InstanceData
		{
			enum class RenderFlags : uint32_t
			{
				None = 0u,
				Weighted = 1u
			};
			Mat4 modelMatrix;
			Vector4 color;
			RenderFlags renderFlags;
		};
#pragma pack(pop)

		bool BindInstanceDescriptorSet(Anvil::DescriptorSet &descSet);
		virtual bool BindEntity(CBaseEntity &ent);
		virtual bool BindVertexAnimationOffset(uint32_t offset);
		virtual bool BindScene(const Scene &scene,bool bView) override;
		virtual bool Draw(CModelSubMesh &mesh);
		virtual void EndDraw() override;
	protected:
		ShaderEntity(prosper::Context &context,const std::string &identifier,const std::string &vsShader,const std::string &fsShader,const std::string &gsShader="");
		bool Draw(CModelSubMesh &mesh,bool bUseVertexWeightBuffer);
		bool Draw(CModelSubMesh &mesh,const std::function<bool(CModelSubMesh&)> &fDraw,bool bUseVertexWeightBuffer);

		virtual uint32_t GetInstanceDescriptorSetIndex() const=0;
		virtual void GetVertexAnimationPushConstantInfo(uint32_t &offset) const=0;

		CBaseEntity *m_boundEntity = nullptr;
	};
};
REGISTER_BASIC_BITWISE_OPERATORS(pragma::ShaderEntity::InstanceData::RenderFlags);
REGISTER_BASIC_BITWISE_OPERATORS(pragma::ShaderScene::DebugFlags);

#endif