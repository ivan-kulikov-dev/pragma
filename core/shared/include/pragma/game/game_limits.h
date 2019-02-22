#ifndef __GAME_LIMITS_H__
#define __GAME_LIMITS_H__

#include "pragma/networkdefinitions.h"
#include <mathutil/umath.h>

enum class GameLimits : uint32_t
{
	MaxAbsoluteLights = 1'024,
	MaxAbsoluteShadowLights = 20,
	MaxCSMCascades = 4,
	MaxDirectionalLightSources = 4,

	MaxActiveShadowMaps = 20, // Spot lights
	MaxActiveShadowCubeMaps = 20, // Point lights

	MaxMeshVertices = 1'872'457,
	MaxWorldDistance = 1'048'576, // Maximum reasonable distance; Used for traces among other things
	MaxRayCastRange = 65'536,

	MaxBones = 256 // Maximum number of bones per entity; Has to be the same as the value used in shaders
};
REGISTER_BASIC_ARITHMETIC_OPERATORS(GameLimits);

#endif