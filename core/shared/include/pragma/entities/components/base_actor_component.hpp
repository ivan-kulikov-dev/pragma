#ifndef __BASE_ACTOR_COMPONENT_HPP__
#define __BASE_ACTOR_COMPONENT_HPP__

#include "pragma/entities/components/base_entity_component.hpp"
#include "pragma/entities/entity_component_event.hpp"
#include <sharedutils/property/util_property.hpp>

namespace pragma
{
	struct DLLNETWORK CEOnCharacterKilled
		: public ComponentEvent
	{
		CEOnCharacterKilled(DamageInfo *damageInfo);
		virtual void PushArguments(lua_State *l) override;
		DamageInfo *damageInfo;
	};
	class DLLNETWORK BaseActorComponent
		: public BaseEntityComponent
	{
	public:
		static ComponentEventId EVENT_ON_KILLED;
		static ComponentEventId EVENT_ON_RESPAWN;
		static ComponentEventId EVENT_ON_DEATH;
		static void RegisterEvents(pragma::EntityComponentManager &componentManager);

		virtual void Initialize() override;
		virtual void Kill(DamageInfo *dmgInfo=nullptr);
		virtual void Respawn();
		virtual void SetFrozen(bool b);
		bool IsFrozen() const;
		bool IsAlive() const;
		bool IsDead() const;
		void Ragdolize();
		bool FindHitgroup(const PhysCollisionObject &phys,HitGroup &hitgroup) const;
		PhysObjHandle GetHitboxPhysicsObject() const;

		const util::PBoolProperty &GetFrozenProperty() const;

		void SetMoveController(const std::string &moveController);
		int32_t GetMoveController() const;
	protected:
		BaseActorComponent(BaseEntity &ent);
		bool m_bAlive;
		util::PBoolProperty m_bFrozen = nullptr;
		std::string m_moveControllerName = "move_yaw";
		int32_t m_moveController = -1;
		struct DLLNETWORK HitboxData
		{
			HitboxData(uint32_t boneId,const Vector3 &offset);
			uint32_t boneId;
			Vector3 offset;
		};
		pragma::NetEventId m_netEvSetFrozen = pragma::INVALID_NET_EVENT;
		std::vector<HitboxData> m_hitboxData;
		std::unique_ptr<PhysObj> m_physHitboxes;
		virtual void OnPhysicsInitialized();
		virtual void OnPhysicsDestroyed();
		virtual void PhysicsUpdate(double tDelta);
		virtual void OnDeath(DamageInfo *dmgInfo);
		void UpdateHitboxPhysics();
		void UpdateMoveController();
		void InitializeMoveController();
	};
};

#endif