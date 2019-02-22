#include "stdafx_client.h"
#include "pragma/particlesystem/operators/c_particle_operator_pause_emission.hpp"
#include <mathutil/umath.h>
#include <pragma/math/vector/wvvector3.h>
#include <sharedutils/util_string.h>
#include <sharedutils/util.h>
#include <algorithm>

REGISTER_PARTICLE_OPERATOR(pause_emission,CParticleOperatorPauseEmission);
REGISTER_PARTICLE_OPERATOR(pause_child_emission,CParticleOperatorPauseChildEmission);

CParticleOperatorPauseEmissionBase::CParticleOperatorPauseEmissionBase(pragma::CParticleSystemComponent &pSystem,const std::unordered_map<std::string,std::string> &values)
	: CParticleOperator(pSystem,values)
{
	for(auto &pair : values)
	{
		auto key = pair.first;
		ustring::to_lower(key);
		if(key == "pause_start")
			m_fStart = util::to_float(pair.second);
		else if(key == "pause_end")
			m_fEnd = util::to_float(pair.second);
	}
	pSystem.SetAlwaysSimulate(true); // Required, otherwise Simulate() might not get called
}
void CParticleOperatorPauseEmissionBase::Initialize()
{
	CParticleOperator::Initialize();
	Simulate(0.0);
}
void CParticleOperatorPauseEmissionBase::Simulate(double tDelta)
{
	CParticleOperator::Simulate(tDelta);
	auto *ps = GetTargetParticleSystem();
	if(ps == nullptr)
		return;
	if(m_fEnd <= m_fStart)
		return;
	auto t = ps->GetLifeTime();
	switch(m_state)
	{
		case State::Initial:
			if(t < m_fStart)
				return;
			ps->PauseEmission();
			m_state = State::Paused;
			break;
		case State::Paused:
			if(t < m_fEnd)
				return;
			ps->ResumeEmission();
			m_state = State::Unpaused;
			break;
		case State::Unpaused:
			// Complete
			break;
	}
}

/////////////////////

CParticleOperatorPauseEmission::CParticleOperatorPauseEmission(pragma::CParticleSystemComponent &pSystem,const std::unordered_map<std::string,std::string> &values)
	: CParticleOperatorPauseEmissionBase(pSystem,values)
{}
pragma::CParticleSystemComponent *CParticleOperatorPauseEmission::GetTargetParticleSystem() {return &GetParticleSystem();}

/////////////////////

CParticleOperatorPauseChildEmission::CParticleOperatorPauseChildEmission(pragma::CParticleSystemComponent &pSystem,const std::unordered_map<std::string,std::string> &values)
	: CParticleOperatorPauseEmissionBase(pSystem,values)
{
	std::string childName;
	for(auto &pair : values)
	{
		auto key = pair.first;
		ustring::to_lower(key);
		if(key == "name")
			childName = pair.second;
	}
	auto &children = GetParticleSystem().GetChildren();
	auto it = std::find_if(children.begin(),children.end(),[&childName](const util::WeakHandle<pragma::CParticleSystemComponent> &hSystem) {
		return hSystem.valid() && ustring::match(childName,hSystem.get()->GetParticleSystemName());
	});
	if(it == children.end())
		return;
	m_hChildSystem = *it;
}
pragma::CParticleSystemComponent *CParticleOperatorPauseChildEmission::GetTargetParticleSystem() {return m_hChildSystem.get();}
