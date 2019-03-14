#ifndef __ENV_LIGHT_SPOT_H__
#define __ENV_LIGHT_SPOT_H__

#include "pragma/entities/components/base_entity_component.hpp"
#include <pragma/definitions.h>
#include "pragma/entities/baseentity.h"
#include "pragma/entities/baseentity_handle.h"
#include "pragma/entities/environment/lights/env_light.h"
#include "pragma/entities/components/base_entity_component.hpp"
#include <string>

namespace pragma
{
	class DLLNETWORK BaseEnvLightSpotComponent
		: public BaseEntityComponent
	{
	public:
		BaseEnvLightSpotComponent(BaseEntity &ent);
		virtual void Initialize() override;
		virtual void SetOuterCutoffAngle(float ang);
		virtual void SetInnerCutoffAngle(float ang);
		float GetOuterCutoffAngle() const;
		float GetInnerCutoffAngle() const;

		virtual void SetConeStartOffset(float offset);
		float GetConeStartOffset() const;
	protected:
		util::PFloatProperty m_angInnerCutoff = nullptr;
		util::PFloatProperty m_angOuterCutoff = nullptr;
		util::PFloatProperty m_coneStartOffset = nullptr;
		pragma::NetEventId m_netEvSetConeStartOffset = pragma::INVALID_NET_EVENT;
	};
};

#endif