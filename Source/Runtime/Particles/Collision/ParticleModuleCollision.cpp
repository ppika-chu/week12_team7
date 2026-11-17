#include "EnginePCH.h"
#include "Particles/Collision/ParticleModuleCollision.h"

#include "Collision/HitResult.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleTypeDataBase.h"

#include <algorithm>
#include <cmath>
#include <limits>

UParticleModuleCollision::UParticleModuleCollision()
{
	bSpawnModule = true;
	bFinalUpdateModule = true;

	// 기본 속도 유지 배율 분포
	UDistributionVectorConstant* DampingDistribution = CreateDefaultSubobject<UDistributionVectorConstant>("DistributionDampingFactor");
	DampingDistribution->SetConstant(FVector(0.5f, 0.5f, 0.5f));
	DampingFactor.SetDistribution(DampingDistribution);

	// 기본 허용 반사 횟수 분포
	UDistributionFloatConstant* CollisionCountDistribution = CreateDefaultSubobject<UDistributionFloatConstant>("DistributionMaxCollisions");
	CollisionCountDistribution->SetConstant(3.0f);
	MaxCollisions.SetDistribution(CollisionCountDistribution);
}

uint32 UParticleModuleCollision::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	return sizeof(FParticleCollisionPayload);
}

bool UParticleModuleCollision::CanRun(const FParticleEmitterInstance& Owner, int32 Offset) const
{
	// 실행 자료와 payload 접근 범위 검사
	if (!Owner.SpriteTemplate || !Owner.CurrentLODLevel ||
		Offset < static_cast<int32>(sizeof(FBaseParticle)) ||
		Offset % alignof(FParticleCollisionPayload) != 0 ||
		static_cast<int64>(Offset) + static_cast<int64>(sizeof(FParticleCollisionPayload)) > Owner.ParticleSize)
	{
		return false;
	}

	// 현재 LOD 설정의 지원 여부 검사
	FString ValidationError;
	return ValidateLOD(*Owner.CurrentLODLevel, ValidationError);
}

void UParticleModuleCollision::InitializePayload(FParticleEmitterInstance& Owner, FParticleCollisionPayload& Payload) const
{
	FRandomStream& RandomStream = Owner.GetRandomStream();

	// 입자별 속도 유지 배율 샘플링
	const FVector SampledDamping = DampingFactor.GetValue(Owner.EmitterTime, &RandomStream);
	Payload.DampingFactor = FVector(
		std::isfinite(SampledDamping.X) ? std::clamp(SampledDamping.X, 0.0f, 1.0f) : 0.5f,
		std::isfinite(SampledDamping.Y) ? std::clamp(SampledDamping.Y, 0.0f, 1.0f) : 0.5f,
		std::isfinite(SampledDamping.Z) ? std::clamp(SampledDamping.Z, 0.0f, 1.0f) : 0.5f);

	// 정수 변환 전 반사 횟수 제한
	const float SampledCount = MaxCollisions.GetValue(Owner.EmitterTime, &RandomStream);
	const double ClampedCount = std::isfinite(SampledCount)
		? std::clamp(static_cast<double>(SampledCount), 0.0, static_cast<double>(std::numeric_limits<int32>::max()))
		: 0.0;

	Payload.RemainingCollisions = static_cast<int32>(ClampedCount);
	Payload.bInitialized = 1;
}

void UParticleModuleCollision::Spawn(const FSpawnContext& Context)
{
	if (!CanRun(Context.Owner, Context.Offset))
	{
		return;
	}

	// 생성 중인 입자의 Collision payload 접근
	SPAWN_INIT
	PARTICLE_ELEMENT(FParticleCollisionPayload, CollisionPayload);

	InitializePayload(Context.Owner, CollisionPayload);
}

FVector UParticleModuleCollision::ComputeBounceVelocity(const FVector& Velocity, const FVector& Normal, const FVector& Damping) const
{
	// 표면 법선 방향 속도의 반전
	const float NormalSpeed = FVector::Dot(Velocity, Normal);
	const FVector ReflectedVelocity = Velocity - Normal * (2.0f * NormalSpeed);

	// 반사 후 축별 속도 감쇠
	FVector Result = ReflectedVelocity * Damping;

	// 축별 감쇠로 생긴 표면 안쪽 성분 제거
	const float InwardSpeed = FVector::Dot(Result, Normal);
	if (InwardSpeed < 0.0f)
	{
		Result -= Normal * InwardSpeed;
	}
	return Result;
}

