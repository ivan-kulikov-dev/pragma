#include "stdafx_shared.h"
#include <pragma/definitions.h>
#include <pragma/engine.h>
#include "pragma/entities/components/base_player_component.hpp"
#include <pragma/math/angle/wvquaternion.h>
#include <pragma/physics/movetypes.h>
#include "pragma/physics/physobj.h"
#include <mathutil/umat.h>
#include "pragma/entities/baseflashlight.h"
#include "pragma/physics/physxquerysinglefiltercallback.h"
#include "pragma/entities/baseentity.h"
#include <pragma/game/game.h>
#include "pragma/physics/raytraces.h"
#include "pragma/physics/collisionmasks.h"
#include <sharedutils/util.h>
#include "pragma/physics/physshape.h"
#include "pragma/physics/physenvironment.h"
#include "pragma/game/damageinfo.h"
#include "pragma/util/ammo_type.h"
#include "pragma/lua/luacallback.h"
#include "luasystem.h"
#include "pragma/lua/luafunction_call.h"
#include "pragma/physics/physcontroller.h"
#include "pragma/entities/components/basetoggle.h"
#include "pragma/util/util_game.hpp"
#include "pragma/entities/entity_iterator.hpp"
#include "pragma/entities/components/usable_component.hpp"
#include "pragma/entities/components/base_character_component.hpp"
#include "pragma/entities/components/base_observable_component.hpp"
#include "pragma/entities/components/base_health_component.hpp"
#include "pragma/entities/baseplayer.hpp"
#include "pragma/entities/components/base_physics_component.hpp"
#include "pragma/entities/components/base_transform_component.hpp"
#include "pragma/entities/components/base_model_component.hpp"
#include "pragma/entities/components/base_animated_component.hpp"
#include "pragma/entities/components/velocity_component.hpp"
#include "pragma/entities/components/logic_component.hpp"
#include "pragma/model/model.h"

using namespace pragma;


ComponentEventId BasePlayerComponent::EVENT_HANDLE_ACTION_INPUT = pragma::INVALID_COMPONENT_ID;
void BasePlayerComponent::RegisterEvents(pragma::EntityComponentManager &componentManager)
{
	auto componentType = std::type_index(typeid(BasePlayerComponent));
	EVENT_HANDLE_ACTION_INPUT = componentManager.RegisterEvent("HANDLE_ACTION_INPUT",componentType);
}

