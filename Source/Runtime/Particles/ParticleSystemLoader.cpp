#include "EnginePCH.h"
#include "Particles/ParticleSystemLoader.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataBase.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/SubUV/ParticleModuleSubUV.h"
#include "Particles/Collision/ParticleModuleCollision.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Math/InterpCurve.h"
#include "Render/Material.h"
#include "Render/ImageLoader.h"
#include "Render/RenderCommand.h"
#include "Render/Texture2D.h"
#include "Serialization/TypeSerializer.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace
{
std::filesystem::path ToFilePath(const FString& Path)
{
	return std::filesystem::path{ std::u8string(Path.begin(), Path.end()) };
}

// 리플렉션의 에셋 경로와 달리 분포는 모듈이 소유한 객체이므로 값을 중첩 저장한다.
template<typename T>
json WriteCurveValue(const T& Value)
{
	if constexpr (std::is_same_v<T, FVector2>)
	{
		return json{ { "X", Value.X }, { "Y", Value.Y } };
	}
	else
	{
		return json(Value);
	}
}

template<typename T>
T ReadCurveValue(const json& Value)
{
	if constexpr (std::is_same_v<T, FVector2>)
	{
		return FVector2(Value.at("X").get<float>(), Value.at("Y").get<float>());
	}
	else
	{
		return Value.get<T>();
	}
}

template<typename T>
json WriteCurve(const FInterpCurve<T>& Curve)
{
	json Points = json::array();
	for (const FInterpCurvePoint<T>& Point : Curve.Points)
	{
		Points.push_back({ { "InVal", Point.InVal }, { "OutVal", WriteCurveValue(Point.OutVal) },
			{ "ArriveTangent", WriteCurveValue(Point.ArriveTangent) },
			{ "LeaveTangent", WriteCurveValue(Point.LeaveTangent) },
			{ "InterpMode", static_cast<uint8>(Point.InterpMode) } });
	}
	return Points;
}

template<typename T>
void ReadCurve(FInterpCurve<T>& Curve, const json& Points)
{
	Curve.Reset();
	for (const json& Item : Points)
	{
		const int32 Index = Curve.AddPoint(Item.at("InVal").get<float>(), ReadCurveValue<T>(Item.at("OutVal")),
			static_cast<EInterpCurveMode>(Item.value("InterpMode", 0)));
		if (Index == INDEX_NONE)
		{
			throw std::runtime_error("Invalid particle curve key");
		}
		FInterpCurvePoint<T>& Point = Curve.Points[Index];
		if (Item.contains("ArriveTangent")) Point.ArriveTangent = ReadCurveValue<T>(Item.at("ArriveTangent"));
		if (Item.contains("LeaveTangent")) Point.LeaveTangent = ReadCurveValue<T>(Item.at("LeaveTangent"));
	}
}

json SaveDistribution(UDistribution* Distribution)
{
	if (!Distribution) return nullptr;
	json Item = { { "Class", Distribution->GetClass()->Name } };
	json Properties = json::object();
	Distribution->Serialize(Properties, false);
	Item["Properties"] = std::move(Properties);
	if (auto* Curve = Cast<UDistributionFloatConstantCurve>(Distribution)) Item["Curve"] = WriteCurve(Curve->ConstantCurve);
	else if (auto* Curve = Cast<UDistributionFloatUniformCurve>(Distribution)) Item["Curve"] = WriteCurve(Curve->ConstantCurve);
	else if (auto* Curve = Cast<UDistributionVectorConstantCurve>(Distribution)) Item["Curve"] = WriteCurve(Curve->ConstantCurve);
	else if (auto* Curve = Cast<UDistributionVectorUniformCurve>(Distribution))
	{
		Item["MinCurve"] = WriteCurve(Curve->MinCurve);
		Item["MaxCurve"] = WriteCurve(Curve->MaxCurve);
	}
	return Item;
}

template<typename T>
T* LoadDistribution(const json& Item, UObject* Outer)
{
	if (Item.is_null()) return nullptr;
	UClass* Class = FindClass(Item.at("Class").get<FString>());
	if (!Class || !Class->Constructor || !Class->IsChildOf(T::StaticClass()))
	{
		throw std::runtime_error("Unknown or invalid particle distribution class");
	}
	T* Distribution = static_cast<T*>(NewObject<UObject>(Outer, Class));
	if (Item.contains("Properties"))
	{
		json Properties = Item.at("Properties");
		Distribution->Serialize(Properties, true);
	}
	if (auto* Curve = Cast<UDistributionFloatConstantCurve>(Distribution)) ReadCurve(Curve->ConstantCurve, Item.at("Curve"));
	else if (auto* Curve = Cast<UDistributionFloatUniformCurve>(Distribution)) ReadCurve(Curve->ConstantCurve, Item.at("Curve"));
	else if (auto* Curve = Cast<UDistributionVectorConstantCurve>(Distribution)) ReadCurve(Curve->ConstantCurve, Item.at("Curve"));
	else if (auto* Curve = Cast<UDistributionVectorUniformCurve>(Distribution))
	{
		ReadCurve(Curve->MinCurve, Item.at("MinCurve"));
		ReadCurve(Curve->MaxCurve, Item.at("MaxCurve"));
	}
	return Distribution;
}

template<typename F>
void VisitDistributions(UParticleModule* Module, F&& Visit)
{
	if (auto* Spawn = Cast<UParticleModuleSpawn>(Module))
	{
		Visit("Rate", Spawn->Rate); Visit("RateScale", Spawn->RateScale); Visit("BurstScale", Spawn->BurstScale);
	}
	if (auto* Lifetime = Cast<UParticleModuleLifetime>(Module)) Visit("Lifetime", Lifetime->Lifetime);
	if (auto* Location = Cast<UParticleModuleLocation>(Module)) Visit("StartLocation", Location->StartLocation);
	if (auto* Velocity = Cast<UParticleModuleVelocity>(Module))
	{
		Visit("StartVelocity", Velocity->StartVelocity); Visit("StartVelocityRadial", Velocity->StartVelocityRadial);
	}
	if (auto* Size = Cast<UParticleModuleSize>(Module)) Visit("StartSize", Size->StartSize);
	if (auto* Size = Cast<UParticleModuleSizeMultiplyLife>(Module)) Visit("LifeMultiplier", Size->LifeMultiplier);
	if (auto* Color = Cast<UParticleModuleColor>(Module))
	{
		Visit("StartColor", Color->StartColor); Visit("StartAlpha", Color->StartAlpha);
	}
	if (auto* Color = Cast<UParticleModuleColorOverLife>(Module))
	{
		Visit("ColorOverLife", Color->ColorOverLife); Visit("AlphaOverLife", Color->AlphaOverLife);
	}
	if (auto* SubUV = Cast<UParticleModuleSubUV>(Module))
	{
		Visit("SubImageIndex", SubUV->SubImageIndex);
	}
	if (auto* Collision = Cast<UParticleModuleCollision>(Module))
	{
		Visit("DampingFactor", Collision->DampingFactor);
		Visit("MaxCollisions", Collision->MaxCollisions);
	}
}

json SaveModule(UParticleModule* Module)
{
	if (!Module) return nullptr;
	json Item = { { "Class", Module->GetClass()->Name } };
	json Properties = json::object();
	Module->Serialize(Properties, false);
	Item["Properties"] = std::move(Properties);
	json Distributions = json::object();
	VisitDistributions(Module, [&Distributions](const char* Name, const auto& Raw)
	{
		Distributions[Name] = SaveDistribution(Raw.Distribution);
	});
	if (!Distributions.empty()) Item["Distributions"] = std::move(Distributions);
	// 머티리얼은 다른 컴포넌트와 같이 UMaterial::SaveMaterial로 쓴다. Details에서 텍스처를 바꾼 복사본(경로 없음)도 원본 + 값으로 남는다
	if (auto* Required = Cast<UParticleModuleRequired>(Module))
	{
		Item["Material"] = Required->Material ? UMaterial::SaveMaterial(Required->Material) : json(nullptr);
	}
	if (auto* Spawn = Cast<UParticleModuleSpawn>(Module))
	{
		Item["Bursts"] = json::array();
		for (const FParticleBurst& Burst : Spawn->BurstList)
		{
			Item["Bursts"].push_back({ { "Count", Burst.Count }, { "CountLow", Burst.CountLow }, { "Time", Burst.Time } });
		}
	}
	return Item;
}

template<typename T>
T* LoadModule(const json& Item, UObject* Outer)
{
	if (Item.is_null()) return nullptr;
	UParticleModuleCollision::StaticClass();
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
		// Cascade 초기 파일의 단순 수명·스폰 값을 CY 분포로 옮긴다.
		if (auto* Lifetime = Cast<UParticleModuleLifetime>(Module); Lifetime && Properties.contains("LifetimeMin"))
		{
			auto* Range = Cast<UDistributionFloatUniform>(Lifetime->Lifetime.Distribution);
			Range->Min = Properties.at("LifetimeMin").get<float>();
			Range->Max = Properties.value("LifetimeMax", Range->Min);
		}
		if (auto* Spawn = Cast<UParticleModuleSpawn>(Module))
		{
			if (Properties.contains("Rate")) Cast<UDistributionFloatConstant>(Spawn->Rate.Distribution)->SetConstant(Properties.at("Rate").get<float>());
			if (Properties.contains("RateScale")) Cast<UDistributionFloatConstant>(Spawn->RateScale.Distribution)->SetConstant(Properties.at("RateScale").get<float>());
			if (Properties.contains("BurstScale")) Cast<UDistributionFloatConstant>(Spawn->BurstScale.Distribution)->SetConstant(Properties.at("BurstScale").get<float>());
		}
	}
	if (Item.contains("Distributions"))
	{
		const json& Distributions = Item.at("Distributions");
		VisitDistributions(Module, [&Distributions, Module](const char* Name, auto& Raw)
		{
			if (Distributions.contains(Name))
			{
				using DistributionType = std::remove_pointer_t<decltype(Raw.Distribution)>;
				auto* Replacement = LoadDistribution<DistributionType>(Distributions.at(Name), Module);
				delete Raw.Distribution;
				Raw.Distribution = Replacement;
			}
		});
	}
	// 예전 파일은 이 항목 없이 Properties의 경로만 있다. 그때는 위 Serialize가 읽은 값을 그대로 쓴다
	if (auto* Required = Cast<UParticleModuleRequired>(Module); Required && Item.contains("Material"))
	{
		const json& MaterialItem = Item.at("Material");
		Required->Material = MaterialItem.is_null() ? nullptr : UMaterial::LoadMaterial(MaterialItem);
	}
	if (auto* Spawn = Cast<UParticleModuleSpawn>(Module); Spawn && Item.contains("Bursts"))
	{
		Spawn->BurstList.Reset();
		for (const json& Burst : Item.at("Bursts"))
		{
			Spawn->BurstList.Add(FParticleBurst{ Burst.at("Count").get<int32>(), Burst.value("CountLow", -1), Burst.at("Time").get<float>() });
		}
	}
	return Module;
}

