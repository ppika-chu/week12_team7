#pragma once
#include "Editor/AdvancedPreviewScene/AdvancedPreviewScene.h"

class UParticleSystem;
class UParticleSystemComponent;
class AActor;

// 파티클 에디터의 프리뷰 장면. 원점에 파티클 시스템 컴포넌트 하나를 둔다
// (UE Cascade는 FCascadeEdPreviewViewportClient가 FPreviewScene과 UCascadeParticleSystemComponent를 가진다)
class FParticleSystemPreviewScene : public FAdvancedPreviewScene
{
public:
	explicit FParticleSystemPreviewScene(UParticleSystem* InSystem);

	// 살아 있는 파티클을 지우고 처음부터 다시 재생한다 (Cascade Restart Sim)
	void RestartSimulation();

	// 프리뷰 월드 시간 배율로 멈춤과 재생 속도를 바꾼다 (Cascade Time 메뉴)
	void SetPaused(bool bInPaused);
	bool IsPaused() const { return bPaused; }
	void SetSimulationSpeed(float InSpeed);
	float GetSimulationSpeed() const { return SimulationSpeed; }
	// 실제로 월드에 들어간 시간 배율. 멈춤이면 0
	float GetTimeScale() const { return bPaused ? 0.0f : SimulationSpeed; }

	UParticleSystemComponent* GetParticleComponent() const { return ParticleComponent; }
	// 원점에서 바닥 귀퉁이까지 거리 (m). 카메라 Far 계산에 쓴다
	float GetSceneRadius() const;

private:
	// 멈춤이면 0, 아니면 재생 속도를 월드 시간 배율로 넣는다
	void ApplyTimeScale();

	UParticleSystem* System = nullptr; // 소유는 UAssetManager
	bool bPaused = false;
	float SimulationSpeed = 1.0f;

	// 원점에 둔 빈 액터 + 파티클 시스템 컴포넌트. 소유는 PreviewWorld
	AActor* PreviewActor = nullptr;
	UParticleSystemComponent* ParticleComponent = nullptr;
};
