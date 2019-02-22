#include "stdafx_shared.h"
#include <pragma/game/game.h>
#include "pragma/physics/raytraces.h"
#include "pragma/physics/physcollisionobject.h"
#include "pragma/physics/physshape.h"
#include "pragma/entities/components/base_transform_component.hpp"
#include "pragma/entities/components/base_physics_component.hpp"
#include "pragma/entities/entity_iterator.hpp"
#include "pragma/entities/components/damageable_component.hpp"
#include <pragma/math/intersection.h>

bool Game::IsServer() {return false;}
bool Game::IsClient() {return false;}

void Game::SplashDamage(const Vector3 &origin,Float radius,DamageInfo &dmg,const std::function<bool(BaseEntity*,DamageInfo&)> &callback)
{
	auto &force = dmg.GetForce();
	auto forceLen = uvec::length(force);
	auto damage = dmg.GetDamage();
	if(radius <= 0.f)
		return;
	struct EntityCandidate
	{
		EntityCandidate(BaseEntity *ent,Vector3 &pos,Float dist)
			: hEntity(ent->GetHandle()),position(pos),distance(dist)
		{}
		EntityHandle hEntity;
		Vector3 position;
		Float distance;
	};
	std::vector<EntityCandidate> ents;
	EntityIterator entIt {*this};
	entIt.AttachFilter<EntityIteratorFilterSphere>(origin,radius);
	for(auto *ent : entIt)
	{
		auto pTrComponent = ent->GetTransformComponent();
		auto pPhysComponent = ent->GetPhysicsComponent();
		Vector3 min {};
		Vector3 max {};
		if(pPhysComponent.valid())
			pPhysComponent->GetCollisionBounds(&min,&max);
		Vector3 pos;
		Geometry::ClosestPointOnAABBToPoint(min,max,origin -pTrComponent->GetPosition(),&pos);
		auto dist = uvec::length(pos);
		ents.push_back(EntityCandidate(ent,pos,dist));
	}
	std::vector<EntityHandle> traceFilter;
	traceFilter.reserve(2);
	auto *attacker = dmg.GetAttacker();
	if(attacker != nullptr)
		traceFilter.push_back(attacker->GetHandle());
	auto *inflictor = dmg.GetInflictor();
	if(inflictor != nullptr)
		traceFilter.push_back(inflictor->GetHandle());
	auto *entOrigin = (attacker != nullptr) ? attacker : inflictor;
	for(auto it=ents.begin();it!=ents.end();++it)
	{
		auto &c = *it;
		if(c.hEntity.IsValid())
		{
			auto *ent = c.hEntity.get();
			auto pTrComponent = ent->GetTransformComponent();
			if(pTrComponent.expired())
				continue;
			auto pos = c.position;

			auto dir = pos -origin;
			auto l = uvec::length(dir);
			auto pPhysComponent = ent->GetPhysicsComponent();
			if(l == 0.f) // Damage origin is within entity
			{
				pos = pTrComponent->GetPosition();
				if(pPhysComponent.valid())
					pos += pPhysComponent->GetCollisionCenter();
				dir = pos -origin;
				l = uvec::length(dir);
				if(l == 0.f) // Damage origin is at entity center
				{
					pos = pTrComponent->GetPosition();
					dir = pos -origin;
					l = uvec::length(dir);
					if(l == 0.f) // Damage origin is at entity origin
						dir = uvec::create_random_unit_vector(); // Just use a random direction
				}
			}
			if(l != 0.f)
				dir /= l;

			TraceData data;
			data.SetSource(origin);
			data.SetTarget(pos);
			data.SetFilter(traceFilter);
			if(entOrigin != nullptr && pPhysComponent.valid())
			{
				data.SetCollisionFilterGroup(pPhysComponent->GetCollisionFilter());
				data.SetCollisionFilterMask(pPhysComponent->GetCollisionFilterMask());
			}
			auto r = RayCast(data);
			if(r.hit == true && r.entity.get() != ent)
			{
				auto pPhysComponent = ent->GetPhysicsComponent();
				auto &pos = pTrComponent->GetPosition();
				auto center = (pPhysComponent.valid()) ? pPhysComponent->GetCollisionCenter() : Vector3{};
				Vector3 min {};
				Vector3 max {};
				if(pPhysComponent.valid())
					pPhysComponent->GetCollisionBounds(&min,&max);
				min = pos +center +(min -center) *0.5f;
				data.SetTarget(min);
				r = RayCast(data);
				if(r.hit == true && r.entity.get() != ent)
				{
					max = pos +center +(max -center) *0.5f;
					data.SetTarget(max);
					r = RayCast(data);
				}
			}
			if(r.hit == false || r.entity.get() == ent)
			{
				auto pDamageableComponent = ent->GetComponent<pragma::DamageableComponent>();
				if(pDamageableComponent.valid())
				{
					auto scale = (radius -c.distance) /radius;
					DamageInfo dmgEnt;
					dmgEnt.SetAttacker(dmg.GetAttacker());
					dmgEnt.SetInflictor(dmg.GetInflictor());
					dmgEnt.SetDamage(CUInt16(CFloat(damage) *scale));
					dmgEnt.SetSource(const_cast<Vector3&>(origin));
					dmgEnt.SetDamageType(static_cast<DAMAGETYPE>(dmg.GetDamageTypes()));
					dmgEnt.SetForce(dir *(forceLen *scale));
					if(callback == nullptr || callback(ent,dmgEnt) == true)
						pDamageableComponent->TakeDamage(dmgEnt);
				}
			}
		}
	}
}
void Game::SplashDamage(const Vector3 &origin,Float radius,UInt32 damage,Float force,BaseEntity *attacker,BaseEntity *inflictor,const std::function<bool(BaseEntity*,DamageInfo&)> &callback)
{
	DamageInfo info;
	info.SetForce(Vector3(force,0.f,0.f));
	info.SetAttacker(attacker);
	info.SetInflictor(inflictor);
	info.SetDamage(CUInt16(damage));
	info.SetDamageType(DAMAGETYPE::EXPLOSION);
	SplashDamage(origin,radius,info,callback);
}
void Game::SplashDamage(const Vector3 &origin,Float radius,UInt32 damage,Float force,const EntityHandle &attacker,const EntityHandle &inflictor,const std::function<bool(BaseEntity*,DamageInfo&)> &callback)
{
	SplashDamage(origin,radius,damage,force,attacker.get(),inflictor.get(),callback);
}