// 여러 이미터가 같은 모듈 객체를 쓰면(공유) 처음 나올 때만 내용을 쓰고 "Id"를 붙인다. 다음부터는 {"Ref": Id}만 쓴다.
// 읽을 때 같은 Id는 같은 객체가 되어 공유가 유지된다 (UE는 패키지 안 객체 참조로 저장한다)
struct FModuleShareTable
{
	std::unordered_map<const UParticleModule*, int32> SavedIds;
	std::unordered_map<int32, UParticleModule*> LoadedModules;
};

json WriteModule(UParticleModule* Module, FModuleShareTable& Table)
{
	if (!Module) return nullptr;
	const auto Found = Table.SavedIds.find(Module);
	if (Found != Table.SavedIds.end()) return { { "Ref", Found->second } };
	const int32 Id = static_cast<int32>(Table.SavedIds.size());
	Table.SavedIds[Module] = Id;
	json Item = SaveModule(Module);
	Item["Id"] = Id;
	return Item;
}

template<typename T>
T* ReadModule(const json& Item, UObject* Outer, FModuleShareTable& Table)
{
	if (Item.is_null()) return nullptr;
	if (Item.contains("Ref"))
	{
		const auto Found = Table.LoadedModules.find(Item.at("Ref").get<int32>());
		T* Shared = Found != Table.LoadedModules.end() ? Cast<T>(Found->second) : nullptr;
		if (!Shared) throw std::runtime_error("Invalid shared particle module reference");
		return Shared;
	}
	T* Module = LoadModule<T>(Item, Outer);
	// Id가 없는 옛 파일은 공유 없이 읽는다
	if (Item.contains("Id")) Table.LoadedModules[Item.at("Id").get<int32>()] = Module;
	return Module;
}