#ifdef PHYS_ENGINE_BULLET
void BasePlayerComponent::SetStandHeight(float height)
{
	m_standHeight = height;
}
void BasePlayerComponent::SetCrouchHeight(float height)
{
	m_crouchHeight = height;
}
void BasePlayerComponent::OnTakenDamage(DamageInfo &info,unsigned short oldHealth,unsigned short newHealth)
{
	if(oldHealth == 0 || newHealth != 0)
		return;
	auto charComponent = GetEntity().GetCharacterComponent();
	if(charComponent.valid())
		charComponent->Kill(&info);
}
void BasePlayerComponent::OnKilled(DamageInfo *dmgInfo)
{
	SetObserverMode(OBSERVERMODE::THIRDPERSON);
	auto &ent = GetEntity();
	auto *nw = ent.GetNetworkState();
	auto *game = nw->GetGameState();
	game->CallCallbacks<void,BasePlayerComponent*,DamageInfo*>("OnPlayerDeath",this,dmgInfo);

	auto charComponent = ent.GetCharacterComponent();
	charComponent->RemoveWeapons();
}
util::EventReply BasePlayerComponent::HandleEvent(ComponentEventId eventId,ComponentEvent &evData)
{
	if(BaseEntityComponent::HandleEvent(eventId,evData) == util::EventReply::Handled)
		return util::EventReply::Handled;
	if(eventId == BaseCharacterComponent::EVENT_ON_KILLED)
		OnKilled(static_cast<const CEOnCharacterKilled&>(evData).damageInfo);
	else if(eventId == BaseCharacterComponent::EVENT_ON_RESPAWN)
		OnRespawn();
	else if(eventId == BaseHealthComponent::EVENT_ON_TAKEN_DAMAGE)
	{
		auto &healthInfo = static_cast<pragma::CEOnTakenDamage&>(evData);
		OnTakenDamage(healthInfo.damageInfo,healthInfo.oldHealth,healthInfo.newHealth);
	}
	return util::EventReply::Unhandled;
}
bool BasePlayerComponent::CanUnCrouch() const
{
	if(m_shapeStand == nullptr)
		return true;
	auto &ent = GetEntity();
	auto pPhysComponent = ent.GetPhysicsComponent();
	auto *phys = pPhysComponent.valid() ? pPhysComponent->GetPhysicsObject() : nullptr;
	if(phys == nullptr)
		return true;
	auto *col = phys->GetCollisionObject();
	if(col == nullptr)
		return false;
	if(ent.IsCharacter() == false)
		return false;
	auto pTrComponent = ent.GetTransformComponent();
	auto &charComponent = *ent.GetCharacterComponent();
	auto colPos = col->GetPos();
	auto *nw = ent.GetNetworkState();
	auto *game = nw->GetGameState();
	auto *shape = m_shapeStand->GetConvexShape();
	auto data = charComponent.GetAimTraceData();
	data.SetSource(colPos);
	data.SetTarget(colPos +(pTrComponent.valid() ? pTrComponent->GetUp() : uvec::UP) *0.001f); // Target position musn't be the same as the source position, otherwise the trace will never detect a hit
	data.SetSource(shape);
	TraceResult result;
	return !game->Sweep(data,&result); // Overlap only works with collision objects, not with individual shapes, so we have to use Sweep instead
}
void BasePlayerComponent::Think(double tDelta)
{
	auto &ent = GetEntity();
	auto pPhysComponent = ent.GetPhysicsComponent();
	auto *phys = pPhysComponent.valid() ? pPhysComponent->GetPhysicsObject() : nullptr;
	if(m_bCrouching || m_crouchTransition != CrouchTransition::None)
	{
		if(GetActionInput(Action::Crouch) == false && m_crouchTransition != CrouchTransition::Uncrouching)
			UnCrouch();
		else if(m_crouchTransition != CrouchTransition::None)
		{
			NetworkState *state = ent.GetNetworkState();
			Game *game = state->GetGameState();
			if(game->CurTime() >= m_tCrouch)
			{
				m_tCrouch = 0.f;
				if(m_crouchTransition == CrouchTransition::Crouching)
				{
					m_bCrouching = true;
					if(phys != NULL && phys->IsController())
					{
						auto *controller = static_cast<ControllerPhysObj*>(phys);
						if(controller->IsCapsule())
						{
							auto *capsule = static_cast<CapsuleControllerPhysObj*>(controller);
							capsule->SetHeight(m_crouchHeight);
						}
						OnFullyCrouched();
					}
				}
				else
				{
					m_bCrouching = false;
					if(phys != NULL && phys->IsController())
					{
						auto *controller = static_cast<ControllerPhysObj*>(phys);
						if(controller->IsCapsule())
						{
							auto *capsule = static_cast<CapsuleControllerPhysObj*>(controller);
							capsule->SetHeight(m_standHeight);
						}
						OnFullyUnCrouched();
					}
				}
				m_crouchTransition = CrouchTransition::None;
			}
		}
	}

	// Update animation
	auto animComponent = ent.GetAnimatedComponent();
	if(animComponent.valid() && (phys == nullptr || phys->IsController())) // Only run this if not in ragdoll mode
	{
		auto pTrComponent = ent.GetTransformComponent();
		auto pVelComponent = ent.GetComponent<pragma::VelocityComponent>();
		auto &vel = pVelComponent.valid() ? pVelComponent->GetVelocity() : Vector3{};
		auto speed = uvec::length(vel);
		auto charComponent = ent.GetCharacterComponent();
		const auto movementSpeedThreshold = 0.04f;
		if(speed >= movementSpeedThreshold)
		{
			if((m_bForceAnimationUpdate == true || animComponent->GetActivity() == animComponent->TranslateActivity(Activity::Idle)) && PlaySharedActivity(IsWalking() ? Activity::Walk : Activity::Run) == true)
				m_movementActivity = animComponent->GetActivity();
			m_bForceAnimationUpdate = false;
			
			if(pTrComponent.valid())
			{
				auto rot = pTrComponent->GetOrientation();
				auto rotView = charComponent.valid() ? charComponent->GetViewOrientation() : rot;
				auto rotRef = charComponent.valid() ? charComponent->GetOrientationAxesRotation() : rot;
				rotView = rotRef *rotView;
				rot = rotRef *rot;
				auto ang = EulerAngles(rot);
				ang.y = EulerAngles(rotView).y;
				rot = uquat::get_inverse(rotRef) *uquat::create(ang);
				pTrComponent->SetOrientation(rot);
			}
		}
		else
		{
			auto bMoving = IsMoving();
			if(m_bForceAnimationUpdate == true || bMoving == true)
			{
				m_bForceAnimationUpdate = false;
				PlaySharedActivity(Activity::Idle);
				if(bMoving == true)
					animComponent->SetLastAnimationBlendScale(1.f -(charComponent.valid() ? charComponent->GetMovementBlendScale() : 0.f));
			}
		}
	}
}

