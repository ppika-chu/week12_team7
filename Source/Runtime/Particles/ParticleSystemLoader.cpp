#include "EnginePCH.h"
#include "Asset/AssetManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModules.h"
#include "ParticleModuleRequired.h"
#include "ParticleModuleSpawn.h"
#include "ParticleModuleTypeDataBase.h"
#include "ParticleSystem.h"
#include "ParticleSystemLoader.h"

#include <filesystem>
#include <fstream>

namespace
{
// 엔진의 경로 문자열은 UTF-8이다.
std::filesystem::path ToFilePath(const FString& Path)
{
	return std::filesystem::path{ std::u8string(Path.begin(), Path.end()) };
}

// 모듈 하나를 { Class, Properties }로 저장한다. 없는 모듈은 null.
json SaveModule(UParticleModule* Module)
{
	if (!Module)
	{
		return json(nullptr);
	}

	json Item = json::object();
	Item["Class"] = Module->GetClass()->Name;

	json Properties = json::object();
	Module->Serialize(Properties, false);
	Item["Properties"] = std::move(Properties);
	return Item;
}

// 클래스 이름으로 모듈을 만들고 리플렉션 프로퍼티를 채운다. null이면 nullptr.
template <typename T>
T* LoadModule(const json& Item, UObject* Outer)
{
	if (Item.is_null())
	{
		return nullptr;
	}

	UClass* Class = FindClass(Item.at("Class").get<FString>());
	if (!Class || !Class->Constructor || !Class->IsChildOf(T::StaticClass()))
	{
		throw std::runtime_error("Unknown or invalid particle module class");
	}

	T* Module = static_cast<T*>(NewObject<UObject>(Outer, Class));
	if (Item.contains("Properties"))
	{
		json Properties = Item.at("Properties");
		Module->Serialize(Properties, true);
	}
	return Module;
}

// 로드에 실패한 조립 중간 결과를 정리한다.
void DestroySystem(UParticleSystem* System)
{
	if (!System)
	{
		return;
	}

	for (UParticleEmitter* Emitter : System->Emitters)
	{
		if (!Emitter)
		{
			continue;
		}
		for (UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			if (!LOD)
			{
				continue;
			}
			delete LOD->RequiredModule;
			delete LOD->SpawnModule;
			delete LOD->TypeDataModule;
			for (UParticleModule* Module : LOD->Modules)
			{
				delete Module;
			}
			delete LOD;
		}
		delete Emitter;
	}
	delete System;
}

void ReadSystem(UParticleSystem* System, const json& Root)
{
	for (const json& EmitterItem : Root.at("Emitters"))
	{
		UParticleEmitter* Emitter = NewObject<UParticleEmitter>(System);
		System->Emitters.Add(Emitter);

		for (const json& LODItem : EmitterItem.at("LODLevels"))
		{
			UParticleLODLevel* LOD = NewObject<UParticleLODLevel>(Emitter);
			Emitter->LODLevels.Add(LOD);

			LOD->Level = LODItem.value("Level", 0);
			LOD->bEnabled = LODItem.value("Enabled", true);

			if (LODItem.contains("Required"))
			{
				LOD->RequiredModule = LoadModule<UParticleModuleRequired>(LODItem.at("Required"), LOD);
			}
			if (LODItem.contains("Spawn"))
			{
				LOD->SpawnModule = LoadModule<UParticleModuleSpawn>(LODItem.at("Spawn"), LOD);
			}
			if (LODItem.contains("TypeData"))
			{
				LOD->TypeDataModule = LoadModule<UParticleModuleTypeDataBase>(LODItem.at("TypeData"), LOD);
			}
			if (LODItem.contains("Modules"))
			{
				for (const json& ModuleItem : LODItem.at("Modules"))
				{
					if (UParticleModule* Module = LoadModule<UParticleModule>(ModuleItem, LOD))
					{
						LOD->Modules.Add(Module);
					}
				}
			}

			// Required와 Spawn은 코어가 직접 읽으므로 파일에 없어도 반드시 채운다.
			if (!LOD->RequiredModule)
			{
				LOD->RequiredModule = NewObject<UParticleModuleRequired>(LOD);
			}
			if (!LOD->SpawnModule)
			{
				LOD->SpawnModule = NewObject<UParticleModuleSpawn>(LOD);
			}
		}
	}
}

json WriteSystem(UParticleSystem* System)
{
	json Root = json::object();
	Root["Emitters"] = json::array();

	for (UParticleEmitter* Emitter : System->Emitters)
	{
		json EmitterItem = json::object();
		EmitterItem["LODLevels"] = json::array();

		for (UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			json LODItem = json::object();
			LODItem["Level"] = LOD->Level;
			LODItem["Enabled"] = LOD->bEnabled;
			LODItem["Required"] = SaveModule(LOD->RequiredModule);
			LODItem["Spawn"] = SaveModule(LOD->SpawnModule);
			LODItem["TypeData"] = SaveModule(LOD->TypeDataModule);

			LODItem["Modules"] = json::array();
			for (UParticleModule* Module : LOD->Modules)
			{
				LODItem["Modules"].push_back(SaveModule(Module));
			}
			EmitterItem["LODLevels"].push_back(std::move(LODItem));
		}
		Root["Emitters"].push_back(std::move(EmitterItem));
	}
	return Root;
}
} // namespace

UParticleSystem* FParticleSystemLoader::LoadAsset(const FString& Key, const FString& Path, FString& Error)
{
	// 이미 등록된 키는 다시 읽지 않는다.
	if (UParticleSystem* Existing = UAssetManager::GetAssetByPath<UParticleSystem>(Key))
	{
		return Existing;
	}

	UParticleSystem* System = nullptr;
	try
	{
		std::ifstream File(ToFilePath(Path));
		if (!File.is_open())
		{
			Error = "Cannot open file: " + Path;
			return nullptr;
		}

		const json Root = json::parse(File);
		System = NewObject<UParticleSystem>();
		ReadSystem(System, Root);
	}
	catch (const std::exception& Exception)
	{
		DestroySystem(System);
		Error = Exception.what();
		return nullptr;
	}

	System->UpdateAndBuild();

	// 완성된 뒤에만 검색표에 올린다. 같은 키의 옛 에셋은 Retired 처리된다.
	System->SetPath(Key);
	TMap<FString, URenderAsset*> Replacements;
	Replacements.Add(Key, System);
	TMap<FString, URenderAsset*> Prepared;
	UAssetManager::Get().PrepareAssetRegistration(Replacements, Prepared);
	UAssetManager::Get().CommitAssetRegistration(Prepared);
	return System;
}

bool FParticleSystemLoader::SaveAsset(UParticleSystem* System, const FString& Path, FString& Error)
{
	if (!System)
	{
		Error = "Particle system is null";
		return false;
	}

	try
	{
		const json Root = WriteSystem(System);

		std::ofstream File(ToFilePath(Path), std::ios::trunc);
		if (!File.is_open())
		{
			Error = "Cannot open file: " + Path;
			return false;
		}
		File << Root.dump(1, '\t');
		if (!File.good())
		{
			Error = "Write failed: " + Path;
			return false;
		}
		return true;
	}
	catch (const std::exception& Exception)
	{
		Error = Exception.what();
		return false;
	}
}
