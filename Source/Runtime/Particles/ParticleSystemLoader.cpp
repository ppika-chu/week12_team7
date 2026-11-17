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
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Math/InterpCurve.h"
#include "Serialization/TypeSerializer.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

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

// 이미터 하나를 읽어 System 끝에 붙인다
UParticleEmitter* ReadEmitter(UParticleSystem* System, const json& EmitterItem)
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
			LOD->RequiredModule = LoadModule<UParticleModuleRequired>(LODItem.at("Required"), LOD);
		}
		if (LODItem.contains("Spawn") && !LODItem.at("Spawn").is_null())
		{
			// 교체 로드가 성공한 뒤 기존 Spawn과 기본 분포를 정리한다.
			UParticleModuleSpawn* Replacement = LoadModule<UParticleModuleSpawn>(LODItem.at("Spawn"), LOD);
			delete LOD->SpawnModule->Rate.Distribution;
			delete LOD->SpawnModule->RateScale.Distribution;
			delete LOD->SpawnModule->BurstScale.Distribution;
			delete LOD->SpawnModule;
			LOD->SpawnModule = Replacement;
		}
		if (LODItem.contains("TypeData")) LOD->TypeDataModule = LoadModule<UParticleModuleTypeDataBase>(LODItem.at("TypeData"), LOD);
		if (LODItem.contains("Modules"))
		{
			for (const json& ModuleItem : LODItem.at("Modules"))
			{
				if (UParticleModule* Module = LoadModule<UParticleModule>(ModuleItem, LOD)) LOD->Modules.Add(Module);
			}
		}
	}
	// payload 오프셋은 최상위 LOD의 모듈 순서를 공유한다.
	for (int32 LODIndex = 1; LODIndex < Emitter->LODLevels.Num(); ++LODIndex)
	{
		const auto& HighModules = Emitter->LODLevels[0]->Modules;
		const auto& Modules = Emitter->LODLevels[LODIndex]->Modules;
		if (HighModules.Num() != Modules.Num()) throw std::runtime_error("Particle LOD module layouts differ");
		for (int32 Index = 0; Index < Modules.Num(); ++Index)
		{
			if (HighModules[Index]->GetClass() != Modules[Index]->GetClass()) throw std::runtime_error("Particle LOD module layouts differ");
		}
		UParticleModuleTypeDataBase* HighType = Emitter->LODLevels[0]->TypeDataModule;
		UParticleModuleTypeDataBase* Type = Emitter->LODLevels[LODIndex]->TypeDataModule;
		if ((HighType == nullptr) != (Type == nullptr) || (HighType && HighType->GetClass() != Type->GetClass()))
		{
			throw std::runtime_error("Particle LOD type-data layouts differ");
		}
	}
	return Emitter;
}

void ReadSystem(UParticleSystem* System, const json& Root)
{
	for (const json& EmitterItem : Root.at("Emitters"))
	{
		ReadEmitter(System, EmitterItem);
	}
}

json WriteEmitter(UParticleEmitter* Emitter)
{
	json EmitterItem = { { "Name", Emitter->EmitterName }, { "InitialAllocationCount", Emitter->InitialAllocationCount }, { "LODLevels", json::array() } };
	for (UParticleLODLevel* LOD : Emitter->GetLODLevels())
	{
		json LODItem = { { "Level", LOD->Level }, { "Enabled", LOD->bEnabled }, { "PeakActiveParticles", LOD->PeakActiveParticles },
			{ "Required", SaveModule(LOD->RequiredModule) }, { "Spawn", SaveModule(LOD->SpawnModule) },
			{ "TypeData", SaveModule(LOD->TypeDataModule) }, { "Modules", json::array() } };
		for (UParticleModule* Module : LOD->GetModules()) LODItem["Modules"].push_back(SaveModule(Module));
		EmitterItem["LODLevels"].push_back(std::move(LODItem));
	}
	return EmitterItem;
}

json WriteSystem(UParticleSystem* System)
{
	json Root = { { "Emitters", json::array() } };
	for (UParticleEmitter* Emitter : System->GetEmitters())
	{
		Root["Emitters"].push_back(WriteEmitter(Emitter));
	}
	return Root;
}
} // namespace

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
		UParticleEmitter* NewEmitter = ReadEmitter(DestSystem, WriteEmitter(Source));
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
