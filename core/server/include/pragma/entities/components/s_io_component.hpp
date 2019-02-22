#ifndef __S_IO_COMPONENT_HPP__
#define __S_IO_COMPONENT_HPP__

#include "pragma/serverdefinitions.h"
#include "pragma/entities/components/s_entity_component.hpp"
#include <pragma/entities/components/base_io_component.hpp>

namespace pragma
{
	class DLLSERVER SIOComponent final
		: public BaseIOComponent
	{
	public:
		SIOComponent(BaseEntity &ent) : BaseIOComponent(ent) {}
		virtual void Initialize() override;
		virtual luabind::object InitializeLuaObject(lua_State *l) override;
	};
};

#endif