#pragma once
#include "ParticleModuleTypeDataBase.h"
#include "Render/Mesh.h"

class UParticleEmitter;
class FParticleEmitterInstance;
class IParticleEmitterInstanceOwner;

class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataMesh, UParticleModuleTypeDataBase)

	REFLECT_START(UParticleModuleTypeDataMesh)
		PROPERTY(Mesh)
		PROPERTY(bConsiderParticleSizeForCollision)
	REFLECT_END()

public:
	UParticleModuleTypeDataMesh() = default;
	virtual ~UParticleModuleTypeDataMesh() override = default;

	UStaticMesh* Mesh = nullptr;
	bool bConsiderParticleSizeForCollision = true; // 입자 크기 반영 여부

	// FParticleMeshEmitterInstance를 생성한다.
	virtual FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate) override;
};
