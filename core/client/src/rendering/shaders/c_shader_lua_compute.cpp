#include "stdafx_client.h"
#include "pragma/lua/classes/c_lshader.h"
#include "pragma/rendering/shaders/c_shader_lua.hpp"
#include <buffers/prosper_buffer.hpp>
#include <prosper_command_buffer.hpp>
#include <misc/compute_pipeline_create_info.h>

void Lua::ComputePipelineCreateInfo::AddSpecializationConstant(lua_State *l,Anvil::ComputePipelineCreateInfo &pipelineInfo,uint32_t constantId,::DataStream &ds)
{
	Lua::PushBool(l,pipelineInfo.add_specialization_constant(constantId,ds->GetSize(),ds->GetData()));
}

void Lua::Shader::Compute::RecordDispatch(lua_State *l,prosper::ShaderCompute &shader,uint32_t x,uint32_t y,uint32_t z)
{
	Lua::PushBool(l,shader.RecordDispatch(x,y,z));
}
void Lua::Shader::Compute::RecordBeginCompute(lua_State *l,prosper::ShaderCompute &shader,Lua::Vulkan::CommandBuffer &hCommandBuffer,uint32_t pipelineIdx)
{
	if((*hCommandBuffer)->get_command_buffer_type() != Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY)
		return;
	Lua::PushBool(l,shader.BeginCompute(std::static_pointer_cast<prosper::PrimaryCommandBuffer>(hCommandBuffer),pipelineIdx));
}
void Lua::Shader::Compute::RecordCompute(lua_State *l,prosper::ShaderCompute &shader)
{
	shader.Compute();
	Lua::PushBool(l,true);
}
void Lua::Shader::Compute::RecordEndCompute(lua_State *l,prosper::ShaderCompute &shader)
{
	shader.EndCompute();
	Lua::PushBool(l,true);
}