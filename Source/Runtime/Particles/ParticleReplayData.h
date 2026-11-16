#pragma once

#include "Containers/Array.h"
#include "ParticleSpriteInstanceData.h"

class UMaterial;

// 이미터 한 개의 프레임 렌더 사본
struct FDynamicSpriteEmitterReplayData
{
	UMaterial* Material = nullptr;
	TArray<FParticleSpriteInstanceData> Particles;
};