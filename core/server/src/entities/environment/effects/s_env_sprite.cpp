#include "stdafx_server.h"
#include "pragma/entities/environment/effects/s_env_sprite.h"
#include "pragma/entities/s_entityfactories.h"
#include "pragma/lua/s_lentity_handles.hpp"

using namespace pragma;

LINK_ENTITY_TO_CLASS(env_sprite,EnvSprite);

void SSpriteComponent::SendData(NetPacket &packet,nwm::RecipientFilter &rp)
{
	packet->WriteString(m_spritePath);
	packet->Write<float>(m_size);
	packet->Write<float>(m_bloomScale);
	packet->Write<Color>(m_color);
	packet->Write<uint32_t>(m_particleRenderMode);
	packet->Write<float>(m_tFadeIn);
	packet->Write<float>(m_tFadeOut);
}

luabind::object SSpriteComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<SSpriteComponentHandleWrapper>(l);}

void EnvSprite::Initialize()
{
	SBaseEntity::Initialize();
	AddComponent<SSpriteComponent>();
}