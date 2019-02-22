#include "stdafx_server.h"
#include "pragma/entities/environment/audio/s_env_sound.h"
#include "pragma/entities/s_entityfactories.h"
#include <sharedutils/util_string.h>
#include <sharedutils/util.h>
#include <pragma/audio/alsound.h>
#include "pragma/audio/s_alsound.h"
#include "pragma/lua/s_lentity_handles.hpp"
#include <pragma/audio/alsound_type.h>
#include <pragma/entities/components/map_component.hpp>

using namespace pragma;

LINK_ENTITY_TO_CLASS(env_sound,EnvSound);

void SSoundComponent::Initialize()
{
	BaseEnvSoundComponent::Initialize();
}
void SSoundComponent::OnSoundCreated(ALSound &snd)
{
	BaseEnvSoundComponent::OnSoundCreated(snd);
	auto pMapComponent = GetEntity().GetComponent<pragma::MapComponent>();
	dynamic_cast<SALSound&>(snd).SetEntityMapIndex(pMapComponent.valid() ? pMapComponent->GetMapIndex() : 0u);
}
void SSoundComponent::SendData(NetPacket &packet,nwm::RecipientFilter &rp)
{
	packet->Write<float>(m_kvMaxDist);
	auto idx = (m_sound != nullptr) ? m_sound->GetIndex() : std::numeric_limits<uint32_t>::max();
	packet->Write<uint32_t>(idx);
}

luabind::object SSoundComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<SSoundComponentHandleWrapper>(l);}

/////////////

void EnvSound::Initialize()
{
	SBaseEntity::Initialize();
	AddComponent<SSoundComponent>();
}