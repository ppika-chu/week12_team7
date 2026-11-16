#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Particles/ParticleModule.h"
#include <type_traits>

class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;

// 이미터의 거리별 버전 하나
// 모든 LOD 레벨은 같은 종류의 모듈을 같은 순서로 가져야 한다. 값은 달라도 되고 모듈을 끌 수는 있다.
// 그래야 LOD가 바뀌어도 파티클 메모리 배치(stride, payload 위치)가 변하지 않는다.
class UParticleLODLevel : public UObject
{
	DECLARE_CLASS(UParticleLODLevel, UObject)

	REFLECT_START(ClassName)
		PROPERTY(bEnabled)
	REFLECT_END()

public:
	UParticleLODLevel() = default;
	virtual ~UParticleLODLevel() = default;

	// Modules를 보고 SpawnModules, UpdateModules 목록과 TypeDataModule을 다시 만든다.
	// 모듈을 추가·삭제·재배치하거나 Spawn/Update 플래그가 바뀌면 호출해야 한다.
	void UpdateModuleLists();

	template<typename T>
	T* AddModule()
	{
		static_assert(std::is_base_of_v<UParticleModule, T> && !std::is_abstract_v<T>,
			"T must be derived from UParticleModule");
		T* Module = NewObject<T>(this);
		Module->InitializeDefaults();
		Modules.Add(Module);
		UpdateModuleLists();
		return Module;
	}
	const TArray<UParticleModule*>& GetModules() const { return Modules; }

public:
	int32 Level = 0;
	bool bEnabled = true;

	// 항상 있는 모듈은 목록과 따로 보관한다.
	UParticleModuleRequired* RequiredModule = nullptr;
	UParticleModuleSpawn* SpawnModule = nullptr;

	// 에디터에서 쌓는 일반 모듈 (Lifetime, Location, Velocity, Color, Size ...)
	TArray<UParticleModule*> Modules;

	// 그리는 방식을 바꾸는 모듈. 없으면 Sprite 이미터다.
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	// UpdateModuleLists가 Modules에서 추려 두는 목록.
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;

	// 이 LOD에서 동시에 살아 있던 최대 파티클 수. 초기 메모리 크기를 정할 때 참고한다.
	int32 PeakActiveParticles = 0;
};
