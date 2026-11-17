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

	TArray<UParticleEmitter*> Emitters;
};

// Root와, Outer를 따라 Root에 닿는 객체(LOD, 모듈, 분포)를 모두 지운다.
// UE는 배열에서 빼기만 하고 GC가 지우지만, 이 엔진은 GC가 없어 직접 지운다. 실행 중인 인스턴스가 가리키지 않을 때 불러야 한다
void DestroyParticleObject(UObject* Root);