void UParticleModuleCollision::FinalUpdate(const FUpdateContext& Context)
{
	if (!CanRun(Context.Owner, Context.Offset))
	{
		return;
	}

	const bool bUseLocalSpace = Context.Owner.CurrentLODLevel->RequiredModule->bUseLocalSpace;
	FMatrix ComponentToWorld = FMatrix::Identity;
	FMatrix WorldToComponent = FMatrix::Identity;

	if (bUseLocalSpace)
	{
		ComponentToWorld = Context.Owner.Component.GetComponentToWorld();

		// 로컬 변환 행렬의 유효성 검사
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				if (!std::isfinite(ComponentToWorld.M[Row][Column]))
				{
					return;
				}
			}
		}

		const float Determinant = ComponentToWorld.Determinant();
		if (!std::isfinite(Determinant) || FMath::IsNearlyZero(Determinant))
		{
			return;
		}

		WorldToComponent = ComponentToWorld.Inverse();

		// 역변환 결과의 유효성 검사
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				if (!std::isfinite(WorldToComponent.M[Row][Column]))
				{
					return;
				}
			}
		}
	}

	BEGIN_UPDATE_LOOP
	// 충돌 종료 또는 이동 정지 입자 제외
	if ((static_cast<uint32>(Particle.Flags) & (STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation)) != 0)
	{
		CONTINUE_UPDATE_LOOP
	}

	PARTICLE_ELEMENT(FParticleCollisionPayload, CollisionPayload);

	// 뒤늦게 활성화된 입자의 설정 초기화
	if (CollisionPayload.bInitialized == 0)
	{
		InitializePayload(Context.Owner, CollisionPayload);
	}

	// 현재 변환 기준의 월드 검사 구간
	const FVector WorldStart = bUseLocalSpace
		? ComponentToWorld.TransformPosition(Particle.OldLocation)
		: Particle.OldLocation;
	const FVector WorldEnd = bUseLocalSpace
		? ComponentToWorld.TransformPosition(Particle.Location)
		: Particle.Location;

	FHitResult Hit;
	if (!Context.Owner.Component.ParticleLineCheck(WorldStart, WorldEnd, Hit))
	{
		CONTINUE_UPDATE_LOOP
	}

	// 응답 전 월드 속도 보관
	const FVector WorldIncomingVelocity = bUseLocalSpace
		? ComponentToWorld.TransformVector(Particle.Velocity)
		: Particle.Velocity;
	const FVector WorldBaseVelocity = bUseLocalSpace
		? ComponentToWorld.TransformVector(Particle.BaseVelocity)
		: Particle.BaseVelocity;

	// 월드 접점 바깥쪽 위치 보정
	const FVector WorldLocation = Hit.Location + Hit.Normal * CollisionOffset;
	Particle.Location = bUseLocalSpace
		? WorldToComponent.TransformPosition(WorldLocation)
		: WorldLocation;

	bool bKill = false;
	if (CollisionPayload.RemainingCollisions > 0)
	{
		--CollisionPayload.RemainingCollisions;

		const FVector BouncedWorldVelocity = ComputeBounceVelocity(WorldIncomingVelocity, Hit.Normal, CollisionPayload.DampingFactor);
		const FVector BouncedWorldBaseVelocity = ComputeBounceVelocity(WorldBaseVelocity, Hit.Normal, CollisionPayload.DampingFactor);

		Particle.Velocity = bUseLocalSpace
			? WorldToComponent.TransformVector(BouncedWorldVelocity)
			: BouncedWorldVelocity;
		Particle.BaseVelocity = bUseLocalSpace
			? WorldToComponent.TransformVector(BouncedWorldBaseVelocity)
			: BouncedWorldBaseVelocity;
	}
	else
	{
		Particle.Flags |= static_cast<int32>(STATE_Particle_IgnoreCollisions);

		switch (CompletionOption)
		{
		case EParticleCollisionComplete::Kill:
			bKill = true;
			break;

		case EParticleCollisionComplete::FreezeTranslation:
			Particle.Flags |= static_cast<int32>(STATE_Particle_FreezeTranslation);
			Particle.Velocity = FVector::ZeroVector;
			Particle.BaseVelocity = FVector::ZeroVector;
			break;

		case EParticleCollisionComplete::FreezeMovement:
			Particle.Flags |= static_cast<int32>(STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation);
			Particle.Velocity = FVector::ZeroVector;
			Particle.BaseVelocity = FVector::ZeroVector;
			Particle.RotationRate = 0.0f;
			Particle.BaseRotationRate = 0.0f;
			break;

		case EParticleCollisionComplete::HaltCollisions:
			break;
		}
	}

	// 월드 충돌 결과의 값 복사
	FParticleCollisionEvent Event;
	Event.EmitterUUID = Context.Owner.SpriteTemplate->GetUUID();
	Event.ParticleId = static_cast<uint32>(Particle.Flags) & STATE_CounterMask;
	Event.HitActorUUID = Hit.HitActor ? Hit.HitActor->GetUUID() : 0;
	Event.HitComponentUUID = Hit.HitComponent ? Hit.HitComponent->GetUUID() : 0;
	Event.Location = Hit.ImpactPoint;
	Event.Normal = Hit.Normal;
	Event.IncomingVelocity = WorldIncomingVelocity;
	Event.Time = Hit.Time;
	Context.Owner.Component.QueueParticleCollision(Event);

	// 이벤트 보관 후 입자 제거
	if (bKill)
	{
		KILL_CURRENT_PARTICLE
		CONTINUE_UPDATE_LOOP
	}
	END_UPDATE_LOOP
}