const UParticleModuleTypeDataBase* GetLODTypeData(const UParticleLODLevel& LOD)
{
	const UParticleModuleTypeDataBase* TypeData = LOD.TypeDataModule;
	bool bListed = false;
	for (UParticleModule* Module : LOD.GetModules())
	{
		if (const auto* ListedType = Cast<UParticleModuleTypeDataBase>(Module))
		{
			if (bListed || (TypeData && TypeData != ListedType))
			{
				throw std::runtime_error("Multiple particle TypeData modules");
			}
			TypeData = ListedType;
			bListed = true;
		}
	}
	return TypeData;
}

void ValidateEmitter(const UParticleEmitter& Emitter)
{
	if (Emitter.LODLevels.IsEmpty())
	{
		throw std::runtime_error("Particle emitter has no LOD");
	}
	const UParticleLODLevel* HighLOD = Emitter.LODLevels[0];
	if (!HighLOD || !HighLOD->RequiredModule || !HighLOD->SpawnModule)
	{
		throw std::runtime_error("Missing particle LOD modules");
	}
	const UParticleModuleTypeDataBase* HighTypeData = GetLODTypeData(*HighLOD);
	for (const UParticleLODLevel* LOD : Emitter.LODLevels)
	{
		if (!LOD || !LOD->RequiredModule || !LOD->SpawnModule)
		{
			throw std::runtime_error("Missing particle LOD modules");
		}
		if (LOD->RequiredModule->bUseLocalSpace != HighLOD->RequiredModule->bUseLocalSpace)
		{
			throw std::runtime_error("Particle LOD spaces differ");
		}
		const UParticleModuleTypeDataBase* TypeData = GetLODTypeData(*LOD);
		if ((HighTypeData == nullptr) != (TypeData == nullptr) ||
			(HighTypeData && HighTypeData->GetClass() != TypeData->GetClass()))
		{
			throw std::runtime_error("Particle LOD TypeData differs");
		}
		const auto& HighModules = HighLOD->GetModules();
		const auto& Modules = LOD->GetModules();
		if (HighModules.Num() != Modules.Num())
		{
			throw std::runtime_error("Particle LOD module layouts differ");
		}
		for (int32 Index = 0; Index < Modules.Num(); ++Index)
		{
			if (!HighModules[Index] || !Modules[Index] || HighModules[Index]->GetClass() != Modules[Index]->GetClass())
			{
				throw std::runtime_error("Particle LOD module layouts differ");
			}
		}
		FString ValidationError;
		if (!UParticleModuleSubUV::ValidateLOD(*LOD, ValidationError) ||
			!UParticleModuleCollision::ValidateLOD(*LOD, ValidationError))
		{
			throw std::runtime_error(ValidationError);
		}
	}
}

