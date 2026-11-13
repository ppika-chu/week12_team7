#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

class UParticleLODLevel;
class UParticleModule;
class IParticleEmitterInstanceOwner;
struct FParticleEmitterInstance;

// 효과 하나의 설계도
// LOD 레벨 목록을 갖고, 모듈 구성으로부터 파티클 한 개의 메모리 배치를 계산해 둔다.
class UParticleEmitter : public UObject
{
	DECLARE_CLASS(UParticleEmitter, UObject)

	REFLECT_START(ClassName)
		PROPERTY(EmitterName)
	REFLECT_END()

public:
	UParticleEmitter() = default;
	virtual ~UParticleEmitter() = default;

	UParticleLODLevel* GetLODLevel(int32 LODIndex) const;

	// 모든 LOD 레벨의 모듈 목록을 다시 만들고 Build를 호출한다.
	// 모듈 구성이 바뀌면 이 함수 하나만 부르면 된다.
	void UpdateModuleLists();

	// TypeData에 모듈 정보를 넘기고 메모리 배치를 다시 계산한다.
	virtual void Build();

	// 파티클 한 개의 크기(ParticleSize)와 모듈별 payload 위치, 인스턴스 데이터 위치를 계산한다.
	// 최상위 LOD(0)의 모듈 순서를 기준으로 하며, 다른 LOD도 같은 순서라고 가정한다.
	void CacheEmitterModuleInfo();

	// 이 이미터를 실행할 인스턴스를 만들어 초기화한다. 삭제는 호출한 쪽(컴포넌트)이 한다.
	virtual FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent) { return nullptr; }

public:
	FString EmitterName = "Particle Emitter";

	TArray<UParticleLODLevel*> LODLevels;

	// ---- CacheEmitterModuleInfo가 채우는 값 ----

	// FBaseParticle + 모든 payload의 바이트 수. 16바이트 정렬은 인스턴스가 메모리를 잡을 때 한다.
	int32 ParticleSize = 0;
	// 이미터 인스턴스 하나에 필요한 인스턴스 데이터 바이트 수
	int32 ReqInstanceBytes = 0;

	// TypeData 모듈의 payload / 인스턴스 데이터 위치
	int32 TypeDataOffset = 0;
	int32 TypeDataInstanceOffset = -1;

	// 모듈 -> payload 시작 위치 (파티클 칸 기준 바이트). 최상위 LOD의 모듈을 키로 쓴다.
	TMap<UParticleModule*, uint32> ModuleOffsetMap;
	// 모듈 -> 인스턴스 데이터 시작 위치. 모든 LOD의 같은 자리 모듈이 같은 값을 갖는다.
	TMap<UParticleModule*, uint32> ModuleInstanceOffsetMap;
	// 인스턴스 데이터를 쓰는 모듈 목록 (인스턴스 생성 시 PrepPerInstanceBlock 호출 대상)
	TArray<UParticleModule*> ModulesNeedingInstanceData;

	// 처음 메모리를 잡을 때 쓸 파티클 수. 0이면 인스턴스가 추정한다.
	int32 InitialAllocationCount = 0;
	// 모든 LOD 중 동시에 살아 있던 최대 파티클 수
	int32 PeakActiveParticles = 0;
};

// TypeData가 없는 기본 이미터. 파티클을 카메라를 향한 사각형으로 그린다 (UE ParticleSpriteEmitter.h).
class UParticleSpriteEmitter : public UParticleEmitter
{
	DECLARE_CLASS(UParticleSpriteEmitter, UParticleEmitter)

public:
	UParticleSpriteEmitter() = default;
	virtual ~UParticleSpriteEmitter() = default;

	virtual FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent) override;
};
