#include "EnginePCH.h"
#include "Particles/SubUV/ParticleModuleSubUV.h"

#include <algorithm>

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

UParticleModuleSubUV::UParticleModuleSubUV()
{
	bFinalUpdateModule = true;

	UDistributionFloatConstantCurve* Distribution = CreateDefaultSubobject<UDistributionFloatConstantCurve>("DistributionSubImageIndex");
	Distribution->AddPoint(0.0f, 0.0f);
	Distribution->AddPoint(1.0f, 1.0f);
	SubImageIndex.SetDistribution(Distribution);
}

void UParticleModuleSubUV::InitializeDefaults()
{
	const UParticleLODLevel* LODLevel = Cast<UParticleLODLevel>(GetOuter());
	if (!LODLevel || !LODLevel->RequiredModule)
	{
		return;
	}

	const UParticleModuleRequired* RequiredModule = LODLevel->RequiredModule;
	const int64 FrameCount = static_cast<int64>(std::max(RequiredModule->SubImages_Horizontal, 1)) * std::max(RequiredModule->SubImages_Vertical, 1);

	UDistributionFloatConstantCurve* Distribution = static_cast<UDistributionFloatConstantCurve*>(SubImageIndex.Distribution);
	Distribution->Reset();
	Distribution->AddPoint(0.0f, 0.0f);
	Distribution->AddPoint(1.0f, static_cast<float>(FrameCount));
}

void UParticleModuleSubUV::FinalUpdate(const FUpdateContext& Context)
{
	FRandomStream& RandomStream = Context.Owner.GetRandomStream();

	BEGIN_UPDATE_LOOP
	PARTICLE_ELEMENT(float, ImageIndex);
	ImageIndex = SubImageIndex.GetValue(Particle.RelativeTime, &RandomStream);
	END_UPDATE_LOOP
}

uint32 UParticleModuleSubUV::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	return sizeof(float);
}

bool UParticleModuleSubUV::ValidateLOD(const UParticleLODLevel& LODLevel, FString& Error)
{
	Error.clear();

	const UParticleModuleSubUV* ActiveModule = nullptr;
	for (UParticleModule* Module : LODLevel.GetModules())
	{
		const UParticleModuleSubUV* SubUVModule = Cast<UParticleModuleSubUV>(Module);
		if (!SubUVModule || !SubUVModule->bEnabled)
		{
			continue;
		}
		if (ActiveModule)
		{
			Error = "[ParticleModuleSubUV] 하나의 LOD에는 활성 SubUV 모듈을 하나만 둘 수 있습니다.";
			return false;
		}
		ActiveModule = SubUVModule;
	}

	if (!ActiveModule)
	{
		return true;
	}

	const UParticleModuleRequired* RequiredModule = LODLevel.RequiredModule;
	if (!RequiredModule || RequiredModule->SubImages_Horizontal <= 0 || RequiredModule->SubImages_Vertical <= 0)
	{
		Error = "[ParticleModuleSubUV] SubUV의 가로와 세로 칸 수는 양수여야 합니다.";
		return false;
	}

	const int64 FrameCount = static_cast<int64>(RequiredModule->SubImages_Horizontal) * RequiredModule->SubImages_Vertical;
	constexpr int64 MaxFrameCount = 1LL << 24;
	if (FrameCount > MaxFrameCount)
	{
		Error = "[ParticleModuleSubUV] SubUV 전체 칸 수는 16777216 이하여야 합니다.";
		return false;
	}

	if (!ActiveModule->SubImageIndex.IsCreated())
	{
		Error = "[ParticleModuleSubUV] SubUV 프레임 Distribution이 없습니다.";
		return false;
	}

	return true;
}