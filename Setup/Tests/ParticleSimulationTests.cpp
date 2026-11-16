#include "ParticleTestSupport.h"

#include "Distributions/DistributionVector.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include <limits>

namespace
{
struct FTestEmitterOwner : IParticleEmitterInstanceOwner
{
	FRandomStream Random{ 12345 };
	int32 Limit = 256;
	FVector Location = FVector::ZeroVector;
	FVector GetComponentLocation() const override { return Location; }
	FMatrix GetComponentToWorld() const override { return FMatrix::Identity; }
	FRandomStream& GetRandomStream() override { return Random; }
	float GetEmitterDelay() const override { return 0.0f; }
	bool GetJustRegistered() const override { return false; }
	int32 GetMaxParticles() const override { return Limit; }
};

FBaseParticle& GetParticle(FParticleEmitterInstance& Instance, int32 Index)
{
	return *reinterpret_cast<FBaseParticle*>(Instance.ParticleData +
		Instance.ParticleStride * Instance.ParticleIndices[Index]);
}

void SetVector(FRawDistributionVector& Target, const FVector& Value)
{
	auto* Constant = NewObject<UDistributionVectorConstant>();
	Constant->SetConstant(Value);
	Target.SetDistribution(Constant);
}

class UTestPayloadModule : public UParticleModule
{
	DECLARE_CLASS(UTestPayloadModule, UParticleModule)

public:
	UTestPayloadModule() { bSpawnModule = bUpdateModule = bFinalUpdateModule = true; }
	uint32 RequiredBytes(UParticleModuleTypeDataBase*) override { return sizeof(float); }
	uint32 RequiredBytesPerInstance() override { return sizeof(int32); }
	uint32 PrepPerInstanceBlock(FParticleEmitterInstance*, void* Data) override
	{
		*static_cast<int32*>(Data) = 7;
		return sizeof(int32);
	}
	void Spawn(const FSpawnContext& Context) override
	{
		*reinterpret_cast<float*>(reinterpret_cast<uint8*>(Context.ParticleBase) + Context.Offset) =
			static_cast<float>(*reinterpret_cast<int32*>(Context.Owner.GetModuleInstanceData(this)));
	}
	void Update(const FUpdateContext& Context) override
	{
		BEGIN_UPDATE_LOOP
			*reinterpret_cast<float*>(const_cast<uint8*>(ParticleBase) + Context.Offset) += Context.DeltaTime;
		END_UPDATE_LOOP
	}
	void FinalUpdate(const FUpdateContext& Context) override
	{
		++*reinterpret_cast<int32*>(Context.Owner.GetModuleInstanceData(this));
	}
};

void TestDistributionsAndCurves()
{
	FInterpCurveFloat Curve;
	Require(NearlyEqual(Curve.Eval(0.5f, 9.0f), 9.0f), "empty curve fallback");
	Curve.AddPoint(1.0f, 10.0f);
	Curve.AddPoint(0.0f, 0.0f);
	Curve.AddPoint(1.0f, 4.0f);
	Require(Curve.GetPoints().Num() == 2 && NearlyEqual(Curve.Eval(0.5f), 2.0f), "duplicate curve key replaces value");
	Require(NearlyEqual(Curve.Eval(-1.0f), 0.0f) && NearlyEqual(Curve.Eval(2.0f), 4.0f), "curve endpoint clamping");
	Require(Curve.AddPoint(std::numeric_limits<float>::infinity(), 0.0f) == -1, "nonfinite curve key rejected");
	Require(NearlyEqual(Curve.Eval(std::numeric_limits<float>::quiet_NaN(), 9.0f), 9.0f), "nan curve input fallback");
	Curve.Points[0].InterpMode = EInterpCurveMode::Constant;
	Require(NearlyEqual(Curve.Eval(0.5f), 0.0f) && NearlyEqual(Curve.Eval(1.0f), 4.0f), "constant interpolation endpoints");
	Curve.Points[0].InterpMode = EInterpCurveMode::CurveUser;
	Require(NearlyEqual(Curve.Eval(0.5f), 2.0f), "cubic interpolation");
	Require(Curve.RemovePoint(1) && !Curve.RemovePoint(8), "curve removal bounds");
	FInterpCurveFloat WideCurve;
	const float Largest = std::numeric_limits<float>::max();
	WideCurve.AddPoint(-Largest, -Largest);
	WideCurve.AddPoint(Largest, Largest);
	Require(NearlyEqual(WideCurve.Eval(0.0f), 0.0f), "wide finite linear curves avoid intermediate overflow");

	auto* Constant = NewObject<UDistributionFloatConstant>();
	Constant->SetConstant(2.0f);
	FRawDistributionFloat Cache;
	Cache.SetDistribution(Constant);
	Require(NearlyEqual(Cache.GetValue(0.5f), 2.0f), "constant cache initial");
	Constant->SetConstant(3.0f);
	Require(NearlyEqual(Cache.GetValue(0.5f), 3.0f), "constant revision invalidates cache");
	auto* Replacement = NewObject<UDistributionFloatConstant>();
	Replacement->SetConstant(6.0f);
	Cache.Distribution = Replacement;
	Require(NearlyEqual(Cache.GetValue(0.5f), 6.0f), "CY direct distribution replacement invalidates cache");
	auto* Dynamic = NewObject<UDistributionFloatConstantCurve>();
	Dynamic->AddPoint(0.0f, 0.0f);
	Dynamic->AddPoint(1.0f, 10.0f);
	Cache.SetDistribution(Dynamic);
	Require(NearlyEqual(Cache.GetValue(0.2f), 2.0f) && NearlyEqual(Cache.GetValue(0.8f), 8.0f), "curve cache never freezes time");

	auto* Uniform = NewObject<UDistributionVectorUniform>();
	Uniform->Min = FVector(-1.0f, -2.0f, -3.0f);
	Uniform->Max = FVector(1.0f, 2.0f, 3.0f);
	FRandomStream A(77), B(77);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const FVector Value = Uniform->GetValue(0.0f, &A);
		const FVector Repeat = Uniform->GetValue(0.0f, &B);
		Require(NearlyEqual(Value.X, Repeat.X) && NearlyEqual(Value.Y, Repeat.Y) && NearlyEqual(Value.Z, Repeat.Z), "seeded distributions repeat");
		Require(Value.X >= -1.0f && Value.X <= 1.0f && Value.Y >= -2.0f && Value.Y <= 2.0f && Value.Z >= -3.0f && Value.Z <= 3.0f, "uniform axes remain in range");
	}
}

