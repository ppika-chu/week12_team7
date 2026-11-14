#pragma once
#include <Core/Types.h>

class UParticleEmitter;
class UParticleSystemComponent;
class UParticleLODLevel;
class UParticleModule;
class UMaterial;
struct FParticleEventInstancePayload;
struct FBaseParticle;

// 런타임 이미터 데이터를 만들기 위해 모듈들에서 모은 정보
struct FParticleEmitterBuildInfo
{
};

// 이미터 하나의 실행 상태, 파티클 메모리 보유
struct FParticleEmitterInstance
{
	// 이 인스턴스가 따르는 설계도(이미터 에셋)
	UParticleEmitter* SpriteTemplate;

	// 이 인스턴스를 소유한 컴포넌트
	UParticleSystemComponent* Component;

	// 현재 쓰고 있는 LOD 단계의 번호
	int32 CurrentLODLevelIndex;
	// 현재 쓰고 있는 LOD 단계
	UParticleLODLevel* CurrentLODLevel;
	//
	//컴포넌트가 이 이미터에 대해서 TIck과 렌더링을 비활성화할 수 있다.
	bool bEnabled;
	// 대량의 파티클을 다루기 위해 직접 할당한 바이트 블록을 사용한다.
	// 파티클 데이터 블록. 파티클 하나가 ParticleStride 바이트씩 차지한다.
	uint8* ParticleData;
	// 파티클 인덱스 배열. 앞쪽 ActiveParticles개가 살아 있는 파티클의 슬롯 번호다.
	uint16* ParticleIndices;
	// 인스턴스 데이터 블록. 파티클이 아니라 인스턴스당 하나씩 필요한 모듈 데이터를 담는다.
	uint8* InstanceData;
	// 인스턴스 데이터 블록의 크기(바이트)
	int32 InstancePayloadSize;
	// 슬롯 안에서 추가 데이터(페이로드)가 시작하는 위치(바이트)
	int32 PayloadOffset;
	// 이전 위치
	FVector OldLocation;
	// 현재 위치
	FVector Location;
	// 파티클 하나의 전체 크기(바이트)
	int32 ParticleSize;
	// ParticleData 안에서 파티클 사이의 간격(바이트). i번 슬롯은 ParticleData + i * ParticleStride
	int32 ParticleStride;
	// 현재 살아 있는 파티클 수
	int32 ActiveParticles;
	// 지금까지 만든 파티클 수. 계속 늘어나기만 한다.
	uint32 ParticleCounter;
	// ParticleData에 담을 수 있는 최대 파티클 수(현재 할당된 슬롯 수)
	int32 MaxActiveParticles;
	// 스폰하고 남은 소수 부분. 프레임당 0.3개처럼 1개가 안 되는 양을 쌓아 두었다가 1을 넘으면 생성한다.
	float SpawnFraction;
	// 인스턴스가 만들어진 뒤 흐른 시간(초)
	float SecondsSinceCreation;
	// 현재 진행된 루프 횟수
	int32 LoopCount;
	// 이미터 진행 시간
	float EmitterDuration;
	// 현재 이미터 진행된 시간
	float EmitterTime;
	// 이미터 시작 시 딜레이 시간
	float CurrentDelay;
	// 현재 사용하고 있는 머테리얼
	UMaterial* CurrentMaterial;

	bool IsRenderDataDirty = false;

	FParticleEmitterInstance(UParticleSystemComponent& InComponent);
	~FParticleEmitterInstance();

	void InitParameters(UParticleEmitter* Emitter);
	void Init();
	bool Resize(int32 NewMax);

	UParticleLODLevel* GetCurrentLODLevelChecked();

	void Tick(float DeltaTime, bool bSupressSpawning);

private:
	int32 GetModuleDataOffset(UParticleModule* Module);
	float Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
	void KillParticles();
	void KillParticles(int32 Index);
	void ResetParticleParameters(float DeltaTime);
	void Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
	float Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bSuppressSpawning, bool bFirstTime);
	float Spawn(float DeltaTime);
	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, FParticleEventInstancePayload* EventPayload);
	void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);
	void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime);
	void UpdateBoundingBox(float DeltaTime);
};