// 이미터 하나의 로드와 시스템 끝 등록
UParticleEmitter* ReadEmitter(UParticleSystem* System, const json& EmitterItem, FModuleShareTable& Table)
{
	UParticleEmitter* Emitter = System->AddEmitter();
	Emitter->EmitterName = EmitterItem.value("Name", FString("Particle Emitter"));
	Emitter->InitialAllocationCount = EmitterItem.value("InitialAllocationCount", 0);
	for (const json& LODItem : EmitterItem.at("LODLevels"))
	{
		UParticleLODLevel* LOD = Emitter->AddLODLevel();
		LOD->Level = LODItem.value("Level", LOD->Level);
		LOD->bEnabled = LODItem.value("Enabled", true);
		LOD->PeakActiveParticles = LODItem.value("PeakActiveParticles", 0);
		if (LODItem.contains("Required") && !LODItem.at("Required").is_null())
		{
			delete LOD->RequiredModule;
			LOD->RequiredModule = nullptr;
			LOD->RequiredModule = ReadModule<UParticleModuleRequired>(LODItem.at("Required"), LOD, Table);
		}
		if (LODItem.contains("Spawn") && !LODItem.at("Spawn").is_null())
		{
			// 교체 로드가 성공한 뒤 기존 Spawn과 기본 분포를 정리한다.
			UParticleModuleSpawn* Replacement = ReadModule<UParticleModuleSpawn>(LODItem.at("Spawn"), LOD, Table);
			delete LOD->SpawnModule->Rate.Distribution;
			delete LOD->SpawnModule->RateScale.Distribution;
			delete LOD->SpawnModule->BurstScale.Distribution;
			delete LOD->SpawnModule;
			LOD->SpawnModule = Replacement;
		}
		if (LODItem.contains("TypeData")) LOD->TypeDataModule = ReadModule<UParticleModuleTypeDataBase>(LODItem.at("TypeData"), LOD, Table);
		if (LODItem.contains("Modules"))
		{
			for (const json& ModuleItem : LODItem.at("Modules"))
			{
				UParticleModule* Module = ReadModule<UParticleModule>(ModuleItem, LOD, Table);
				if (!Module)
				{
					throw std::runtime_error("Null particle module");
				}
				LOD->Modules.Add(Module);
			}
		}
	}
	ValidateEmitter(*Emitter);
	return Emitter;
}

