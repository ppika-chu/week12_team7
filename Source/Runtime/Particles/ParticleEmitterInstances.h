#pragma once

#include "Math/Vector.h"
#include "Math/Box.h"
#include "Particles/ParticleEmitterInstanceOwner.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;
class UMaterial;
struct FBaseParticle;
struct FRandomStream;
struct FDynamicEmitterReplayDataBase;
struct FDynamicEmitterDataBase;

// 실행 중인 이미터 하나 (UE ParticleEmitterInstances.h)
// 파티클 메모리와 Spawn / Update / Kill을 맡는다. 설정은 SpriteTemplate(에셋)에서 읽기만 한다.
struct FParticleEmitterInstance
{
public:
	explicit FParticleEmitterInstance(IParticleEmitterInstanceOwner& InComponent);
	virtual ~FParticleEmitterInstance();

	FParticleEmitterInstance(const FParticleEmitterInstance&) = delete;
	FParticleEmitterInstance& operator=(const FParticleEmitterInstance&) = delete;

	virtual void InitParameters(UParticleEmitter* InTemplate);
	virtual void Init();
	virtual bool Resize(int32 NewMaxActiveParticles);
	virtual void Tick(float DeltaTime, bool bSuppressSpawning);

	void Rewind();
	void KillParticlesForced();
	void SetCurrentLODIndex(int32 InLODIndex);
	bool HasCompleted() const;
	bool HasFinishedSpawning() const;

	UParticleLODLevel* GetCurrentLODLevelChecked() const;
	uint32 GetModuleDataOffset(UParticleModule* Module) const;
	uint8* GetModuleInstanceData(UParticleModule* Module) const;
	FRandomStream& GetRandomStream() { return Component.GetRandomStream(); }

	// 렌더용 스냅샷을 새로 만들어 돌려준다. 그릴 파티클이 없으면 nullptr. 삭제는 받은 쪽이 한다.
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected) { return nullptr; }

protected:
	// 스냅샷에 종류와 상관없는 값과 파티클 메모리 복사본을 채운다.
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData);

	float Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
	double Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bSuppressSpawning, bool bFirstTime);
	void Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
	void Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
	void Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);

	virtual void KillParticles();
	void KillParticle(int32 Index);
	virtual void ResetParticleParameters(float DeltaTime);
	virtual void UpdateBoundingBox(float DeltaTime);

	virtual double Spawn(float DeltaTime);
	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity);
	virtual void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);
	virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime);

	void SetupEmitterDuration();
	int32 GetCurrentBurstCount();
	void ResetBurstList();
	void CheckEmitterFinished();

public:
	UParticleEmitter* SpriteTemplate = nullptr;
	IParticleEmitterInstanceOwner& Component;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;
	UMaterial* CurrentMaterial = nullptr;

	int32 TypeDataOffset = 0;
	int32 TypeDataInstanceOffset = -1;

	// 파티클 메모리
	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
	uint8* InstanceData = nullptr;
	int32 InstancePayloadSize = 0;
	int32 ParticleSize = 0;
	int32 ParticleStride = 0;
	int32 ActiveParticles = 0;
	uint32 ParticleCounter = 0;
	int32 MaxActiveParticles = 0;
	int32 ParticleLimit = 256;

	// 시간
	double SpawnFraction = 0.0;
	float SecondsSinceCreation = 0.0f;
	float EmitterTime = 0.0f;
	float LastDeltaTime = 0.0f;
	int32 LoopCount = 0;
	float EmitterDuration = 0.0f;
	TArray<float> EmitterDurations;
	float CurrentDelay = 0.0f;

	// 위치와 범위
	FVector Location;
	FVector OldLocation;
	FBox ParticleBoundingBox;

	// LOD별, Burst별 이번 루프에 터졌는지
	TArray<TArray<uint8>> BurstFired;

	bool bEnabled = true;
	bool bKillOnDeactivate = false;
	bool bKillOnCompleted = false;
	bool bHaltSpawning = false;
	bool bEmitterIsDone = false;
};

// TypeData가 없는 스프라이트 이미터의 인스턴스
struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
public:
	explicit FParticleSpriteEmitterInstance(IParticleEmitterInstanceOwner& InComponent)
		: FParticleEmitterInstance(InComponent)
	{
	}

	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;

protected:
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};

struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
	explicit FParticleMeshEmitterInstance(IParticleEmitterInstanceOwner& InComponent)
		: FParticleEmitterInstance(InComponent)
	{
	}

	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;

protected:
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};
