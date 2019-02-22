#ifndef __C_FUNCBUTTON_H__
#define __C_FUNCBUTTON_H__
#include "pragma/clientdefinitions.h"
#include "pragma/entities/c_baseentity.h"
#include "pragma/entities/func/basefuncbutton.h"

namespace pragma
{
	class DLLCLIENT CButtonComponent final
		: public BaseFuncButtonComponent
	{
	public:
		CButtonComponent(BaseEntity &ent) : BaseFuncButtonComponent(ent) {}
		virtual void Initialize() override;
		virtual luabind::object InitializeLuaObject(lua_State *l) override;
	};
};

class DLLCLIENT CFuncButton
	: public CBaseEntity
{
public:
	virtual void Initialize() override;
};

#endif