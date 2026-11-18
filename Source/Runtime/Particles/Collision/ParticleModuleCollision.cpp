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
#include "Particles/ParticleModuleTypeDataBeam.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
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

bool UParticleModuleCollision::ApplyCompletion(FBaseParticle& Particle) const
{
	Particle.Flags |= static_cast<int32>(STATE_Particle_IgnoreCollisions);

	switch (CompletionOption)
	{
	case EParticleCollisionComplete::Kill:
		return true;

	case EParticleCollisionComplete::FreezeTranslation:
		Particle.Flags |= static_cast<int32>(STATE_Particle_FreezeTranslation);
		Particle.Velocity = FVector::ZeroVector;
		Particle.BaseVelocity = FVector::ZeroVector;
		break;

	case EParticleCollisionComplete::FreezeMovement:
		Particle.Flags |= static_cast<int32>(
			STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation);
		Particle.Velocity = FVector::ZeroVector;
		Particle.BaseVelocity = FVector::ZeroVector;
		Particle.RotationRate = 0.0f;
		Particle.BaseRotationRate = 0.0f;
		break;

	case EParticleCollisionComplete::HaltCollisions:
		break;
	}

	return false;
}

void UParticleModuleCollision::FinalUpdate(const FUpdateContext& Context)
{
	FParticleEmitterInstance& Owner = Context.Owner;
	if (!CanRun(Owner, Context.Offset))
	{
		return;
	}

	const bool bUseLocalSpace = Owner.CurrentLODLevel->RequiredModule->bUseLocalSpace;
	const FMatrix ComponentToWorld = Owner.Component.GetComponentToWorld();
	FMatrix WorldToComponent = FMatrix::Identity;

	if (bUseLocalSpace)
	{
		auto IsFiniteMatrix = [](const FMatrix& Matrix)
		{
			for (const auto& Row : Matrix.M)
			{
				for (float Value : Row)
				{
					if (!std::isfinite(Value))
					{
						return false;
					}
				}
			}
			return true;
		};

		if (!IsFiniteMatrix(ComponentToWorld))
		{
			return;
		}

		const float Determinant = ComponentToWorld.Determinant();
		if (!std::isfinite(Determinant) || FMath::IsNearlyZero(Determinant))
		{
			return;
		}

		WorldToComponent = ComponentToWorld.Inverse();
		if (!IsFiniteMatrix(WorldToComponent))
		{
			return;
		}
	}

	// 월드 축별 스케일 크기
	const FVector WorldScale(
		ComponentToWorld.TransformVector(FVector(1.0f, 0.0f, 0.0f)).Size(),
		ComponentToWorld.TransformVector(FVector(0.0f, 1.0f, 0.0f)).Size(),
		ComponentToWorld.TransformVector(FVector(0.0f, 0.0f, 1.0f)).Size());

	const auto* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(Owner.CurrentLODLevel->TypeDataModule);
	FVector MeshHalfExtent = FVector::ZeroVector;
	if (MeshTypeData && MeshTypeData->Mesh)
	{
		const FBox& Bounds = MeshTypeData->Mesh->GetRenderBounds();
		MeshHalfExtent = (Bounds.Max - Bounds.Min) * 0.5f;
	}

	const bool bScaleMeshBounds = MeshTypeData && MeshTypeData->bConsiderParticleSizeForCollision;

	BEGIN_UPDATE_LOOP
	if ((static_cast<uint32>(Particle.Flags) &
			(STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation)) != 0)
	{
		CONTINUE_UPDATE_LOOP
	}

	PARTICLE_ELEMENT(FParticleCollisionPayload, CollisionPayload);
	if (CollisionPayload.bInitialized == 0)
	{
		InitializePayload(Owner, CollisionPayload);
	}

	const FVector WorldStart = bUseLocalSpace
		? ComponentToWorld.TransformPosition(Particle.OldLocation)
		: Particle.OldLocation;
	const FVector WorldEnd = bUseLocalSpace
		? ComponentToWorld.TransformPosition(Particle.Location)
		: Particle.Location;
	const FVector WorldMove = WorldEnd - WorldStart;
	const float MoveLength = WorldMove.Size();
	if (!std::isfinite(MoveLength) || MoveLength <= SMALL_NUMBER)
	{
		CONTINUE_UPDATE_LOOP
	}

	const FVector MoveDirection = WorldMove / MoveLength;
	const FVector WorldSize = Particle.Size.GetAbs() * WorldScale;
	const FVector EndExtension = MoveDirection * WorldSize / DirScalar;
	const FVector TraceEnd = WorldEnd + EndExtension;

	const FVector HalfExtent = bScaleMeshBounds ? MeshHalfExtent * WorldSize : MeshHalfExtent;
	const bool bUseBoxSweep =
		std::isfinite(HalfExtent.X) && HalfExtent.X > KINDA_SMALL_NUMBER &&
		std::isfinite(HalfExtent.Y) && HalfExtent.Y > KINDA_SMALL_NUMBER &&
		std::isfinite(HalfExtent.Z) && HalfExtent.Z > KINDA_SMALL_NUMBER;

	FHitResult Hit;
	const bool bHit = bUseBoxSweep
		? Owner.Component.ParticleBoxSweep(WorldStart, TraceEnd, HalfExtent, Hit)
		: Owner.Component.ParticleLineCheck(WorldStart, TraceEnd, Hit);
	if (!bHit)
	{
		CONTINUE_UPDATE_LOOP
	}

	// 원래 이동축 기준의 중심 이동거리
	const float CenterDistance = std::clamp(FVector::Dot(Hit.Location - WorldStart - EndExtension, MoveDirection),0.0f, MoveLength);
	FVector WorldResolvedLocation =	WorldStart + MoveDirection * CenterDistance + Hit.Normal * CollisionOffset;

	if (Hit.bStartPenetrating && Hit.PenetrationDepth > 0.0f)
	{
		WorldResolvedLocation += Hit.Normal * Hit.PenetrationDepth;
	}

	const FVector WorldIncomingVelocity = bUseLocalSpace 
		? ComponentToWorld.TransformVector(Particle.Velocity)
		: Particle.Velocity;

	bool bKill = false;
	if (CollisionPayload.RemainingCollisions > 0)
	{
		--CollisionPayload.RemainingCollisions;

		// 반사 후 잔여 변위
		const FVector RemainingMove = MoveDirection * (MoveLength - CenterDistance);
		WorldResolvedLocation += ComputeBounceVelocity(RemainingMove, Hit.Normal, CollisionPayload.DampingFactor);

		const FVector WorldBaseVelocity = bUseLocalSpace
			? ComponentToWorld.TransformVector(Particle.BaseVelocity)
			: Particle.BaseVelocity;
		const FVector BouncedVelocity =
			ComputeBounceVelocity(WorldIncomingVelocity, Hit.Normal, CollisionPayload.DampingFactor);
		const FVector BouncedBaseVelocity =
			ComputeBounceVelocity(WorldBaseVelocity, Hit.Normal, CollisionPayload.DampingFactor);

		Particle.Velocity = bUseLocalSpace
			? WorldToComponent.TransformVector(BouncedVelocity)
			: BouncedVelocity;
		Particle.BaseVelocity = bUseLocalSpace
			? WorldToComponent.TransformVector(BouncedBaseVelocity)
			: BouncedBaseVelocity;
	}
	else
	{
		bKill = ApplyCompletion(Particle);
	}

	Particle.Location = bUseLocalSpace
		? WorldToComponent.TransformPosition(WorldResolvedLocation)
		: WorldResolvedLocation;

	// 제거 전 충돌 결과 보관
	FParticleCollisionEvent Event;
	Event.EmitterUUID = Owner.SpriteTemplate->GetUUID();
	Event.ParticleId = static_cast<uint32>(Particle.Flags) & STATE_CounterMask;
	Event.HitActorUUID = Hit.HitActor ? Hit.HitActor->GetUUID() : 0;
	Event.HitComponentUUID = Hit.HitComponent ? Hit.HitComponent->GetUUID() : 0;
	Event.Location = Hit.ImpactPoint;
	Event.Normal = Hit.Normal;
	Event.IncomingVelocity = WorldIncomingVelocity;
	Event.Time = Hit.Time;
	Owner.Component.QueueParticleCollision(Event);

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

	const UParticleModuleCollision* ActiveModule = nullptr;
	for (UParticleModule* Module : LODLevel.GetModules())
	{
		const auto* CollisionModule = Cast<UParticleModuleCollision>(Module);
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
	if (!LODLevel.RequiredModule)
	{
		Error = "[ParticleModuleCollision] Missing Required module.";
		return false;
	}

	auto IsSupportedTypeData = [](UParticleModuleTypeDataBase* TypeData)
	{
		return !TypeData || TypeData->IsA<UParticleModuleTypeDataBeam>() ||
			TypeData->IsA<UParticleModuleTypeDataRibbon>() ||
			TypeData->IsA<UParticleModuleTypeDataMesh>();
	};

	if (!IsSupportedTypeData(LODLevel.TypeDataModule))
	{
		Error = "[ParticleModuleCollision] Unsupported TypeData.";
		return false;
	}
	for (UParticleModule* Module : LODLevel.GetModules())
	{
		if (!IsSupportedTypeData(Cast<UParticleModuleTypeDataBase>(Module)))
		{
			Error = "[ParticleModuleCollision] Unsupported TypeData.";
			return false;
		}
	}

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
	if (!std::isfinite(ActiveModule->DirScalar) || ActiveModule->DirScalar <= 0.0f)
	{
		Error = "[ParticleModuleCollision] Invalid DirScalar.";
		return false;
	}
	return true;
}