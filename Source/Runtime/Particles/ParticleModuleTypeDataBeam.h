#pragma once
#include "ParticleModuleTypeDataBase.h"
#include "Math/Vector.h"

class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataBeam, UParticleModuleTypeDataBase)

	REFLECT_START(UParticleModuleTypeDataBeam)
		PROPERTY(BeamEndOffset)
		PROPERTY(BeamWidth)
	REFLECT_END()

public:
	UParticleModuleTypeDataBeam() = default;
	virtual ~UParticleModuleTypeDataBeam() override = default;


	// 각 빔의 시작점 기준 오프셋. 방향과 길이는 컴포넌트 공간에서 정의한다.
	FVector BeamEndOffset = FVector(2.0f, 0.0f, 0.0f);
	float BeamWidth = 0.1f; // 월드 공간에서의 전체 폭

	virtual FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate) override;
};
