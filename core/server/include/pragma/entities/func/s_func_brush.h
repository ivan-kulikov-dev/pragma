#ifndef __S_FUNC_BRUSH_H__
#define __S_FUNC_BRUSH_H__
#include "pragma/serverdefinitions.h"
#include "pragma/entities/s_baseentity.h"
#include "pragma/entities/func/basefuncbrush.h"
#include "pragma/entities/components/s_entity_component.hpp"

namespace pragma
{
	class DLLSERVER SBrushComponent final
		: public BaseFuncBrushComponent,
		public SBaseNetComponent
	{
	public:
		SBrushComponent(BaseEntity &ent) : BaseFuncBrushComponent(ent) {}
		virtual void Initialize() override;
		virtual void OnEntitySpawn() override;
		virtual void SendData(NetPacket &packet,nwm::RecipientFilter &rp) override;
		virtual bool ShouldTransmitNetData() const override {return true;}
		virtual luabind::object InitializeLuaObject(lua_State *l) override;
	};
};

class DLLSERVER FuncBrush
	: public SBaseEntity
{
public:
	virtual void Initialize() override;
};

#endif