void TestParticleAllocationAndCap()
{
	FParticleDataContainer Storage;
	Require(Storage.Allocate(65536, 128), "full uint16 slot range allocates");
	Require(Storage.ParticleIndices[65535] == 65535, "last uint16 slot preserved");
	uint8* Original = Storage.ParticleData;
	Require(!Storage.Allocate(65537, 128) && !Storage.Allocate(1, 127) && Storage.ParticleData == Original, "invalid allocations retain original storage");
	Require(!Storage.Allocate(65536, 1 << 30), "allocation overflow rejected");

	FTestEmitterOwner Owner;
	Owner.Limit = 2;
	std::unique_ptr<UParticleSystem> System{ MakeParticleTestSystem(4.0f, 0.0f) };
	std::unique_ptr<FParticleEmitterInstance> Instance{ System->Emitters[0]->CreateInstance(Owner) };
	Require(Instance && Instance->ParticleStride % 16 == 0 && !Instance->Resize(3), "component cap and aligned stride");
	Instance->Tick(0.125f, false);
	Require(Instance->ActiveParticles == 0, "fractional rate first half");
	Instance->Tick(0.125f, false);
	Require(Instance->ActiveParticles == 1, "fractional rate carries");
	Instance->Tick(0.25f, false);
	Instance->Tick(1.0f, false);
	Require(Instance->ActiveParticles == 2, "spawn never exceeds component cap");
	Instance->KillParticlesForced();
	Instance->Tick(0.125f, false);
	Require(Instance->ActiveParticles == 0, "discarded full-cap spawns produce no delayed debt");
	Instance->Tick(0.125f, false);
	Require(Instance->ActiveParticles == 1, "fractional rate resumes after cap clears");
	Instance->Tick(std::numeric_limits<float>::quiet_NaN(), false);
	Instance->Tick(-1.0f, false);
	Require(Instance->ActiveParticles == 1, "invalid tick cannot corrupt simulation");
}

