#ifndef __S_ENV_CAMERA_H__
#define __S_ENV_CAMERA_H__

#include "pragma/serverdefinitions.h"
#include "pragma/entities/s_baseentity.h"
#include "pragma/entities/environment/env_camera.h"
#include "pragma/entities/components/s_entity_component.hpp"

namespace pragma
{
	class DLLSERVER SCameraComponent final
		: public BaseEnvCameraComponent
	{
	public:
		SCameraComponent(BaseEntity &ent) : BaseEnvCameraComponent(ent) {}
		virtual luabind::object InitializeLuaObject(lua_State *l) override;
	};
};

class DLLSERVER EnvCamera
	: public SBaseEntity
{
public:
	virtual void Initialize() override;
};

#endif