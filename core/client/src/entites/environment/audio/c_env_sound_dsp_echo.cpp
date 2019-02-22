#include "stdafx_client.h"
#include "pragma/entities/environment/audio/c_env_sound_dsp_echo.h"
#include "pragma/entities/c_entityfactories.h"
#include "pragma/c_engine.h"
#include "pragma/entities/components/c_player_component.hpp"
#include <pragma/networking/nwm_util.h>
#include "pragma/lua/c_lentity_handles.hpp"
#include <alsoundsystem.hpp>

using namespace pragma;

extern DLLCENGINE CEngine *c_engine;

LINK_ENTITY_TO_CLASS(env_sound_dsp_echo,CEnvSoundDspEcho);

void CSoundDspEchoComponent::ReceiveData(NetPacket &packet)
{
	m_kvDelay = packet->Read<float>();
	m_kvLRDelay = packet->Read<float>();
	m_kvDamping = packet->Read<float>();
	m_kvFeedback = packet->Read<float>();
	m_kvSpread = packet->Read<float>();
}

void CSoundDspEchoComponent::OnEntitySpawn()
{
	//BaseEnvSoundDspEcho::OnEntitySpawn(); // Not calling BaseEnvSoundDspEcho::OnEntitySpawn() to skip the dsp effect lookup
	auto *soundSys = c_engine->GetSoundSystem();
	if(soundSys == nullptr)
		return;
	al::EfxEchoProperties props {};
	props.flDelay = m_kvDelay;
	props.flLRDelay = m_kvLRDelay;
	props.flDamping = m_kvDamping;
	props.flFeedback = m_kvFeedback;
	props.flSpread = m_kvSpread;
	m_dsp = soundSys->CreateEffect(props);
}
luabind::object CSoundDspEchoComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<CSoundDspEchoComponentHandleWrapper>(l);}

//////////////////

void CEnvSoundDspEcho::Initialize()
{
	CBaseEntity::Initialize();
	AddComponent<CSoundDspEchoComponent>();
}