bool BasePlayerComponent::IsMoving() const
{
	auto &ent = GetEntity();
	auto animComponent = ent.GetAnimatedComponent();
	return (animComponent.valid() && animComponent->GetActivity() == m_movementActivity && m_movementActivity != Activity::Invalid) ? true : false;
}
bool BasePlayerComponent::IsWalking() const {return GetActionInput(Action::Walk);}
bool BasePlayerComponent::IsSprinting() const {return (!IsWalking() && GetActionInput(Action::Sprint)) ? true : false;}
#elif PHYS_ENGINE_PHYSX
void BasePlayerComponent::Think(double tDelta)
{
	BaseCharacter::Think(tDelta);
	if(m_bCrouching || m_crouchTransition != -1)
	{
		if(GetActionInput(ACTION::CROUCH) == false && m_crouchTransition != 1)
			UnCrouch();
		else if(m_crouchTransition != -1)
		{
			NetworkState *state = m_entity->GetNetworkState();
			Game *game = state->GetGameState();
			if(game->CurTime() >= m_tCrouch)
			{
				m_tCrouch = 0.f;
				if(m_crouchTransition == 0)
				{
					m_bCrouching = true;
					PhysObj *phys = m_entity->GetPhysicsObject();
					if(phys != NULL && phys->IsController())
					{
						ControllerPhysObj *physController = static_cast<ControllerPhysObj*>(phys);
						physx::PxController *controller = physController->GetController();
						physx::PxExtendedVec3 pos = controller->getFootPosition();
						controller->resize(m_crouchHeight *0.5f);
						controller->setFootPosition(pos);
						OnFullyCrouched();
					}
				}
				else
				{
					m_bCrouching = false;
					PhysObj *phys = m_entity->GetPhysicsObject();
					if(phys != NULL && phys->IsController())
					{
						ControllerPhysObj *physController = static_cast<ControllerPhysObj*>(phys);
						physx::PxController *controller = physController->GetController();
						controller->resize(m_standHeight *0.5f);
						controller->setPosition(controller->getPosition());
						OnFullyUnCrouched();
					}
				}
				m_crouchTransition = -1;
			}
		}
	}
}
void BasePlayerComponent::SetMoveType(MOVETYPE movetype)
{

}
void BasePlayerComponent::SetStandHeight(float height)
{
	m_standHeight = height;
	if(IsCrouching())
		return;
	PhysObj *phys = m_entity->GetPhysicsObject();
	if(phys == NULL || !phys->IsController())
		return;
	ControllerPhysObj *physController = static_cast<ControllerPhysObj*>(phys);
	physx::PxController *controller = physController->GetController();
	controller->resize(height *0.5f);
}
void BasePlayerComponent::SetCrouchHeight(float height)
{
	m_crouchHeight = height;
	if(!IsCrouching())
		return;
	PhysObj *phys = m_entity->GetPhysicsObject();
	if(phys == NULL || !phys->IsController())
		return;
	ControllerPhysObj *physController = static_cast<ControllerPhysObj*>(phys);
	physx::PxController *controller = physController->GetController();
	controller->resize(height *0.5f);
}
bool BasePlayerComponent::CanUnCrouch()
{
	if(!m_bCrouching && m_crouchTransition != -1)
		return true;
	PhysObj *phys = m_entity->GetPhysicsObject();
	if(phys == NULL || !phys->IsController())
		return true;
	ControllerPhysObj *physController = static_cast<ControllerPhysObj*>(phys);
	if(physController->IsCapsule())
	{
		physx::PxCapsuleController *controller = static_cast<physx::PxCapsuleController*>(physController->GetController());
		physx::PxScene *scene = controller->getScene();

		physx::PxReal r = controller->getRadius();
		physx::PxCapsuleGeometry geom(r,m_standHeight *0.5f);

		physx::PxExtendedVec3 position = controller->getFootPosition();
		position += controller->getUpDirection() *(r +m_standHeight *0.5f +controller->getContactOffset());
		physx::PxVec3 pos(position.x,position.y,position.z);
		physx::PxOverlapBuffer hit;
		PhysXQuerySingleFilterCallback filterCall(controller->getActor());
		physx::PxQueryFilterData filter(physx::PxQueryFlag::eANY_HIT | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER);
		unsigned int filterGroup;
		unsigned int filterMask;
		m_entity->GetCollisionFilter(&filterGroup,&filterMask);
		filter.data.word0 = filterGroup;
		filter.data.word1 = filterMask;
		if(scene->overlap(
				geom,
				physx::PxTransform(pos,controller->getActor()->getGlobalPose().q),
				hit,
				physx::PxQueryFilterData(physx::PxQueryFlag::eANY_HIT | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER),
				&filterCall
			) && hit.hasBlock)
		{
			physx::PxRigidActor *actor = hit.block.actor;
			if(actor->userData != NULL)
			{
				PhysObj *phys = static_cast<PhysObj*>(actor->userData);
				BaseEntity *ent = phys->GetOwner();
				if(ent != NULL)
				{
					physx::PxTransform t = hit.block.shape->getLocalPose();
					std::cout<<"BLOCKED BY "<<ent->GetClass()<<" ("<<t.p.x<<","<<t.p.y<<","<<t.p.z<<")"<<std::endl;
				}
			}
			return false;
		}
	}
	else
	{
		physx::PxBoxController *controller = static_cast<physx::PxBoxController*>(physController->GetController());
		physx::PxScene *scene = controller->getScene();

		physx::PxReal h = m_standHeight -m_crouchHeight;
		physx::PxVec3 halfExtents(
			controller->getHalfForwardExtent(),
			h,
			controller->getHalfSideExtent()
		);
		physx::PxBoxGeometry geom(halfExtents);
		physx::PxExtendedVec3 position = controller->getFootPosition();
		physx::PxVec3 pos(position.x,position.y +controller->getHalfHeight() +h *0.5f,position.z);
		physx::PxOverlapBuffer hit;
		if(scene->overlap(
				geom,
				physx::PxTransform(pos,controller->getActor()->getGlobalPose().q),
				hit,
				physx::PxQueryFilterData(physx::PxQueryFlag::eANY_HIT | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER)
			) && hit.hasBlock)
			return false;
	}
	return true;
}
#endif

Con::c_cout& BasePlayerComponent::print(Con::c_cout &os)
{
	auto &ent = GetEntity();
	os<<"Player["<<GetPlayerName()<<"]["<<ent.GetIndex()<<"]"<<"["<<ent.GetClass()<<"]"<<"[";
	auto mdlComponent = ent.GetModelComponent();
	auto hMdl = mdlComponent.valid() ? mdlComponent->GetModel() : nullptr;
	if(hMdl == nullptr)
		os<<"NULL";
	else
		os<<hMdl->GetName();
	os<<"]";
	return os;
}

std::ostream& BasePlayerComponent::print(std::ostream &os)
{
	auto &ent = GetEntity();
	os<<"Player["<<GetPlayerName()<<"]["<<ent.GetIndex()<<"]"<<"["<<ent.GetClass()<<"]"<<"[";
	auto mdlComponent = ent.GetModelComponent();
	auto hMdl = mdlComponent.valid() ? mdlComponent->GetModel() : nullptr;
	if(hMdl == nullptr)
		os<<"NULL";
	else
		os<<hMdl->GetName();
	os<<"]";
	return os;
}