void ValidateLODSettings(const UParticleSystem& System)
{
	if (!std::isfinite(System.LODDistanceCheckTime) || System.LODDistanceCheckTime < 0.0f)
	{
		throw std::runtime_error("Invalid particle LODDistanceCheckTime");
	}

	float PreviousDistance = 0.0f;
	for (float Distance : System.LODDistances)
	{
		if (!std::isfinite(Distance) || Distance < PreviousDistance)
		{
			throw std::runtime_error("Invalid particle LODDistances");
		}
		PreviousDistance = Distance;
	}
}

void ReadSystem(UParticleSystem* System, const json& Root)
{
	System->LODDistanceCheckTime = Root.value("LODDistanceCheckTime", System->LODDistanceCheckTime);
	System->LODDistances.Reset();
	if (const auto Distances = Root.find("LODDistances"); Distances != Root.end())
	{
		if (!Distances->is_array())
		{
			throw std::runtime_error("Particle LODDistances must be an array");
		}
		for (const json& Distance : *Distances)
		{
			System->LODDistances.Add(Distance.get<float>());
		}
	}
	ValidateLODSettings(*System);

	FModuleShareTable Table;
	for (const json& EmitterItem : Root.at("Emitters"))
	{
		ReadEmitter(System, EmitterItem, Table);
	}
}

json WriteEmitter(UParticleEmitter* Emitter, FModuleShareTable& Table)
{
	if (!Emitter)
	{
		throw std::runtime_error("Particle emitter is null");
	}
	ValidateEmitter(*Emitter);
	json EmitterItem = { { "Name", Emitter->EmitterName }, { "InitialAllocationCount", Emitter->InitialAllocationCount }, { "LODLevels", json::array() } };
	for (UParticleLODLevel* LOD : Emitter->GetLODLevels())
	{
		json LODItem = { { "Level", LOD->Level }, { "Enabled", LOD->bEnabled }, { "PeakActiveParticles", LOD->PeakActiveParticles },
			{ "Required", WriteModule(LOD->RequiredModule, Table) }, { "Spawn", WriteModule(LOD->SpawnModule, Table) },
			{ "TypeData", WriteModule(LOD->TypeDataModule, Table) }, { "Modules", json::array() } };
		for (UParticleModule* Module : LOD->GetModules()) LODItem["Modules"].push_back(WriteModule(Module, Table));
		EmitterItem["LODLevels"].push_back(std::move(LODItem));
	}
	return EmitterItem;
}

