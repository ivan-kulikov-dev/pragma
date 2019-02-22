#include "stdafx_shared.h"
#include "pragma/networkstate/networkstate.h"
#include <pragma/game/game.h>
#include "pragma/entities/point/constraints/point_constraint_conetwist.h"
#include "pragma/physics/physenvironment.h"
#include "pragma/physics/physconstraint.h"
#include "pragma/entities/baseentity.h"
#include "pragma/physics/physobj.h"
#include <sharedutils/util.h>
#include "pragma/util/util_handled.hpp"
#include "pragma/entities/components/base_physics_component.hpp"
#include "pragma/entities/components/base_transform_component.hpp"
#include "pragma/entities/baseentity_events.hpp"

using namespace pragma;

void BasePointConstraintConeTwistComponent::Initialize()
{
	BasePointConstraintComponent::Initialize();

	BindEvent(BaseEntity::EVENT_HANDLE_KEY_VALUE,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		auto &kvData = static_cast<CEKeyValueData&>(evData.get());
		if(ustring::compare(kvData.key,"swingspan1",false))
			m_kvSwingSpan1 = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"swingspan2",false))
			m_kvSwingSpan2 = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"twistspan",false))
			m_kvTwistSpan = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"softness",false))
			m_kvSoftness = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"biasfactor",false))
			m_kvBiasFactor = util::to_float(kvData.value);
		else if(ustring::compare(kvData.key,"relaxationfactor",false))
			m_kvRelaxationFactor = util::to_float(kvData.value);
		else
			return util::EventReply::Unhandled;
		return util::EventReply::Handled;
	});
}

void BasePointConstraintConeTwistComponent::InitializeConstraint(BaseEntity *src,BaseEntity *tgt)
{
	auto &entThis = GetEntity();
	auto *state = entThis.GetNetworkState();
	auto *game = state->GetGameState();
	auto *physEnv = game->GetPhysicsEnvironment();

	auto pPhysComponentSrc = src->GetPhysicsComponent();
	auto *physSrc = pPhysComponentSrc.valid() ? dynamic_cast<RigidPhysObj*>(pPhysComponentSrc->GetPhysicsObject()) : nullptr;
	auto pPhysComponentTgt = tgt->GetPhysicsComponent();
	auto *physTgt = pPhysComponentTgt.valid() ? dynamic_cast<RigidPhysObj*>(pPhysComponentTgt->GetPhysicsObject()) : nullptr;
	if(physSrc == nullptr || physTgt == nullptr || !physSrc->IsRigid() || !physTgt->IsRigid())
		return;
	auto *rigidSrc = static_cast<RigidPhysObj*>(physSrc)->GetRigidBody();
	auto *rigidTgt = static_cast<RigidPhysObj*>(physTgt)->GetRigidBody();
	if(rigidSrc == nullptr || rigidTgt == nullptr)
		return;

	auto pTrComponent = entThis.GetTransformComponent();
	auto pTrComponentTgt = tgt->GetTransformComponent();
	auto originConstraint = pTrComponent.valid() ? pTrComponent->GetPosition() : Vector3{};
	auto originTgt = pTrComponentTgt.valid() ? pTrComponentTgt->GetPosition() : Vector3{};

	auto rotConstraint = pTrComponent.valid() ? pTrComponent->GetOrientation() : uquat::identity();
	auto rotTgt = pTrComponentTgt.valid() ? pTrComponentTgt->GetOrientation() : uquat::identity();

	auto pTrComponentSrc = src->GetTransformComponent();
	originTgt = originConstraint -originTgt;
	originConstraint -= pTrComponentSrc.valid() ? pTrComponentSrc->GetPosition() : Vector3{};
	
	auto swingSpan1 = CFloat(umath::deg_to_rad(m_kvSwingSpan1));
	auto swingSpan2 = CFloat(umath::deg_to_rad(m_kvSwingSpan2));
	auto twistSpan = CFloat(umath::deg_to_rad(m_kvTwistSpan));

	auto *coneTwist = physEnv->CreateConeTwistConstraint(
		rigidSrc,originConstraint,rotConstraint,
		rigidTgt,originTgt,rotTgt
	);
	if(coneTwist != nullptr)
	{
		coneTwist->SetLimit(
			swingSpan1,swingSpan2,twistSpan,
			m_kvSoftness,m_kvBiasFactor,
			m_kvRelaxationFactor
		);
		m_constraints.push_back(coneTwist->GetHandle());
	}
}