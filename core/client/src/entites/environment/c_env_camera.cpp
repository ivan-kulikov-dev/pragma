#include "stdafx_client.h"
#include "pragma/entities/environment/c_env_camera.h"
#include "pragma/entities/components/c_player_component.hpp"
#include "pragma/entities/c_entityfactories.h"
#include "pragma/entities/shared_spawnflags.h"
#include <pragma/networking/nwm_util.h>
#include "pragma/lua/c_lentity_handles.hpp"
#include <pragma/entities/components/basetoggle.h>
#include <pragma/entities/components/base_transform_component.hpp>

using namespace pragma;

LINK_ENTITY_TO_CLASS(env_camera,CEnvCamera);

extern DLLCLIENT CGame *c_game;

CCameraComponent::~CCameraComponent()
{
	if(m_cbCameraUpdate.IsValid())
		m_cbCameraUpdate.Remove();
}
util::EventReply CCameraComponent::HandleEvent(ComponentEventId eventId,ComponentEvent &evData)
{
	if(BaseEnvCameraComponent::HandleEvent(eventId,evData) == util::EventReply::Handled)
		return util::EventReply::Handled;
	if(eventId == BaseToggleComponent::EVENT_ON_TURN_ON)
	{
		if(m_cbCameraUpdate.IsValid())
			m_cbCameraUpdate.Remove();
		auto *pl = c_game->GetLocalPlayer();
		if(pl != nullptr)
			pl->SetObserverMode(OBSERVERMODE::THIRDPERSON);
		m_cbCameraUpdate = c_game->AddCallback("CalcView",FunctionCallback<void,std::reference_wrapper<Vector3>,std::reference_wrapper<Quat>>::Create([this](std::reference_wrapper<Vector3> refPos,std::reference_wrapper<Quat> refRot) {
			auto &ent = GetEntity();
			auto *pToggleComponent = static_cast<pragma::BaseToggleComponent*>(ent.FindComponent("toggle").get());
			if(pToggleComponent != nullptr && pToggleComponent->IsTurnedOn() == false)
				return;
			auto pTrComponent = ent.GetTransformComponent();
			auto &pos = refPos.get();
			auto &rot = refRot.get();
			if(pTrComponent.expired())
			{
				pos = {};
				rot = uquat::identity();
			}
			else
			{
				pos = pTrComponent->GetPosition();
				rot = pTrComponent->GetOrientation();
			}
			//c_game->SetCameraPosition(&GetPosition(),&GetOrientation());
		}));
	}
	else if(eventId == BaseToggleComponent::EVENT_ON_TURN_OFF)
	{
		auto *pl = c_game->GetLocalPlayer();
		if(pl != nullptr)
			pl->SetObserverMode(OBSERVERMODE::FIRSTPERSON);
		if(m_cbCameraUpdate.IsValid())
			m_cbCameraUpdate.Remove();
		c_game->SetCameraPosition(NULL,NULL);
	}
	return util::EventReply::Unhandled;
}
luabind::object CCameraComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<CCameraComponentHandleWrapper>(l);}

/////////

void CEnvCamera::Initialize()
{
	CBaseEntity::Initialize();
	AddComponent<CCameraComponent>();
}