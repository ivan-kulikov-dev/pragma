#ifndef __S_NPCSTATE_H__
#define __S_NPCSTATE_H__

#include "pragma/serverdefinitions.h"

enum class NPCSTATE : int
{
	NONE,
	IDLE,
	ALERT,
	COMBAT,
	SCRIPT
};

#endif