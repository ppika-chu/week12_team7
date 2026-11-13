#include "EnginePCH.h"
#include "ParticleHelper.h"
#include "Core/HAL/UnrealMemory.h"
#include <limits>

// 소유 블록의 자동 해제
FParticleDataContainer::~FParticleDataContainer()
{
	Reset();
}

// 크기와 정렬을 검증한 단일 블록 할당
bool FParticleDataContainer::Allocate(int32 Capacity, int32 Stride)
{
	if (Capacity <= 0 || Capacity > MaxCapacity || Stride <= 0 || Stride % Alignment != 0)
	{
		return false;
	}

	// 오버플로우 방지
	const uint64 DataBytes = static_cast<uint64>(Capacity) * Stride;
	const uint64 BlockBytes = DataBytes + static_cast<uint64>(Capacity) * sizeof(uint16);
	if (BlockBytes > static_cast<uint64>(std::numeric_limits<int32>::max()))
	{
		return false;
	}

	uint8* NewData = static_cast<uint8*>(FMemory::Malloc(BlockBytes));
	if (!NewData)
	{
		return false;
	}

	Reset();
	MemBlockSize = static_cast<int32>(BlockBytes);
	ParticleDataNumBytes = static_cast<int32>(DataBytes);
	ParticleIndicesNumShorts = Capacity;
	ParticleData = NewData;
	ParticleIndices = reinterpret_cast<uint16*>(ParticleData + ParticleDataNumBytes);
	for (int32 Slot = 0; Slot < Capacity; ++Slot)
	{
		ParticleIndices[Slot] = static_cast<uint16>(Slot);
	}
	return true;
}

// 원본 할당 주소의 해제와 비소유 별칭 초기화
void FParticleDataContainer::Reset()
{
	FMemory::Free(ParticleData);
	ParticleData = nullptr;
	ParticleIndices = nullptr;
	MemBlockSize = ParticleDataNumBytes = ParticleIndicesNumShorts = 0;
}