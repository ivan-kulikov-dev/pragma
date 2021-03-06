/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2020 Florian Weischer */

#include "stdafx_server.h"
#include "pragma/networking/s_nwm_util.h"
#include "pragma/entities/environment/lights/s_env_light_spot.h"
#include "pragma/entities/s_entityfactories.h"
#include "pragma/entities/baseentity_luaobject.h"
#include <pragma/networking/nwm_util.h>
#include <pragma/networking/enums.hpp>
#include "pragma/lua/s_lentity_handles.hpp"
#include <pragma/entities/entity_component_system_t.hpp>

using namespace pragma;

LINK_ENTITY_TO_CLASS(env_light_spot,EnvLightSpot);

void SLightSpotComponent::SendData(NetPacket &packet,networking::ClientRecipientFilter &rp)
{
	packet->Write<float>(*m_angOuterCutoff);
	packet->Write<float>(*m_angInnerCutoff);
	packet->Write<float>(*m_coneStartOffset);
}

void SLightSpotComponent::SetConeStartOffset(float offset)
{
	BaseEnvLightSpotComponent::SetConeStartOffset(offset);
	NetPacket p {};
	p->Write<float>(offset);
	static_cast<SBaseEntity&>(GetEntity()).SendNetEvent(m_netEvSetConeStartOffset,p,pragma::networking::Protocol::SlowReliable);
}

void SLightSpotComponent::SetOuterCutoffAngle(float ang)
{
	BaseEnvLightSpotComponent::SetOuterCutoffAngle(ang);
	auto &ent = GetEntity();
	if(!ent.IsSpawned())
		return;
	NetPacket p;
	nwm::write_entity(p,&ent);
	p->Write<float>(ang);
	server->SendPacket("env_light_spot_outercutoff_angle",p,pragma::networking::Protocol::SlowReliable);
}

void SLightSpotComponent::SetInnerCutoffAngle(float ang)
{
	BaseEnvLightSpotComponent::SetInnerCutoffAngle(ang);
	auto &ent = GetEntity();
	if(!ent.IsSpawned())
		return;
	NetPacket p;
	nwm::write_entity(p,&ent);
	p->Write<float>(ang);
	server->SendPacket("env_light_spot_innercutoff_angle",p,pragma::networking::Protocol::SlowReliable);
}

luabind::object SLightSpotComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<SLightSpotComponentHandleWrapper>(l);}

void SLightSpotComponent::OnEntityComponentAdded(BaseEntityComponent &component)
{
	BaseEntityComponent::OnEntityComponentAdded(component);
	if(typeid(component) == typeid(SLightComponent))
		static_cast<SLightComponent&>(component).SetLight(*this);
}

///////////

void EnvLightSpot::Initialize()
{
	SBaseEntity::Initialize();
	AddComponent<SLightComponent>();
	AddComponent<SLightSpotComponent>();
}
