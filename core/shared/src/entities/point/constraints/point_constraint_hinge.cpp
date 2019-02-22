#include "stdafx_shared.h"
#include "pragma/networkstate/networkstate.h"
#include <pragma/game/game.h>
#include "pragma/entities/point/constraints/point_constraint_hinge.h"
#include "pragma/physics/physenvironment.h"
#include "pragma/physics/physconstraint.h"
#include "pragma/entities/baseentity.h"
#include "pragma/physics/physobj.h"
#include "pragma/physics/physcollisionobject.h"
#include <sharedutils/util.h>
#include "pragma/util/util_handled.hpp"
#include "pragma/entities/components/base_physics_component.hpp"
#include "pragma/entities/components/base_transform_component.hpp"
#include "pragma/entities/baseentity_events.hpp"

using namespace pragma;

void BasePointConstraintHingeComponent::Initialize()
{
	BasePointConstraintComponent::Initialize();

	BindEvent(BaseEntity::EVENT_HANDLE_KEY_VALUE,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		auto &kvData = static_cast<CEKeyValueData&>(evData.get());
		if(ustring::compare(kvData.key,"limit_low",false))
			m_kvLimitLow = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"limit_high",false))
			m_kvLimitHigh = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"softness",false))
			m_kvLimitSoftness = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"biasfactor",false))
			m_kvLimitBiasFactor = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"relaxationfactor",false))
			m_kvLimitRelaxationFactor = util::to_float(kvData.value);
		else
			return util::EventReply::Unhandled;
		return util::EventReply::Handled;
	});
}

void BasePointConstraintHingeComponent::InitializeConstraint(BaseEntity *src,BaseEntity *tgt)
{
	auto pPhysComponentTgt = tgt->GetPhysicsComponent();
	auto *physTgt = pPhysComponentTgt.valid() ? dynamic_cast<RigidPhysObj*>(pPhysComponentTgt->GetPhysicsObject()) : nullptr;
	if(physTgt == nullptr)
		return;
	auto pPhysComponentSrc = src->GetPhysicsComponent();
	auto *physSrc = pPhysComponentSrc.valid() ? dynamic_cast<RigidPhysObj*>(pPhysComponentSrc->GetPhysicsObject()) : nullptr;
	if(physSrc == nullptr)
		return;
	auto *bodySrc = physSrc->GetRigidBody();
	if(bodySrc == nullptr)
		return;
	auto &entThis = GetEntity();
	auto *state = entThis.GetNetworkState();
	auto *game = state->GetGameState();
	auto *physEnv = game->GetPhysicsEnvironment();
	auto pTrComponent = entThis.GetTransformComponent();
	auto posThis = pTrComponent.valid() ? pTrComponent->GetPosition() : Vector3{};
	auto axis = m_posTarget -posThis;
	uvec::normalize(&axis);

	auto &bodies = physTgt->GetRigidBodies();
	for(auto it=bodies.begin();it!=bodies.end();++it)
	{
		auto &bodyTgt = *it;
		if(bodyTgt.IsValid())
		{
			auto posTgt = bodyTgt->GetPos();
			auto *hinge = physEnv->CreateHingeConstraint(static_cast<PhysRigidBody*>(bodyTgt.get()),posThis -posTgt,bodySrc,posThis,axis);
			if(hinge != nullptr)
				m_constraints.push_back(hinge->GetHandle());
		}
	}
}