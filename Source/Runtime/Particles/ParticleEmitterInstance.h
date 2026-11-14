#pragma once

#include "ParticleHelper.h"

class UParticleEmitter;
class UParticleLODLevel;

struct FParticleEmitterInstance
{
	// 기본 LOD 연결과 파티클 저장 공간 초기화
	bool Init(UParticleEmitter* InTemplate, int32 InMaxParticles, int32 InPayloadBytes = 0);
	// 시간 진행과 초당 생성량 적용
	void Tick(float DeltaTime, const FVector& InitialLocation = FVector::ZeroVector,
		const FVector& InitialVelocity = FVector::ZeroVector);
	// 남은 용량 안에서의 생성과 실제 생성 개수 반환
	int32 SpawnParticles(int32 Count, const FVector& InitialLocation = FVector::ZeroVector,
		const FVector& InitialVelocity = FVector::ZeroVector);
	// 수명 갱신과 만료된 파티클 제거
	void UpdateParticles(float DeltaTime);
	// 활성 배열 위치에 해당하는 파티클 제거
	bool KillParticle(int32 ActiveIndex);
	// 메모리와 생성 카운터를 유지하는 전체 비활성화
	void KillAllParticles() { ActiveParticles = 0; }

	// 활성 파티클의 현재 상태 접근
	FBaseParticle& GetParticle(int32 ActiveIndex)
	{
		return *reinterpret_cast<FBaseParticle*>(GetParticleAddress(ActiveIndex));
	}
	// 읽기 전용 활성 파티클 접근
	const FBaseParticle& GetParticle(int32 ActiveIndex) const
	{
		return *reinterpret_cast<const FBaseParticle*>(GetParticleAddress(ActiveIndex));
	}
	// 모듈 전용 바이트 영역의 시작 주소
	uint8* GetParticlePayload(int32 ActiveIndex)
	{
		return GetParticleAddress(ActiveIndex) + PayloadOffset;
	}

	// 삭제 책임이 없는 공유 템플릿 참조
	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleLODLevel* CurrentLODLevel = nullptr;
	int32 CurrentLODLevelIndex = 0;

	FParticleDataContainer DataContainer;
	int32 PayloadOffset = 0;                   // 슬롯 시작부터 payload까지의 거리
	int32 ParticleSize = 0;                    // payload까지 포함한 사용 영역
	int32 ParticleStride = 0;                  // 뒤 패딩을 포함한 슬롯 간격
	int32 ActiveParticles = 0;                 // 활성 인덱스 구간의 길이
	int32 MaxActiveParticles = 0;              // 확보한 전체 슬롯 개수
	uint32 ParticleCounter = 0;                // 성공한 생성의 누적 카운터
	float EmitterTime = 0.0f;

private:
	// 활성 배열 위치에서 실제 슬롯 주소로의 변환
	uint8* GetParticleAddress(int32 ActiveIndex) const;
	double SpawnFraction = 0.0;
};