#pragma once

#include "Particles/ParticleModule.h"
#include "Render/Material.h"

// 모든 이미터에 반드시 하나 있는 모듈
// 이미터 전체에 적용되는 설정(머티리얼, 정렬, 공간, 이미터 수명 등)을 담는다.
// LOD 레벨의 Modules 목록에 들어가지 않고 RequiredModule 포인터로 따로 보관한다.
class UParticleModuleRequired : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleRequired, UParticleModule)

	REFLECT_START(ClassName)
		PROPERTY(Material)
	REFLECT_END()

public:
	UParticleModuleRequired() = default;
	virtual ~UParticleModuleRequired() = default;

public:
	UMaterial* Material = nullptr;
};