bool UParticleModuleCollision::ValidateLOD(const UParticleLODLevel& LODLevel, FString& Error)
{
	Error.clear();

	// 활성 Collision 모듈의 중복 검사
	const UParticleModuleCollision* ActiveModule = nullptr;
	for (UParticleModule* Module : LODLevel.GetModules())
	{
		const UParticleModuleCollision* CollisionModule = Cast<UParticleModuleCollision>(Module);
		if (!CollisionModule || !CollisionModule->bEnabled)
		{
			continue;
		}
		if (ActiveModule)
		{
			Error = "[ParticleModuleCollision] Duplicate modules.";
			return false;
		}
		ActiveModule = CollisionModule;
	}

	if (!ActiveModule)
	{
		return true;
	}

	// 지원 실행 유형 검사
	if (!LODLevel.RequiredModule)
	{
		Error = "[ParticleModuleCollision] Missing Required module.";
		return false;
	}
	// TODO : 임시로 건 제한. 이미터 병합 후 수정할것.
	if (LODLevel.TypeDataModule)
	{
		Error = "[ParticleModuleCollision] 현재 Collision은 TypeData가 없는 Sprite만 지원합니다.";
		return false;
	}
	for (UParticleModule* Module : LODLevel.GetModules())
	{
		if (Module && Module->IsA<UParticleModuleTypeDataBase>())
		{
			Error = "[ParticleModuleCollision] 현재 Collision은 TypeData가 없는 Sprite만 지원합니다.";
			return false;
		}
	}

	// 공유 설정의 유효성 검사
	if (!ActiveModule->DampingFactor.IsCreated() || !ActiveModule->MaxCollisions.IsCreated())
	{
		Error = "[ParticleModuleCollision] Missing distribution.";
		return false;
	}

	const int32 CompletionValue = static_cast<int32>(ActiveModule->CompletionOption);
	if (CompletionValue < 0 || CompletionValue > static_cast<int32>(EParticleCollisionComplete::HaltCollisions))
	{
		Error = "[ParticleModuleCollision] Invalid completion option.";
		return false;
	}
	if (!std::isfinite(ActiveModule->CollisionOffset) || ActiveModule->CollisionOffset < 0.0f)
	{
		Error = "[ParticleModuleCollision] Invalid CollisionOffset.";
		return false;
	}
	return true;
}