extern DLLENGINE Engine *engine;
BasePlayerComponent::BasePlayerComponent(BaseEntity &ent)
	: BaseEntityComponent(ent),
	m_portUDP(0),
	m_playerName(""),
	m_speedWalk(63.33f),
	m_speedRun(190),
	m_speedSprint(320),
	m_speedCrouchWalk(63.33f),
	m_bCrouching(false),
	m_standHeight(72),m_crouchHeight(36),
	m_standEyeLevel(64),m_crouchEyeLevel(28),
	m_tCrouch(0),m_bFlashlightOn(false),
	m_obsMode(util::TEnumProperty<OBSERVERMODE>::Create(OBSERVERMODE::FIRSTPERSON)),
	m_bObserverCameraLocked(false)
{
	m_bLocalPlayer = false;
	m_timeConnected = 0;
}

BasePlayerComponent::~BasePlayerComponent()
{
	//if(m_sprite != NULL)
	//	server->RemoveSprite(m_sprite); // WEAVETODO
	if(m_entFlashlight.IsValid())
		m_entFlashlight->Remove();
}

void BasePlayerComponent::OnRespawn()
{
	auto &ent = GetEntity();
	auto *nw = ent.GetNetworkState();
	auto *game = nw->GetGameState();
	game->CallCallbacks<void,BasePlayerComponent*>("OnPlayerSpawned",this);
}

BasePlayer *BasePlayerComponent::GetBasePlayer() const {return dynamic_cast<BasePlayer*>(m_hBasePlayer.get());}

void BasePlayerComponent::Initialize()
{
	BaseEntityComponent::Initialize();
	m_netEvSetObserverTarget = SetupNetEvent("set_observer_target");
	m_netEvSetObserverCameraOffset = SetupNetEvent("set_observer_camera_offset");
	m_netEvSetObserverCameraLocked = SetupNetEvent("set_observer_camera_locked");
	m_netEvApplyViewRotationOffset = SetupNetEvent("apply_view_rotation_offset");
	m_netEvPrintMessage = SetupNetEvent("print_message");
	m_netEvRespawn = SetupNetEvent("respawn");
	m_netEvSetViewOrientation = SetupNetEvent("set_view_orientation");

	auto &ent = GetEntity();
	ent.AddComponent("character");
	m_hBasePlayer = ent.GetHandle();

	BindEvent(BaseCharacterComponent::EVENT_CALC_MOVEMENT_SPEED,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		static_cast<pragma::CECalcMovementSpeed&>(evData.get()).speed = CalcMovementSpeed();
		return util::EventReply::Handled;
	});
	BindEvent(BaseCharacterComponent::EVENT_CALC_AIR_MOVEMENT_MODIFIER,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		static_cast<pragma::CECalcAirMovementModifier&>(evData.get()).airMovementModifier = CalcAirMovementModifier();
		return util::EventReply::Handled;
	});
	BindEvent(BaseCharacterComponent::EVENT_CALC_MOVEMENT_ACCELERATION,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		static_cast<pragma::CECalcMovementAcceleration&>(evData.get()).acceleration = CalcMovementAcceleration();
		return util::EventReply::Handled;
	});
	BindEvent(BaseCharacterComponent::EVENT_CALC_MOVEMENT_DIRECTION,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		auto &movementDirData = static_cast<pragma::CECalcMovementDirection&>(evData.get());
		movementDirData.direction = CalcMovementDirection(movementDirData.forward,movementDirData.right);
		return util::EventReply::Handled;
	});
	BindEventUnhandled(BaseAnimatedComponent::EVENT_ON_ANIMATION_COMPLETE,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		auto mdlComponent = GetEntity().GetModelComponent();
		auto hMdl = mdlComponent.valid() ? mdlComponent->GetModel() : nullptr;
		if(hMdl == nullptr)
			return util::EventReply::Unhandled;
		auto anim = hMdl->GetAnimation(static_cast<CEOnAnimationComplete&>(evData.get()).animation);
		if(anim == nullptr)
			return util::EventReply::Unhandled;
		if(anim->HasFlag(FAnim::Loop) == false)
			PlaySharedActivity(Activity::Idle); // A non-looping animation has completed; Switch back to idle
		return util::EventReply::Unhandled;
	});
	BindEventUnhandled(BaseAnimatedComponent::EVENT_ON_ANIMATION_START,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		m_movementActivity = Activity::Invalid;
		return util::EventReply::Unhandled;
	});
	BindEventUnhandled(BaseAnimatedComponent::EVENT_TRANSLATE_ACTIVITY,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		if((IsCrouching() == true && m_crouchTransition != CrouchTransition::Uncrouching) || m_crouchTransition == CrouchTransition::Crouching)
		{
			auto &activity = static_cast<CETranslateActivity&>(evData.get()).activity;
			switch(activity)
			{
				case Activity::Idle:
					activity = Activity::CrouchIdle;
					return util::EventReply::Handled;
				case Activity::Walk:
				case Activity::Run:
					activity = Activity::CrouchWalk;
					return util::EventReply::Handled;
			}
		}
		return util::EventReply::Unhandled;
	});
	BindEventUnhandled(BasePhysicsComponent::EVENT_ON_PHYSICS_INITIALIZED,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		OnPhysicsInitialized();
		return util::EventReply::Unhandled;
	});
	BindEventUnhandled(LogicComponent::EVENT_ON_TICK,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		Think(static_cast<CEOnTick&>(evData.get()).deltaTime);
		return util::EventReply::Unhandled;
	});
	BindEvent(BaseCharacterComponent::EVENT_IS_MOVING,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		static_cast<CEIsMoving&>(evData.get()).moving = IsMoving();
		return util::EventReply::Handled;
	});
	BindEventUnhandled(BaseCharacterComponent::EVENT_ON_JUMP,[this](std::reference_wrapper<pragma::ComponentEvent> evData) {
		PlaySharedActivity(Activity::Jump);
	});

	auto whObservableComponent = ent.FindComponent("observable");
	if(whObservableComponent.valid())
	{
		auto *pObservableComponent = static_cast<BaseObservableComponent*>(whObservableComponent.get());
		pObservableComponent->SetFirstPersonObserverOffsetEnabled(true);
		pObservableComponent->SetThirdPersonObserverOffsetEnabled(true);
		pObservableComponent->SetThirdPersonObserverOffset({0.f,10.f,-80.f});
	}
}

