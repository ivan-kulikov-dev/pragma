#ifndef __SURFACEMATERIAL_H__
#define __SURFACEMATERIAL_H__

#include "pragma/networkdefinitions.h"
#include "pragma/physics/phys_liquid.hpp"
#include <string>
#include <unordered_map>
#include <BulletCollision/CollisionShapes/btMaterial.h>

#ifdef PHYS_ENGINE_PHYSX
namespace physx
{
	class PxPhysics;
	class PxMaterial;
};
#endif

class SurfaceMaterial;
class DLLNETWORK SurfaceMaterialManager
{
protected:
	std::vector<SurfaceMaterial> m_materials; // These have to be objects (Not pointers) to uphold the requirements for the btTriangleIndexVertexMaterialArray constructor.
public:
	SurfaceMaterialManager();
	void Load(const std::string &path);
	SurfaceMaterial &Create(const std::string &identifier,Float friction=0.5f,Float restitution=0.5f);
	// The returned pointer is NOT guaranteed to stay alive; Don't store it.
	SurfaceMaterial *GetMaterial(const std::string &id);
	std::vector<SurfaceMaterial> &GetMaterials();
};


namespace pragma
{
	namespace nav
	{
		enum class PolyFlags : uint16_t;
	};
};

struct PhysLiquid;
class DLLNETWORK SurfaceMaterial
#ifdef PHYS_ENGINE_BULLET
	: public btMaterial
#endif
{
public:
	struct AudioInfo
	{
		 // These should correspond to the values specified in "c_game_audio.cpp"
		float lowFreqAbsorption = 0.10f;
		float midFreqAbsorption = 0.20f;
		float highFreqAbsorption = 0.30f;
		float scattering = 0.05f;
		float lowFreqTransmission = 0.100f;
		float midFreqTransmission = 0.050f;
		float highFreqTransmission = 0.030f;
	};
protected:
	UInt m_index;
	std::string m_identifier;
	std::string m_footstepType;
	std::string m_softImpactSound;
	std::string m_hardImpactSound;
	std::string m_bulletImpactSound;
	std::string m_impactParticle;
	pragma::nav::PolyFlags m_navigationFlags;
	std::unique_ptr<PhysLiquid> m_liquidInfo = nullptr;
	AudioInfo m_audioInfo = {};
	PhysLiquid &InitializeLiquid();
public:
#ifdef PHYS_ENGINE_PHYSX
	physx::PxMaterial *m_material;
	SurfaceMaterial(UInt idx);
	SurfaceMaterial(UInt idx,float staticFriction,float dynamicFriction,float restitution,std::string footstepType);
#else
	SurfaceMaterial(const std::string &identifier,UInt idx,Float friction=0.5f,Float restitution=0.5f);
#endif
	SurfaceMaterial(const SurfaceMaterial &other);
	SurfaceMaterial &operator=(const SurfaceMaterial &other);
	void Reset();
#ifdef PHYS_ENGINE_PHYSX
	physx::PxMaterial *GetMaterial();
	float GetDynamicFriction();
	float GetStaticFriction();
	void SetDynamicFriction(float friction);
	void SetStaticFriction(float friction);
	void SetFrictionEnabled(bool b);
	void SetStrongFrictionEnabled(bool b);
#else
	const std::string &GetIdentifier() const;
	UInt GetIndex() const;
	Float GetFriction() const;
	void SetFriction(Float friction);
	Float GetRestitution() const;
#endif
	void SetRestitution(Float restitution);
	const std::string &GetFootstepType() const;
	void SetFootstepType(const std::string &footstep);
	void SetSoftImpactSound(const std::string &snd);
	const std::string &GetSoftImpactSound() const;
	void SetHardImpactSound(const std::string &snd);
	const std::string &GetHardImpactSound() const;
	void SetBulletImpactSound(const std::string &snd);
	const std::string &GetBulletImpactSound() const;
	void SetImpactParticleEffect(const std::string &particle);
	const std::string &GetImpactParticleEffect() const;

	void SetNavigationFlags(pragma::nav::PolyFlags flags);
	pragma::nav::PolyFlags GetNavigationFlags() const;

	void SetDensity(float density);
	float GetDensity() const;
	void SetLinearDragCoefficient(float coefficient);
	float GetLinearDragCoefficient() const;
	void SetTorqueDragCoefficient(float coefficient);
	float GetTorqueDragCoefficient() const;
	void SetWaveStiffness(float stiffness);
	float GetWaveStiffness() const;
	void SetWavePropagation(float propagation);
	float GetWavePropagation() const;

	const AudioInfo &GetAudioInfo() const;
	void SetAudioInfo(const AudioInfo &info);
	void SetAudioLowFrequencyAbsorption(float absp);
	float GetAudioLowFrequencyAbsorption() const;
	void SetAudioMidFrequencyAbsorption(float absp);
	float GetAudioMidFrequencyAbsorption() const;
	void SetAudioHighFrequencyAbsorption(float absp);
	float GetAudioHighFrequencyAbsorption() const;
	void SetAudioScattering(float scattering);
	float GetAudioScattering() const;
	void SetAudioLowFrequencyTransmission(float transmission);
	float GetAudioLowFrequencyTransmission() const;
	void SetAudioMidFrequencyTransmission(float transmission);
	float GetAudioMidFrequencyTransmission() const;
	void SetAudioHighFrequencyTransmission(float transmission);
	float GetAudioHighFrequencyTransmission() const;
};

DLLNETWORK std::ostream &operator<<(std::ostream &out,const SurfaceMaterial &surfaceMaterial);

#endif