void TestModulesAndReplay()
{
	FTestEmitterOwner Owner;
	std::unique_ptr<UParticleSystem> System{ MakeParticleTestSystem(4.0f, 10.0f, true) };
	UParticleLODLevel* LOD = System->Emitters[0]->GetLODLevel(0);
	SetVector(LOD->AddModule<UParticleModuleSize>()->StartSize, FVector(1.0f));
	SetVector(LOD->AddModule<UParticleModuleSizeMultiplyLife>()->LifeMultiplier, FVector(2.0f));
	SetVector(LOD->AddModule<UParticleModuleVelocity>()->StartVelocity, FVector(2.0f, 0.0f, 0.0f));
	LOD->AddModule<UParticleModuleColor>();
	System->UpdateAllModuleLists();
	std::unique_ptr<FParticleEmitterInstance> Instance{ System->Emitters[0]->CreateInstance(Owner) };
	Instance->Tick(0.5f, false);
	Require(Instance->ActiveParticles == 2, "two intra-frame particles spawn");
	Require(NearlyEqual(GetParticle(*Instance, 0).Location.X, 0.5f) && NearlyEqual(GetParticle(*Instance, 1).Location.X, 0.0f), "spawn times produce distinct locations");
	Require(NearlyEqual(GetParticle(*Instance, 0).Size.X, 2.0f), "size-by-life runs at spawn");
	Instance->Tick(0.25f, false);
	Require(NearlyEqual(GetParticle(*Instance, 0).Size.X, 2.0f), "size-by-life resets base before multiplying");
	std::unique_ptr<FDynamicEmitterDataBase> Replay{ Instance->GetDynamicData(false) };
	Require(Replay && Replay->GetSource().ActiveParticleCount == 3, "snapshot includes active particles");
	const auto& Source = Replay->GetSource();
	const FBaseParticle& Snapshot = *reinterpret_cast<const FBaseParticle*>(Source.DataContainer.ParticleData +
		Source.ParticleStride * Source.DataContainer.ParticleIndices[0]);
	const float OriginalX = Snapshot.Location.X;
	GetParticle(*Instance, 0).Location.X = 123.0f;
	Require(NearlyEqual(Snapshot.Location.X, OriginalX), "replay owns immutable particle copy");
}

