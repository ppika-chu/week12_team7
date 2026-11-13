#include "EnginePCH.h"
#include "ParticleLODLevel.h"

UParticleLODLevel::UParticleLODLevel()
	: Level(0)
	, bEnabled(true)
{
}

void UParticleLODLevel::UpdateModuleLists()
{
	/*
	TypeData는 Modules[] 배열에 없습니다라고 했는데 원본에서는 정확하지 않았습니다. 
	원본은 에디터에서 추가할 때 TypeData가 일단 Modules[]에 들어가고,
	UParticleLODLevel::UpdateModuleLists()가 그걸 찾아 TypeDataModule 포인터에 넣은 뒤 배열에서 빼냅니다
	(ParticleLODLevel.cpp:131-137, :172-174). 
	
	그래서 CacheEmitterModuleInfo()의 반복문에“TypeData면 건너뛴다”는 검사(ParticleEmitter.cpp:1023)가 방어용으로 남아 있습니다.
	*/
}
