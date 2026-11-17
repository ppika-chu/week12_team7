#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"

struct FRandomStream;
struct FHitResult;
struct FParticleCollisionEvent;

// 이미터 인스턴스가 자기 주인(파티클 시스템 컴포넌트)에게 묻는 것들 (UE ParticleEmitterInstanceOwner.h)
// 인스턴스는 컴포넌트 클래스를 직접 알지 않고 이 인터페이스로만 접근한다.
class IParticleEmitterInstanceOwner
{
public:
	virtual ~IParticleEmitterInstanceOwner() = default;

	virtual FVector GetComponentLocation() const = 0;
	virtual FMatrix GetComponentToWorld() const = 0;
	virtual FRandomStream& GetRandomStream() = 0;
	// 컴포넌트에서 이미터 전체에 추가로 거는 지연 (초)
	virtual float GetEmitterDelay() const = 0;
	// 이번 프레임에 막 등록되었는지. true면 이전 위치를 현재 위치로 맞춘다.
	virtual bool GetJustRegistered() const = 0;
	virtual int32 GetMaxParticles() const { return 256; }

	// 월드 이동 구간중 충돌 여부 검사
	virtual bool ParticleLineCheck(const FVector& Start, const FVector& End, FHitResult& OutHit) const
	{
		return false;
	}

	// 발생한 충돌 이벤트 저장
	virtual void QueueParticleCollision(const FParticleCollisionEvent& Event)
	{
	}
};
