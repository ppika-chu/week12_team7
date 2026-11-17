#include "ParticleTestSupport.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleSystemLoader.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"

#include <filesystem>
#include <fstream>

namespace
{
json ReadParticleJson(const char* Path)
{
	std::ifstream File(Path);
	return json::parse(File);
}

void WriteParticleJson(const char* Path, const json& Root)
{
	std::ofstream File(Path);
	File << Root.dump(1, '\t');
}
}

void TestParticleLoader()
{
	std::filesystem::create_directories("Build/ParticleTests");
	if (!UAssetManager::GetAssetByPath<UMaterial>("ParticleSpriteMaterial"))
	{
		UAssetManager::Get().RegisterAsset("ParticleSpriteMaterial", NewObject<UMaterial>());
	}
	FString Error;
	UParticleSystem* First = FParticleSystemLoader::LoadAsset("ParticleLoaderTest.First", "Assets/Particles/PSCIntegration.particle", Error);
	if (!First || !Error.empty()) throw std::runtime_error("Particle sample must load: " + Error);
	Require(First->LODDistances.IsEmpty() && NearlyEqual(First->LODDistanceCheckTime, 0.25f), "Assets without distance settings must use LOD defaults");
	UParticleLODLevel* LOD = First->GetEmitters()[0]->GetLODLevels()[0];
	Require(LOD->RequiredModule->Material == UAssetManager::GetAssetByPath<UMaterial>("ParticleSpriteMaterial"), "Particle material reference must survive load");
	Require(LOD->RequiredModule->SortMode == PSORTMODE_DistanceToView, "Particle order setting must survive load");
	Require(LOD->GetModules().Num() == 6, "Particle module order/count must survive load");
	Require(LOD->SpawnModule->BurstList.Num() == 1 && LOD->SpawnModule->BurstList[0].CountLow == 2, "Burst range must survive load");
	auto* Rate = Cast<UDistributionFloatUniformCurve>(LOD->SpawnModule->Rate.Distribution);
	Require(Rate && Rate->ConstantCurve.Points.Num() == 2 && NearlyEqual(Rate->ConstantCurve.Points[1].OutVal.Y, 10.0f), "Uniform float curve must preserve values");
	auto* RateScale = Cast<UDistributionFloatConstant>(LOD->SpawnModule->RateScale.Distribution);
	Require(RateScale && NearlyEqual(RateScale->GetConstant(), 1.0f), "Float constant must preserve value");
	auto* LifetimeModule = Cast<UParticleModuleLifetime>(LOD->GetModules()[0]);
	auto* LifetimeDistribution = LifetimeModule ? Cast<UDistributionFloatUniform>(LifetimeModule->Lifetime.Distribution) : nullptr;
	Require(LifetimeDistribution && NearlyEqual(LifetimeDistribution->Min, 0.9f) && NearlyEqual(LifetimeDistribution->Max, 1.1f), "Float uniform must preserve range");
	auto* Location = Cast<UParticleModuleLocation>(LOD->GetModules()[1]);
	auto* LocationRange = Location ? Cast<UDistributionVectorUniform>(Location->StartLocation.Distribution) : nullptr;
	Require(LocationRange && NearlyEqual(LocationRange->Min.X, -0.1f) && NearlyEqual(LocationRange->Max.Y, 0.1f), "Vector uniform must preserve range");
	auto* Velocity = Cast<UParticleModuleVelocity>(LOD->GetModules()[2]);
	auto* VelocityValue = Velocity ? Cast<UDistributionVectorConstant>(Velocity->StartVelocity.Distribution) : nullptr;
	Require(VelocityValue && NearlyEqual(VelocityValue->GetConstant().Z, 2.0f), "Vector constant must preserve value");
	auto* Size = Cast<UParticleModuleSizeMultiplyLife>(LOD->GetModules()[4]);
	auto* SizeCurve = Size ? Cast<UDistributionVectorConstantCurve>(Size->LifeMultiplier.Distribution) : nullptr;
	Require(SizeCurve && SizeCurve->ConstantCurve.Points[0].InterpMode == EInterpCurveMode::CurveUser, "Curve interpolation mode must survive load");
	Require(NearlyEqual(SizeCurve->ConstantCurve.Points[0].LeaveTangent.X, 1.0f), "Curve tangents must survive load");
	auto* Color = Cast<UParticleModuleColorOverLife>(LOD->GetModules()[5]);
	auto* ColorRange = Color ? Cast<UDistributionVectorUniformCurve>(Color->ColorOverLife.Distribution) : nullptr;
	Require(ColorRange && NearlyEqual(ColorRange->MinCurve.Points[0].OutVal.X, 0.8f) && NearlyEqual(ColorRange->MaxCurve.Points[1].OutVal.Z, 1.0f), "Vector uniform curve must preserve both curves");
	auto* Alpha = Color ? Cast<UDistributionFloatConstantCurve>(Color->AlphaOverLife.Distribution) : nullptr;
	Require(Alpha && Alpha->ConstantCurve.Points.Num() == 4 && Alpha->ConstantCurve.Points[1].InterpMode == EInterpCurveMode::Constant, "Float constant curve must preserve keys/modes");
	First->LODDistances.Add(0.0f);
	First->LODDistances.Add(10.0f);
	First->LODDistanceCheckTime = 0.5f;
	Require(FParticleSystemLoader::SaveAsset(First, "Build/ParticleTests/First.particle", Error), "Particle save must succeed");
	UParticleSystem* Second = FParticleSystemLoader::LoadAsset("ParticleLoaderTest.Second", "Build/ParticleTests/First.particle", Error);
	if (!Second || !FParticleSystemLoader::SaveAsset(Second, "Build/ParticleTests/Second.particle", Error))
	{
		throw std::runtime_error("Particle reload/save must succeed: " + Error);
	}
	Require(Second->LODDistances.Num() == 2 && NearlyEqual(Second->LODDistances[0], 0.0f) && NearlyEqual(Second->LODDistances[1], 10.0f),
		"Particle LOD distances must roundtrip");
	Require(NearlyEqual(Second->LODDistanceCheckTime, 0.5f), "Particle LOD distance check time must roundtrip");
	Require(ReadParticleJson("Build/ParticleTests/First.particle") == ReadParticleJson("Build/ParticleTests/Second.particle"), "All authored particle data must roundtrip");

	json Invalid = ReadParticleJson("Assets/Particles/PSCIntegration.particle");
	Invalid["Emitters"][0]["LODLevels"][0]["Modules"][5]["Distributions"]["AlphaOverLife"]["Class"] = "UObject";
	WriteParticleJson("Build/ParticleTests/InvalidClass.particle", Invalid);
	const int32 CountBeforeInvalidClass = GUObjectArray.Num();
	Require(!FParticleSystemLoader::LoadAsset("ParticleLoaderTest.InvalidClass", "Build/ParticleTests/InvalidClass.particle", Error) && !Error.empty(), "Invalid distribution class must fail");
	Require(GUObjectArray.Num() == CountBeforeInvalidClass, "Failed nested load must release every temporary object");
	Require(!UAssetManager::GetAssetByPath<UParticleSystem>("ParticleLoaderTest.InvalidClass"), "Failed particle load must not register asset");

	Invalid = ReadParticleJson("Assets/Particles/PSCIntegration.particle");
	json LowerLOD = Invalid["Emitters"][0]["LODLevels"][0];
	LowerLOD["Level"] = 1;
	LowerLOD["Modules"].erase(LowerLOD["Modules"].begin());
	Invalid["Emitters"][0]["LODLevels"].push_back(std::move(LowerLOD));
	WriteParticleJson("Build/ParticleTests/InvalidLOD.particle", Invalid);
	const int32 CountBeforeInvalidLOD = GUObjectArray.Num();
	Require(!FParticleSystemLoader::LoadAsset("ParticleLoaderTest.InvalidLOD", "Build/ParticleTests/InvalidLOD.particle", Error) && !Error.empty(), "Mismatched LOD payload layout must fail");
	Require(GUObjectArray.Num() == CountBeforeInvalidLOD, "Invalid LOD load must release all temporary objects");

	json LegacyLODJson = {
		{ "Spawn", { { "Class", "UParticleModuleSpawn" }, { "Properties", { { "Rate", 7.0f }, { "RateScale", 2.0f } } } } },
		{ "Modules", json::array({ { { "Class", "UParticleModuleLifetime" }, { "Properties", { { "LifetimeMin", 2.0f }, { "LifetimeMax", 3.0f } } } } }) }
	};
	json Legacy = { { "Emitters", json::array({ { { "LODLevels", json::array({ LegacyLODJson }) } } }) } };
	WriteParticleJson("Build/ParticleTests/Legacy.particle", Legacy);
	UParticleSystem* LegacySystem = FParticleSystemLoader::LoadAsset("ParticleLoaderTest.Legacy", "Build/ParticleTests/Legacy.particle", Error);
	Require(LegacySystem, "Original Cascade particle format must load");
	UParticleLODLevel* LegacyLOD = LegacySystem->GetEmitters()[0]->GetLODLevels()[0];
	Require(NearlyEqual(LegacyLOD->SpawnModule->Rate.GetValue(), 7.0f) && NearlyEqual(LegacyLOD->SpawnModule->RateScale.GetValue(), 2.0f), "Original Cascade spawn settings must survive migration");
	auto* Lifetime = Cast<UParticleModuleLifetime>(LegacyLOD->GetModules()[0]);
	auto* LifetimeRange = Cast<UDistributionFloatUniform>(Lifetime->Lifetime.Distribution);
	Require(LifetimeRange && NearlyEqual(LifetimeRange->Min, 2.0f) && NearlyEqual(LifetimeRange->Max, 3.0f), "Original Cascade lifetime range must survive migration");
}
