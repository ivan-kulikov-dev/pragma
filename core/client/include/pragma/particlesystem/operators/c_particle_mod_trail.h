#ifndef __C_PARTICLE_MOD_TRAIL_H__
#define __C_PARTICLE_MOD_TRAIL_H__

#include "pragma/clientdefinitions.h"
#include "pragma/particlesystem/c_particlemodifier.h"

class DLLCLIENT CParticleOperatorTrail
	: public CParticleOperator
{
protected:
	float m_travelTime;
	std::vector<uint32_t> m_particleNodes;
public:
	CParticleOperatorTrail(pragma::CParticleSystemComponent &pSystem,const std::unordered_map<std::string,std::string> &values);
	void Simulate(CParticle &particle,double tDelta);
};

#endif