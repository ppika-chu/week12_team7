#pragma once
#include "ParticleModuleTypeDataBase.h"
#include "Math/Vector.h"

class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataBeam, UParticleModuleTypeDataBase)

	REFLECT_START(UParticleModuleTypeDataBeam)
		CLASS_TOOLTIP("Draw each particle as a straight beam.")
		PROPERTY(BeamEndOffset) PROPERTY_TOOLTIP("Beam end = particle position + this (m). Follows component rotation.")
		PROPERTY(BeamWidth) PROPERTY_TOOLTIP("Beam width (m).")
	REFLECT_END()

public:
	UParticleModuleTypeDataBeam() = default;
	virtual ~UParticleModuleTypeDataBeam() override = default;


	// 각 빔의 시작점 기준 오프셋. 방향과 길이는 컴포넌트 공간에서 정의한다.
	FVector BeamEndOffset = FVector(2.0f, 0.0f, 0.0f);
	float BeamWidth = 0.1f; // 월드 공간에서의 전체 폭

	virtual FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate) override;

	// 빔 셰이더 머티리얼이 아니면 ParticleBeamMaterial로 바꾼다. 스프라이트 머티리얼로는 빔이 그려지지 않는다
	void SetToSensibleDefaults(UParticleEmitter* Owner) override;
	// Beam, Ribbon이 같이 쓴다: Owner의 모든 LOD Required 머티리얼을 빔 셰이더 머티리얼로
	static void UseBeamMaterial(UParticleEmitter* Owner);
};
