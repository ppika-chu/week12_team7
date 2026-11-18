#include "EnginePCH.h"
#include "ParticleSystemActor.h"
// 임시 테스트용 임포트
#include "Asset/AssetManager.h"

AParticleSystemActor::AParticleSystemActor()
{
	ParticleComponent = CreateDefaultSubobject<UParticleSystemComponent>("UParticleSystemComponent");
	SetRootComponent(ParticleComponent);

	// 임시 테스트용 코드. 인자의 경로만 바꿔주는 것으로 테스트 particle을 바꿀 수 있음.
	if (UParticleSystem* PreviewSystem = UAssetManager::GetAssetByPath<UParticleSystem>("Particles/SubUVPreview.particle"))
	{
		ParticleComponent->SetTemplate(PreviewSystem);
	}
}