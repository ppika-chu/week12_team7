#pragma once

#include "Asset/RenderAsset.h"

class UParticleEmitter;

// 이펙트 하나의 설계
// 이미터 목록만 갖고, 실행 중 상태는 갖지 않는다. 같은 에셋을 여러 컴포넌트가 Template으로 공유한다.
class UParticleSystem : public URenderAsset
{
	DECLARE_CLASS(UParticleSystem, URenderAsset)

public:
	UParticleSystem() = default;
	virtual ~UParticleSystem() = default;

	// 모든 이미터의 모듈 목록과 메모리 배치를 다시 계산한다. 모듈 구성이 바뀐 뒤 호출한다.
	void UpdateAllModuleLists();

	// 모듈 목록은 그대로 두고 모든 이미터의 메모리 배치만 다시 계산한다.
	void BuildEmitters();

public:
	TArray<UParticleEmitter*> Emitters;
};