json WriteSystem(UParticleSystem* System)
{
	ValidateLODSettings(*System);
	json Root = {
		{ "LODDistances", json::array() },
		{ "LODDistanceCheckTime", System->LODDistanceCheckTime },
		{ "Emitters", json::array() }
	};
	for (float Distance : System->LODDistances)
	{
		Root["LODDistances"].push_back(Distance);
	}
	FModuleShareTable Table;
	for (UParticleEmitter* Emitter : System->GetEmitters())
	{
		Root["Emitters"].push_back(WriteEmitter(Emitter, Table));
	}
	return Root;
}
} // namespace

bool FParticleSystemLoader::ValidateAsset(const UParticleSystem* System, FString& Error)
{
	Error.clear();
	if (!System)
	{
		Error = "Particle system is null";
		return false;
	}
	try
	{
		ValidateLODSettings(*System);
		for (const UParticleEmitter* Emitter : System->GetEmitters())
		{
			if (!Emitter)
			{
				throw std::runtime_error("Particle emitter is null");
			}
			ValidateEmitter(*Emitter);
		}
		return true;
	}
	catch (const std::exception& Exception)
	{
		Error = Exception.what();
		return false;
	}
}

UParticleSystem* FParticleSystemLoader::LoadAsset(const FString& Key, const FString& Path, FString& Error)
{
	Error.clear();
	if (UParticleSystem* Existing = UAssetManager::GetAssetByPath<UParticleSystem>(Key)) return Existing;
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
		System->UpdateAndBuild();
		System->SetPath(Key);
		// 썸네일은 없어도 된다
		const FString ThumbnailPath = GetThumbnailPath(Path);
		if (std::filesystem::exists(ToFilePath(ThumbnailPath)))
		{
			System->ThumbnailImage = UAssetManager::Get().CreateTransientTexture(ImageLoader::Load(ThumbnailPath), System);
		}
		TMap<FString, URenderAsset*> Replacements;
		Replacements.Add(Key, System);
		TMap<FString, URenderAsset*> Prepared;
		UAssetManager::Get().PrepareAssetRegistration(Replacements, Prepared);
		UAssetManager::Get().CommitAssetRegistration(Prepared);
		return System;
	}
	catch (const std::exception& Exception)
	{
		// 실패한 조립 중간 결과에는 생성자가 만든 분포 서브오브젝트도 포함한다.
		DestroyParticleObject(System);
		Error = Exception.what();
		return nullptr;
	}
}

// UE DuplicateObject 대신 저장 형식을 한 번 거쳐 모듈과 분포까지 새 객체로 만든다
UParticleEmitter* FParticleSystemLoader::DuplicateEmitter(UParticleEmitter* Source, UParticleSystem* DestSystem, int32 InsertIndex, FString& Error)
{
	Error.clear();
	if (!Source || !DestSystem)
	{
		Error = "Emitter or system is null";
		return nullptr;
	}
	const int32 OldCount = DestSystem->Emitters.Num();
	try
	{
		// 표를 따로 쓰므로 다른 이미터와의 공유는 복사가 된다. 이 이미터 안 LOD끼리의 공유만 남는다
		FModuleShareTable WriteTable;
		FModuleShareTable ReadTable;
		UParticleEmitter* NewEmitter = ReadEmitter(DestSystem, WriteEmitter(Source, WriteTable), ReadTable);
		// ReadEmitter는 끝에 붙이므로 원하는 자리로 옮긴다
		DestSystem->Emitters.RemoveAt(static_cast<uint32>(OldCount));
		DestSystem->Emitters.Insert(NewEmitter, static_cast<uint32>(std::clamp(InsertIndex, 0, OldCount)));
		NewEmitter->UpdateModuleLists();
		return NewEmitter;
	}
	catch (const std::exception& Exception)
	{
		// 읽다 실패한 이미터는 끝에 붙어 있다
		if (DestSystem->Emitters.Num() > OldCount)
		{
			UParticleEmitter* Partial = DestSystem->Emitters[OldCount];
			DestSystem->Emitters.RemoveAt(static_cast<uint32>(OldCount));
			DestroyParticleObject(Partial);
		}
		Error = Exception.what();
		return nullptr;
	}
}

