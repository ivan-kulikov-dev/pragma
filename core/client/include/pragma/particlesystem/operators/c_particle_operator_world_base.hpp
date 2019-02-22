#ifndef __C_PARTICLE_OPERATOR_WORLD_BASE_HPP__
#define __C_PARTICLE_OPERATOR_WORLD_BASE_HPP__

#include "pragma/clientdefinitions.h"
#include "pragma/particlesystem/c_particlemodifier.h"

class DLLCLIENT CParticleOperatorWorldBase
	: public CParticleOperator
{
public:
	bool ShouldRotateWithEmitter() const;
protected:
	CParticleOperatorWorldBase(pragma::CParticleSystemComponent &pSystem,const std::unordered_map<std::string,std::string> &values);
private:
	bool m_bRotateWithEmitter = false;
};

#endif