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
		CLASS_TOOLTIP("Draw a static mesh instead of a sprite.")
		PROPERTY(Mesh) PROPERTY_TOOLTIP("Static mesh drawn for each particle. Uses the mesh materials.")
		PROPERTY(bConsiderParticleSizeForCollision)
	REFLECT_END()

public:
	UParticleModuleTypeDataMesh() = default;
	virtual ~UParticleModuleTypeDataMesh() override = default;

	UStaticMesh* Mesh = nullptr;

	// FParticleMeshEmitterInstance를 생성한다.
	virtual FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate) override;
};
