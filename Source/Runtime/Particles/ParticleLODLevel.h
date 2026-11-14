#pragma once

#include "UObject/Object.h"
#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleSpawn.h"
#include "Modules/ParticleModule.h"
#include "Containers/Array.h"
#include <type_traits>

#include "ParticleLODLevel.generated.h"

// 단일 LOD의 파티클 설정
UCLASS()
class UParticleLODLevel : public UObject
{
	GENERATED_BODY()

public:
	// 이미터 내부의 LOD 순번
	UPROPERTY(VisibleAnywhere)
	int32 Level = 0;

	// 해당 LOD의 사용 여부
	UPROPERTY(EditAnywhere)
	bool bEnabled = true;

	// LOD별 파티클 모듈
	UPROPERTY(VisibleAnywhere)
	UParticleModuleRequired* RequiredModule = nullptr;

	UPROPERTY(VisibleAnywhere)
	UParticleModuleSpawn* SpawnModule = nullptr;

	template<typename T>
	T* AddModule()
	{
		static_assert(std::is_base_of_v<UParticleModule, T> && !std::is_abstract_v<T>,
			"T must be derived from UParticleModule");

		T* NewModule = NewObject<T>(this);
		NewModule->InitializeDefaults();
		Modules.Add(NewModule);
		return NewModule;
	}
	const TArray<UParticleModule*>& GetModules() const
	{
		return Modules;
	}
private:
	TArray<UParticleModule*> Modules;
};