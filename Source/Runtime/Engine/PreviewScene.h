#pragma once

class UWorld;
class ADirectionalLightActor;
class ASkyLightActor;

// 에셋 에디터용 전용 월드와 기본 라이트 (UE FPreviewScene). 보여줄 대상은 파생 클래스가 GetWorld()에 스폰한다.
class FPreviewScene
{
public:
	FPreviewScene();
	virtual ~FPreviewScene();

	// PreviewWorld를 raw pointer로 소유하므로 복사 금지
	FPreviewScene(const FPreviewScene&) = delete;
	FPreviewScene& operator=(const FPreviewScene&) = delete;

	UWorld* GetWorld() const { return PreviewWorld; }
	ADirectionalLightActor* GetDirectionalLight() const { return DirectionalLight; }
	ASkyLightActor* GetSkyLight() const { return SkyLight; }

private:
	UWorld* PreviewWorld = nullptr;                     // 엔진 WorldList에 넣지 않는다
	ADirectionalLightActor* DirectionalLight = nullptr; // 소유는 PreviewWorld
	ASkyLightActor* SkyLight = nullptr;
};
