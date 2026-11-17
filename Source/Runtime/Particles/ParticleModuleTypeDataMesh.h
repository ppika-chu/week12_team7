#pragma once
#include "ParticleModuleTypeDataBase.h"

class UParticleEmitter;
class FParticleEmitterInstance;
class IParticleEmitterInstanceOwner;

class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataMesh, UParticleModuleTypeDataBase)

public:
	UParticleModuleTypeDataMesh() = default;
	virtual ~UParticleModuleTypeDataMesh() override = default;

	// FParticleMeshEmitterInstance를 생성한다.
	virtual FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate) override;
};