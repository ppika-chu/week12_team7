#include "EnginePCH.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleSystem.h"

void UParticleModule::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	// 모듈 → LOD 레벨 → 이미터 → 시스템 순으로 Outer를 올라간다
	for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		if (UParticleSystem* System = Cast<UParticleSystem>(Outer))
		{
			System->BuildEmitters();
			return;
		}
	}
}