// UE StaticDuplicateObject 대신 저장 형식을 한 번 거쳐 모듈과 분포를 새 객체로 만든다
UParticleModule* FParticleSystemLoader::DuplicateModule(UParticleModule* Source, UObject* Outer, FString& Error)
{
	Error.clear();
	if (!Source)
	{
		Error = "Module is null";
		return nullptr;
	}
	try
	{
		return LoadModule<UParticleModule>(SaveModule(Source), Outer);
	}
	catch (const std::exception& Exception)
	{
		Error = Exception.what();
		return nullptr;
	}
}

bool FParticleSystemLoader::ReloadAsset(UParticleSystem* System, const FString& Path, FString& Error)
{
	Error.clear();
	if (!System)
	{
		Error = "Particle system is null";
		return false;
	}
	UParticleSystem* Loaded = nullptr;
	try
	{
		std::ifstream File(ToFilePath(Path));
		if (!File.is_open())
		{
			Error = "Cannot open file: " + Path;
			return false;
		}
		const json Root = json::parse(File);
		// 다 읽은 뒤에만 바꾼다. 실패하면 지금 내용이 그대로 남는다
		Loaded = NewObject<UParticleSystem>();
		ReadSystem(Loaded, Root);
		Loaded->UpdateAndBuild();
	}
	catch (const std::exception& Exception)
	{
		DestroyParticleObject(Loaded);
		Error = Exception.what();
		return false;
	}

	// 옛 이미터를 지우고 새 이미터를 옮겨 온다. System 객체 주소는 그대로라 Template으로 쓰는 컴포넌트가 따라온다
	for (UParticleEmitter* Emitter : System->Emitters)
	{
		DestroyParticleObject(Emitter);
	}
	System->LODDistances = std::move(Loaded->LODDistances);
	System->LODDistanceCheckTime = Loaded->LODDistanceCheckTime;
	System->Emitters = std::move(Loaded->Emitters);
	for (UParticleEmitter* Emitter : System->Emitters)
	{
		Emitter->SetOuter(System);
	}
	Loaded->Emitters.Empty();
	DestroyParticleObject(Loaded);
	return true;
}

bool FParticleSystemLoader::SaveAsset(UParticleSystem* System, const FString& Path, FString& Error)
{
	Error.clear();
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

FString FParticleSystemLoader::GetThumbnailPath(const FString& AssetFilePath)
{
	return AssetFilePath + ".thumbnail";
}

bool FParticleSystemLoader::SaveThumbnail(UParticleSystem* System, const FString& AssetFilePath, const FImageData& Image, FString& Error)
{
	Error.clear();
	if (!System || !Image.IsValid())
	{
		Error = "No image";
		return false;
	}
	if (!ImageLoader::SavePNG(GetThumbnailPath(AssetFilePath), Image))
	{
		Error = "Cannot write " + GetThumbnailPath(AssetFilePath);
		return false;
	}

	UTexture2D* Thumbnail = System->ThumbnailImage;
	// 이번 프레임 ImGui가 옛 SRV를 이미 잡았을 수 있어, 크기가 같으면 객체를 두고 내용만 바꾼다
	if (Thumbnail && Thumbnail->GetWidth() == Image.Width && Thumbnail->GetHeight() == Image.Height)
	{
		RenderCommand::UpdateTexture(Thumbnail->GetResource(), Image);
		return true;
	}
	// 크기가 다르면 새로 만든다. 옛 객체는 System이 지워질 때 함께 지워진다
	if (UTexture2D* NewThumbnail = UAssetManager::Get().CreateTransientTexture(Image, System))
	{
		System->ThumbnailImage = NewThumbnail;
	}
	return true;
}