void BasePlayerComponent::OnPhysicsInitialized()
{
	auto &ent = GetEntity();
	auto pPhysComponent = ent.GetPhysicsComponent();
	if(pPhysComponent.valid())
		pPhysComponent->AddCollisionFilter(CollisionMask::Player);
}

BaseEntity *BasePlayerComponent::FindUseEntity() const
{
	auto &ent = GetEntity();
	NetworkState *state = ent.GetNetworkState();
	Game *game = state->GetGameState();
	auto charComponent = ent.GetCharacterComponent();
	auto pTrComponent = ent.GetTransformComponent();
	Vector3 origin = charComponent.valid() ? charComponent->GetEyePosition() : (pTrComponent.valid() ? pTrComponent->GetPosition() : Vector3{});
	Vector3 viewDir = charComponent.valid() ? charComponent->GetViewForward() : (pTrComponent.valid() ? pTrComponent->GetForward() : Vector3{});
	const auto dotMin = 0.6f;
	float dotClosest = 1.f;
	float maxDist = 96.f;
	float distClosest = std::numeric_limits<float>::max();
	BaseEntity *entClosest = NULL;
	EntityIterator it{*game};
	it.AttachFilter<TEntityIteratorFilterComponent<pragma::UsableComponent>>();
	for(auto *entOther : it)
	{
		auto pUsableComponent = entOther->GetComponent<pragma::UsableComponent>();
		if(pUsableComponent->CanUse(const_cast<BaseEntity*>(&ent)))
		{
			auto pTrComponentOther = entOther->GetTransformComponent();
			if(pTrComponentOther.expired())
				continue;
			auto pPhysComponentOther = entOther->GetPhysicsComponent();
			auto &posEnt = pTrComponentOther->GetPosition();
			Vector3 min {};
			Vector3 max {};
			if(pPhysComponentOther.valid())
				pPhysComponentOther->GetCollisionBounds(&min,&max);
			min += posEnt;
			max += posEnt;
			Vector3 res;
			Geometry::ClosestPointOnAABBToPoint(min,max,origin,&res);
			
			float dist = glm::distance(origin,res);
			if(dist <= maxDist)
			{
				Vector3 dir = res -origin;
				uvec::normalize(&dir);
				auto dot = uvec::dot(viewDir,dir);
				if(dot >= dotMin && ((dot -dotClosest) >= 0.2f || (distClosest -dist) >= 20.f))
				{
					TraceData data;
					data.SetSource(origin);
					data.SetTarget(origin +dir *dist);
					data.SetFilter(entOther->GetHandle());
					auto result = game->RayCast(data);
					if(!result.hit || result.entity.get() == entOther)
					{
						dotClosest = dot;
						entClosest = entOther;
					}
				}
			}
		}
	}
	return entClosest;
}

void BasePlayerComponent::Use()
{
	BaseEntity *ent = FindUseEntity();
	if(ent == NULL)
		return;
	auto pUsableComponent = ent->GetComponent<pragma::UsableComponent>();
	if(pUsableComponent.valid())
		pUsableComponent->OnUse(&GetEntity());
}

void BasePlayerComponent::SetFlashlight(bool b)
{
	if(m_entFlashlight.IsValid() == false)
		return;
	auto *toggleComponent = static_cast<pragma::BaseToggleComponent*>(m_entFlashlight.get()->FindComponent("toggle").get());
	if(toggleComponent != nullptr)
		toggleComponent->SetTurnedOn(b);
}
void BasePlayerComponent::ToggleFlashlight() {if(IsFlashlightOn()) SetFlashlight(false); else SetFlashlight(true);}
bool BasePlayerComponent::IsFlashlightOn() const
{
	if(m_entFlashlight.IsValid() == false)
		return false;
	auto *toggleComponent = static_cast<pragma::BaseToggleComponent*>(m_entFlashlight.get()->FindComponent("toggle").get());
	return (toggleComponent != nullptr) ? toggleComponent->IsTurnedOn() : false;
}

