#include "EnginePCH.h"
#include "Engine/PreviewScene.h"

#include "Engine/World.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "GameFramework/Actor/SkyLightActor.h"

FPreviewScene::FPreviewScene()
{
	// 프리뷰 전용 타입. 컴포넌트 Tick만 돈다
	PreviewWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);

	// -X 방향으로 40도 내려 비춘다
	const FTransform LightTransform(FRotator(40.0f, 180.0f, 0.0f), FVector(0.0f, 0.0f, 3.0f));
	DirectionalLight = PreviewWorld->SpawnActor<ADirectionalLightActor>(NAME_None, &LightTransform);
	// 가까운 쪽 그림자 해상도를 높인다. 이름과 달리 Directional 컴포넌트를 돌려준다
	DirectionalLight->GetPointLightComponent()->SetCSMEnabled(true);

	// 그늘진 면도 보이게 하는 환경광. 기본 Intensity 1은 그늘이 거의 없어 낮춘다
	const FTransform SkyLightTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 3.0f));
	SkyLight = PreviewWorld->SpawnActor<ASkyLightActor>(NAME_None, &SkyLightTransform);
	SkyLight->GetSkyLightComponent()->SetIntensity(0.3f);
}

FPreviewScene::~FPreviewScene()
{
	// PIE 종료와 같은 순서. 액터는 월드가 지운다
	// TODO: GPU가 이 월드의 자원을 쓰는 중에 지우지 않는지 확인
	if (PreviewWorld)
	{
		PreviewWorld->ClearWorld();
		delete PreviewWorld;
		PreviewWorld = nullptr;
		DirectionalLight = nullptr;
		SkyLight = nullptr;
	}
}

void FPreviewScene::Tick(float DeltaTime)
{
	PreviewWorld->Tick(DeltaTime);
}
