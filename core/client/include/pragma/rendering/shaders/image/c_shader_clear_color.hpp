#ifndef __C_SHADER_CLEAR_COLOR_HPP__
#define __C_SHADER_CLEAR_COLOR_HPP__

#include "pragma/clientdefinitions.h"
#include <shader/prosper_shader.hpp>

namespace pragma
{
	class DLLCLIENT ShaderClearColor
		: public prosper::ShaderGraphics
	{
	public:
		static prosper::ShaderGraphics::VertexBinding VERTEX_BINDING_VERTEX;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_POSITION;

#pragma pack(push,1)
		struct PushConstants
		{
			Vector4 clearColor;
		};
#pragma pack(pop)

		ShaderClearColor(prosper::Context &context,const std::string &identifier);
		ShaderClearColor(prosper::Context &context,const std::string &identifier,const std::string &vsShader,const std::string &fsShader,const std::string &gsShader="");

		bool Draw(const PushConstants &pushConstants={{0.f,0.f,0.f,0.f}});
	protected:
		virtual void InitializeGfxPipeline(Anvil::GraphicsPipelineCreateInfo &pipelineInfo,uint32_t pipelineIdx) override;
	};
};

#endif