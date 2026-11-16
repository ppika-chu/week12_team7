#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"

class UParticleSystem;

// .particle 파일과 UParticleSystem 사이의 변환, 그리고 에셋 매니저 등록을 담당한다.
class FParticleSystemLoader
{
public:
	static UParticleSystem* LoadAsset(const FString& Key, const FString& Path, FString& Error);
	static bool SaveAsset(UParticleSystem* System, const FString& Path, FString& Error);
};
