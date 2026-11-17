#pragma once

#include "Asset/RenderAsset.h"
#include "Containers/Array.h"

class UParticleEmitter;

class UFXSystemAsset : public URenderAsset
{
	DECLARE_CLASS(UFXSystemAsset, URenderAsset)
};

// 같은 에셋을 여러 컴포넌트가 Template으로 공유한다. 실행 상태는 이미터 인스턴스에만 둔다.
class UParticleSystem : public UFXSystemAsset
{
	DECLARE_CLASS(UParticleSystem, UFXSystemAsset)

public:
	static UParticleSystem* GetDefault();
	UParticleEmitter* AddEmitter();
	const TArray<UParticleEmitter*>& GetEmitters() const { return Emitters; }

	// 모듈 구성이 바뀐 뒤 목록과 메모리 배치를 다시 계산한다.
	void UpdateAllModuleLists();
	void UpdateAndBuild() { UpdateAllModuleLists(); }
	void BuildEmitters();

	// 배열 인덱스는 LOD 레벨이고, 값은 해당 레벨의 최소 거리다. 비어 있으면 LOD 0을 사용한다.
	TArray<float> LODDistances;
	float LODDistanceCheckTime = 0.25f;

	TArray<UParticleEmitter*> Emitters;
};
