#ifndef __C_ENV_SMOKE_TRAIL_H__
#define __C_ENV_SMOKE_TRAIL_H__

#include "pragma/clientdefinitions.h"
#include "pragma/entities/c_baseentity.h"
#include "pragma/entities/environment/effects/env_smoke_trail.h"
#include "pragma/entities/components/c_entity_component.hpp"

namespace pragma
{
	class CParticleSystemComponent;
	class DLLCLIENT CSmokeTrailComponent final
		: public BaseEnvSmokeTrailComponent,
		public CBaseNetComponent
	{
	public:
		CSmokeTrailComponent(BaseEntity &ent) : BaseEnvSmokeTrailComponent(ent) {}
		virtual ~CSmokeTrailComponent() override;
		virtual void Initialize() override;
		virtual void ReceiveData(NetPacket &packet) override;
		virtual util::EventReply HandleEvent(ComponentEventId eventId,ComponentEvent &evData) override;
		virtual luabind::object InitializeLuaObject(lua_State *l) override;
		virtual bool ShouldTransmitNetData() const override {return true;}
		virtual void OnEntitySpawn() override;
	protected:
		util::WeakHandle<CParticleSystemComponent> m_hParticle = {};
		void InitializeParticle();
		void DestroyParticle();
	};
};

class DLLCLIENT CEnvSmokeTrail
	: public CBaseEntity
{
public:
	virtual void Initialize() override;
};

#endif