void TestBurstsAndCompletion()
{
	FTestEmitterOwner Owner;
	std::unique_ptr<UParticleSystem> System{ MakeParticleTestSystem(0.0f, 0.0f) };
	UParticleLODLevel* LOD = System->Emitters[0]->GetLODLevel(0);
	LOD->RequiredModule->EmitterDuration = 1.0f;
	LOD->RequiredModule->EmitterLoops = 2;
	LOD->SpawnModule->BurstList.Add(FParticleBurst{ 2, -1, 0.0f });
	std::unique_ptr<FParticleEmitterInstance> Instance{ System->Emitters[0]->CreateInstance(Owner) };
	Instance->Tick(0.1f, false);
	Instance->Tick(0.1f, false);
	Require(Instance->ActiveParticles == 2, "burst fires once in loop");
	Instance->Tick(0.9f, false);
	Require(Instance->ActiveParticles == 4, "burst resets next loop");
	Instance->Tick(1.0f, false);
	Require(Instance->ActiveParticles == 4 && Instance->HasFinishedSpawning() && !Instance->HasCompleted(), "finite loops stop spawning while particles drain");
	Instance->KillParticlesForced();
	Require(Instance->HasCompleted(), "finite emitter completes after particles drain");

	std::unique_ptr<UParticleSystem> KillSystem{ MakeParticleTestSystem(10.0f, 10.0f) };
	UParticleLODLevel* KillLOD = KillSystem->Emitters[0]->GetLODLevel(0);
	KillLOD->RequiredModule->EmitterDuration = 0.2f;
	KillLOD->RequiredModule->EmitterLoops = 1;
	KillLOD->RequiredModule->bKillOnCompleted = true;
	std::unique_ptr<FParticleEmitterInstance> KillInstance{ KillSystem->Emitters[0]->CreateInstance(Owner) };
	KillInstance->Tick(0.1f, false);
	Require(KillInstance->ActiveParticles == 1, "finite emitter produces initial particle");
	KillInstance->Tick(0.11f, false);
	Require(KillInstance->ActiveParticles == 0 && KillInstance->HasCompleted(), "kill-on-complete kills living particles at loop end");

	// Seed 2 gives a short second loop and a longer sampled duration at completion.
	// Total elapsed time cannot be compared to the latest duration multiplied by loop count.
	FTestEmitterOwner RandomOwner;
	RandomOwner.Random.Initialize(2);
	std::unique_ptr<UParticleSystem> RandomSystem{ MakeParticleTestSystem(100.0f, 0.0f) };
	UParticleModuleRequired* RandomRequired = RandomSystem->Emitters[0]->GetLODLevel(0)->RequiredModule;
	RandomRequired->EmitterDurationLow = 0.1f;
	RandomRequired->EmitterDuration = 1.0f;
	RandomRequired->bEmitterDurationUseRange = true;
	RandomRequired->bDurationRecalcEachLoop = true;
	RandomRequired->EmitterLoops = 2;
	std::unique_ptr<FParticleEmitterInstance> RandomInstance{ RandomSystem->Emitters[0]->CreateInstance(RandomOwner) };
	const float HalfFirstLoop = RandomInstance->EmitterDuration * 0.5f;
	RandomInstance->Tick(HalfFirstLoop, false);
	RandomInstance->Tick(HalfFirstLoop + 0.0001f, false);
	RandomInstance->Tick(RandomInstance->EmitterDuration - RandomInstance->EmitterTime + 0.0001f, false);
	Require(RandomInstance->LoopCount == 2 && RandomInstance->HasFinishedSpawning(), "randomized durations reach finite loop cap");
	const int32 ParticlesAtCap = RandomInstance->ActiveParticles;
	RandomInstance->Tick(0.01f, false);
	Require(RandomInstance->ActiveParticles == ParticlesAtCap, "randomized durations must not emit after finite loop cap");
}

void TestModulePayload()
{
	FTestEmitterOwner Owner;
	std::unique_ptr<UParticleSystem> System{ MakeParticleTestSystem(4.0f, 10.0f) };
	UParticleLODLevel* LOD = System->Emitters[0]->GetLODLevel(0);
	auto* Module = LOD->AddModule<UTestPayloadModule>();
	System->UpdateAllModuleLists();
	std::unique_ptr<FParticleEmitterInstance> Instance{ System->Emitters[0]->CreateInstance(Owner) };
	Require(*reinterpret_cast<int32*>(Instance->GetModuleInstanceData(Module)) == 7, "per-instance payload initialized");
	Instance->Tick(0.25f, false);
	const uint32 Offset = Instance->GetModuleDataOffset(Module);
	Require(Offset >= sizeof(FBaseParticle) && Instance->ParticleStride % 16 == 0, "module payload follows aligned particle");
	float* Value = reinterpret_cast<float*>(Instance->ParticleData + Instance->ParticleStride * Instance->ParticleIndices[0] + Offset);
	Require(NearlyEqual(*Value, 7.0f), "spawn receives correct module payload offset");
	Instance->Tick(0.25f, true);
	Require(NearlyEqual(*Value, 7.25f), "update receives same module payload");
	Require(*reinterpret_cast<int32*>(Instance->GetModuleInstanceData(Module)) == 9, "final-update executes after simulation");
}
}

void RunParticleSimulationTests()
{
	TestDistributionsAndCurves();
	TestParticleAllocationAndCap();
	TestModulesAndReplay();
	TestBurstsAndCompletion();
	TestModulePayload();
}
