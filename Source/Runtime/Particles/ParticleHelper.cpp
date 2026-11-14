#include "EnginePCH.h"
#include "Core/HAL/UnrealMemory.h"
#include "Render/MaterialInterface.h"
#include "ParticleHelper.h"

//TODO : Malloc, Free 말고 더 빠르게 할당, 해제하는 방법이 있더라 ParticleEmitterInstances.cpp 196 ~ 290
FORCEINLINE static void* FastParticleSmallBlockAlloc(size_t AllocSize)
{
	return FMemory::Malloc(AllocSize);
}

FORCEINLINE static void FastParticleSmallBlockFree(void* RawMemory, size_t AllocSize)
{
	FMemory::Free(RawMemory);
}


void FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
{
	ParticleDataNumBytes = InParticleDataNumBytes;
	ParticleIndicesNumShorts = InParticleIndicesNumShorts;
	MemBlockSize = ParticleDataNumBytes + ParticleIndicesNumShorts * sizeof(uint16);

	ParticleData = (uint8*)FastParticleSmallBlockAlloc(MemBlockSize); // 줄임: FMemory::Malloc
	ParticleIndices = (uint16*)(ParticleData + ParticleDataNumBytes); // 블록 끝부분을 가리킬 뿐, 따로 할당 안 함
}

void FParticleDataContainer::Free()
{
	if (ParticleData)
	{
		assert(MemBlockSize > 0);
		FastParticleSmallBlockFree(ParticleData, MemBlockSize);
	}
	MemBlockSize = 0;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
	ParticleData = nullptr;
	ParticleIndices = nullptr;
}

FDynamicEmitterDataBase::FDynamicEmitterDataBase(const UParticleModuleRequired* RequiredModule)
	: bSelected(false)
	, bValid(true)
	, EmitterIndex(INDEX_NONE)
{
}

void FDynamicSpriteEmitterData::Init(bool bInSelected)
{
	bSelected = bInSelected;

	//bUsesDynamicParameter = GetSourceData()->DynamicParameterDataOffset > 0;

	UMaterialInterface const* MaterialInterface = const_cast<UMaterialInterface const*>(Source.MaterialInterface);

	//TODO : MaterialInterface->GetRenderProxy()?
	//MaterialResource = MaterialInterface->GetRenderProxy();

	// We won't need this on the render thread
	//Source.MaterialInterface = NULL;
}