Vector2 BasePlayerComponent::CalcMovementSpeed() const
{
	float speed;
	auto physComponent = GetEntity().GetPhysicsComponent();
	if(physComponent.valid() && physComponent->GetMoveType() == MOVETYPE::NOCLIP)
	{
		speed = GetEntity().GetNetworkState()->GetGameState()->GetConVarFloat("sv_noclip_speed");
		if(IsWalking())
			speed *= 0.5f;
		else if(IsSprinting())
			speed *= 2.f;
	}
	else if(IsCrouching())
		speed = GetCrouchedWalkSpeed();
	else if(IsWalking())
		speed = GetWalkSpeed();
	else if(IsSprinting())
		speed = GetSprintSpeed();
	else
		speed = GetRunSpeed();
	return {speed,0.f};
}
float BasePlayerComponent::CalcAirMovementModifier() const {return GetEntity().GetNetworkState()->GetGameState()->GetConVarFloat("sv_player_air_move_scale");}
float BasePlayerComponent::CalcMovementAcceleration() const {return GetEntity().GetNetworkState()->GetGameState()->GetConVarFloat("sv_acceleration");}
Vector3 BasePlayerComponent::CalcMovementDirection(const Vector3 &forward,const Vector3 &right) const
{
	Vector3 dir {};
	if(GetActionInput(Action::MoveForward))
		dir += forward *GetActionInputAxisMagnitude(Action::MoveForward);
	if(GetActionInput(Action::MoveBackward))
		dir -= forward *GetActionInputAxisMagnitude(Action::MoveBackward);
	if(GetActionInput(Action::MoveRight))
		dir += right *GetActionInputAxisMagnitude(Action::MoveRight);
	if(GetActionInput(Action::MoveLeft))
		dir -= right *GetActionInputAxisMagnitude(Action::MoveLeft);
	return dir;
}

void BasePlayerComponent::SetUDPPort(unsigned short port)
{
	m_portUDP = port;
}

void BasePlayerComponent::SetLocalPlayer(bool b) {m_bLocalPlayer = b;}

unsigned short BasePlayerComponent::GetUDPPort() const {return m_portUDP;}

void BasePlayerComponent::SetPlayerName(std::string name)
{
	m_playerName = name;
}

std::string BasePlayerComponent::GetClientIP() {return "[::]:0";}
unsigned short BasePlayerComponent::GetClientPort() {return 0;}

void BasePlayerComponent::OnEntitySpawn()
{
	BaseEntityComponent::OnEntitySpawn();

	auto &ent = GetEntity();
	NetworkState *state = ent.GetNetworkState();
	m_timeConnected = state->RealTime();

	auto pPhysComponent = ent.GetPhysicsComponent();
	if(pPhysComponent.valid())
		pPhysComponent->SetCollisionBounds(Vector3(-16,0,-16),Vector3(16,m_standHeight,16));
	PlaySharedActivity(Activity::Idle);
}

void BasePlayerComponent::GetConVars(std::unordered_map<std::string,std::string> **convars) {*convars = &m_conVars;}

bool BasePlayerComponent::GetConVar(std::string cvar,std::string *val)
{
	std::unordered_map<std::string,std::string>::iterator i = m_conVars.find(cvar);
	if(i == m_conVars.end())
		return false;
	*val = i->second;
	return true;
}

std::string BasePlayerComponent::GetConVarString(std::string cvar) const
{
	std::unordered_map<std::string,std::string>::iterator i = const_cast<BasePlayerComponent*>(this)->m_conVars.find(cvar);
	if(i == m_conVars.end())
		return "";
	return i->second;
}

int BasePlayerComponent::GetConVarInt(std::string cvar) const
{
	std::unordered_map<std::string,std::string>::iterator i = const_cast<BasePlayerComponent*>(this)->m_conVars.find(cvar);
	if(i == m_conVars.end())
		return 0;
	return atoi(i->second.c_str());
}

float BasePlayerComponent::GetConVarFloat(std::string cvar) const
{
	std::unordered_map<std::string,std::string>::iterator i = const_cast<BasePlayerComponent*>(this)->m_conVars.find(cvar);
	if(i == m_conVars.end())
		return 0;
	return util::to_float(i->second);
}

bool BasePlayerComponent::GetConVarBool(std::string cvar) const
{
	std::unordered_map<std::string,std::string>::iterator i = const_cast<BasePlayerComponent*>(this)->m_conVars.find(cvar);
	if(i == m_conVars.end())
		return false;
	return i->second != "0";
}

double BasePlayerComponent::TimeConnected() const {return GetEntity().GetNetworkState()->RealTime() -m_timeConnected;}

double BasePlayerComponent::ConnectionTime() const {return m_timeConnected;}

std::string BasePlayerComponent::GetPlayerName() const {return m_playerName;}

void BasePlayerComponent::SetObserverMode(OBSERVERMODE mode) {*m_obsMode = mode;}
OBSERVERMODE BasePlayerComponent::GetObserverMode() const {return *m_obsMode;}
const util::PEnumProperty<OBSERVERMODE> &BasePlayerComponent::GetObserverModeProperty() const {return m_obsMode;}
void BasePlayerComponent::SetObserverTarget(BaseObservableComponent *ent)
{
	m_hEntObserverTarget = {};
	if(ent == nullptr)
		return;
	m_hEntObserverTarget = ent->GetHandle<BaseObservableComponent>();
}
BaseObservableComponent *BasePlayerComponent::GetObserverTarget() const
{
	auto *r = m_hEntObserverTarget.get();
	if(r == nullptr)
	{
		auto pObsComponent = GetEntity().FindComponent("observable");
		return static_cast<BaseObservableComponent*>(pObsComponent.get());
	}
	return r;
}
void BasePlayerComponent::SetObserverCameraOffset(const Vector3 &offset) {m_observerOffset = offset;}
const Vector3 &BasePlayerComponent::GetObserverCameraOffset() const {return m_observerOffset;}
bool BasePlayerComponent::IsObserverCameraLocked() const {return m_bObserverCameraLocked;}
void BasePlayerComponent::SetObserverCameraLocked(bool b) {m_bObserverCameraLocked = b;}

void BasePlayerComponent::SetViewPos(const Vector3 &pos) {m_posView = pos;}
const Vector3 &BasePlayerComponent::GetViewPos() const {return m_posView;}

void BasePlayerComponent::OnActionInputChanged(Action action,bool b)
{
	//Con::cwar<<"Action input "<<umath::to_integral(action)<<" has changed for player "<<m_entity<<": "<<b<<Con::endl;
	if(action == Action::Walk || action == Action::Sprint)
		m_bForceAnimationUpdate = true;
}

Action BasePlayerComponent::GetActionInputs() const {return m_actionInputs;}
Action BasePlayerComponent::GetRawActionInputs() const {return m_rawInputs;}
void BasePlayerComponent::SetActionInputs(Action actions,bool bKeepMagnitudes)
{
	auto rawInputs = m_rawInputs;
	auto valuesOld = umath::get_power_of_2_values(umath::to_integral(rawInputs));
	for(auto v : valuesOld)
	{
		if((actions &static_cast<Action>(v)) == Action::None) // Action has been unpressed
			SetActionInput(static_cast<Action>(v),false);
	}
	actions &= ~rawInputs;
	auto values = umath::get_power_of_2_values(umath::to_integral(actions));
	for(auto v : values)
		SetActionInput(static_cast<Action>(v),true,bKeepMagnitudes);
}
void BasePlayerComponent::SetActionInput(Action action,bool b,bool bKeepMagnitude)
{
	SetActionInput(action,b,(bKeepMagnitude == true) ? GetActionInputAxisMagnitude(action) : ((b == true) ? 1.f : 0.f));
}
void BasePlayerComponent::SetActionInput(Action action,bool b,float magnitude)
{
	SetActionInputAxisMagnitude(action,(b == true) ? magnitude : 0.f);
	if(((m_rawInputs &action) != Action::None) == b)
		return;
	if(b == false)
		m_rawInputs &= ~action;
	else
		m_rawInputs |= action;

	CEHandleActionInput evData{action,b,magnitude};
	if(InvokeEventCallbacks(EVENT_HANDLE_ACTION_INPUT,evData) == util::EventReply::Handled)
		return;
	auto &ent = GetEntity();
	auto *nw = ent.GetNetworkState();
	auto *game = nw->GetGameState();
	auto r = false;
	if(game->CallCallbacks<bool,BasePlayerComponent*,Action,bool>("OnActionInput",&r,this,action,b) == CallbackReturnType::HasReturnValue)
	{
		if(r == false)
			return;
	}
	if(b == false)
	{
		if(GetActionInput(action) == true)
		{
			m_actionInputs &= ~action;
			OnActionInputChanged(action,b);
		}
		return;
	}
	if(GetActionInput(action))
		return;
	m_actionInputs |= action;
	OnActionInputChanged(action,b);
	auto charComponent = ent.GetCharacterComponent();
	switch(action)
	{
		case Action::Jump:
		{
			if(charComponent.valid())
				charComponent->Jump();
			break;
		}
		case Action::Crouch:
		{
			Crouch();
			break;
		}
		case Action::Attack:
		{
			if(charComponent.valid())
				charComponent->PrimaryAttack();
			break;
		}
		case Action::Attack2:
		{
			if(charComponent.valid())
				charComponent->SecondaryAttack();
			break;
		}
		case Action::Attack3:
		{
			if(charComponent.valid())
				charComponent->TertiaryAttack();
			break;
		}
		case Action::Attack4:
		{
			if(charComponent.valid())
				charComponent->Attack4();
			break;
		}
		case Action::Reload:
		{
			if(charComponent.valid())
				charComponent->ReloadWeapon();
			break;
		}
		case Action::Use:
		{
			Use();
			break;
		}
	}
}

bool BasePlayerComponent::PlaySharedActivity(Activity activity)
{
	auto animComponent = GetEntity().GetAnimatedComponent();
	return animComponent.valid() ? animComponent->PlayActivity(activity) : false;
}

float BasePlayerComponent::GetStandHeight() const {return m_standHeight;}
float BasePlayerComponent::GetCrouchHeight() const {return m_crouchHeight;}
float BasePlayerComponent::GetStandEyeLevel() const {return m_standEyeLevel;}
float BasePlayerComponent::GetCrouchEyeLevel() const {return m_crouchEyeLevel;}
void BasePlayerComponent::SetStandEyeLevel(float eyelevel) {m_standEyeLevel = eyelevel;}
void BasePlayerComponent::SetCrouchEyeLevel(float eyelevel) {m_crouchEyeLevel = eyelevel;}

void BasePlayerComponent::OnCrouch() {}
void BasePlayerComponent::OnUnCrouch() {}
void BasePlayerComponent::OnFullyCrouched() {}
void BasePlayerComponent::OnFullyUnCrouched() {}
void BasePlayerComponent::Crouch()
{
	if((m_bCrouching && m_crouchTransition != CrouchTransition::Uncrouching) || m_crouchTransition == CrouchTransition::Crouching)
		return;
	auto &ent = GetEntity();
	auto pPhysComponent = ent.GetPhysicsComponent();
	auto *phys = pPhysComponent.valid() ? pPhysComponent->GetPhysicsObject() : nullptr;
	m_shapeStand = nullptr;
	NetworkState *state = ent.GetNetworkState();
	Game *game = state->GetGameState();
	if(phys != nullptr && phys->IsController())
	{
		auto *controllerPhys = static_cast<ControllerPhysObj*>(phys);
		assert(controllerPhys->IsCapsule());
		if(!controllerPhys->IsCapsule())
			Con::cwar<<"WARNING: Box-controller crouching not implemented!"<<Con::endl;
		auto *controller = controllerPhys->GetController();
		auto shape = controller->GetShape();
		if(shape != nullptr && controllerPhys->IsCapsule())
		{
			auto *btShape = static_cast<btCapsuleShape*>(shape->GetShape());
			auto radius = btShape->getRadius();
			auto halfHeight = btShape->getHalfHeight();
			auto w = util::metres_to_units(radius);
			auto btH = util::metres_to_units(halfHeight) *2.0; // Half-Height; Multiply by 2 to get full height
			auto h = (btH +w *2.0) /2.0;
			auto *physEnv = game->GetPhysicsEnvironment();
			m_shapeStand = physEnv->CreateCapsuleShape(w,h);
		}
	}

	m_crouchTransition = CrouchTransition::Crouching;
	m_bForceAnimationUpdate = true;
	m_tCrouch = CFloat(game->CurTime()) +0.2f;
	OnCrouch();
}

void BasePlayerComponent::UnCrouch(bool bForce)
{
	if((!m_bCrouching && m_crouchTransition != CrouchTransition::Crouching) || m_crouchTransition == CrouchTransition::Uncrouching)
		return;
	if(!bForce && !CanUnCrouch())
		return;
	m_shapeStand = nullptr;
	m_crouchTransition = CrouchTransition::Uncrouching;
	auto &ent = GetEntity();
	NetworkState *state = ent.GetNetworkState();
	Game *game = state->GetGameState();
	m_bForceAnimationUpdate = true;
	m_tCrouch = CFloat(game->CurTime()) +0.4f;
	OnUnCrouch();
}

bool BasePlayerComponent::IsCrouching() const {return m_bCrouching;}

bool BasePlayerComponent::GetActionInput(Action action) const {return ((m_actionInputs &action) != Action::None) ? true : false;}
bool BasePlayerComponent::GetRawActionInput(Action action) const {return ((m_rawInputs &action) != Action::None) ? true : false;}
const std::unordered_map<Action,float> &BasePlayerComponent::GetActionInputAxisMagnitudes() const {return m_inputAxes;}
float BasePlayerComponent::GetActionInputAxisMagnitude(Action action) const
{
	auto it = m_inputAxes.find(action);
	if(it == m_inputAxes.end())
		return 0.f;
	return it->second;
}
void BasePlayerComponent::SetActionInputAxisMagnitude(Action action,float magnitude) {m_inputAxes[action] = magnitude;}

bool BasePlayerComponent::IsLocalPlayer() const {return m_bLocalPlayer;}

bool BasePlayerComponent::IsKeyDown(int key)
{
	return m_keysPressed[key];
}

float BasePlayerComponent::GetWalkSpeed() const
{
	auto r = m_speedWalk;
	auto pTrComponent = GetEntity().GetTransformComponent();
	if(pTrComponent.valid())
	{
		auto &scale = pTrComponent->GetScale();
		r *= umath::abs_max(scale.x,scale.y,scale.z);
	}
	return r;
}
float BasePlayerComponent::GetRunSpeed() const
{
	auto r = m_speedRun;
	auto pTrComponent = GetEntity().GetTransformComponent();
	if(pTrComponent.valid())
	{
		auto &scale = pTrComponent->GetScale();
		r *= umath::abs_max(scale.x,scale.y,scale.z);
	}
	return r;
}
float BasePlayerComponent::GetSprintSpeed() const
{
	auto r = m_speedSprint;
	auto pTrComponent = GetEntity().GetTransformComponent();
	if(pTrComponent.valid())
	{
		auto &scale = pTrComponent->GetScale();
		r *= umath::abs_max(scale.x,scale.y,scale.z);
	}
	return r;
}
void BasePlayerComponent::SetWalkSpeed(float speed) {m_speedWalk = speed;}
void BasePlayerComponent::SetRunSpeed(float speed) {m_speedRun = speed;}
void BasePlayerComponent::SetSprintSpeed(float speed) {m_speedSprint = speed;}
float BasePlayerComponent::GetCrouchedWalkSpeed() const
{
	auto r = m_speedCrouchWalk;
	auto pTrComponent = GetEntity().GetTransformComponent();
	if(pTrComponent.valid())
	{
		auto &scale = pTrComponent->GetScale();
		r *= umath::abs_max(scale.x,scale.y,scale.z);
	}
	return r;
}
void BasePlayerComponent::SetCrouchedWalkSpeed(float speed) {m_speedCrouchWalk = speed;}

void BasePlayerComponent::PrintMessage(std::string message,MESSAGE)
{}

//////////////////

CEHandleActionInput::CEHandleActionInput(Action action,bool pressed,float magnitude)
	: action{action},pressed{pressed},magnitude{magnitude}
{}
void CEHandleActionInput::PushArguments(lua_State *l)
{
	Lua::PushInt(l,umath::to_integral(action));
	Lua::PushBool(l,pressed);
	Lua::PushNumber(l,magnitude);
}