#include "ParticleTestSupport.h"
#include "Asset/AssetManager.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "GameFramework/Actor/ParticleSystemActor.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleSpriteInstanceData.h"
#include "Render/Buffer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderPacket.h"
#include "Render/Shader.h"
#include <cstring>
#include <cwchar>
#include <chrono>

void RunParticleSimulationTests();
void TestParticleLoader();

namespace
{
void TestActorAndLifecycle()
{
	std::unique_ptr<AParticleSystemActor> Actor{ NewObject<AParticleSystemActor>() };
	Require(Actor->GetComponents().Num() == 1, "PSC actor must create exactly one component");
	Require(Actor->GetRootComponent() == Actor->GetParticleSystemComponent(), "PSC must be the actor root");
	Require(Actor->GetRootComponent()->IsA(UParticleSystemComponent::StaticClass()), "actor root must be the unified PSC");

	UParticleSystem* System = MakeParticleTestSystem();
	FParticleTestFixture Fixture(System, false);
	auto& PSC = *Fixture.Component;
	Require(!PSC.IsActive() && PSC.GetEmitterInstances().IsEmpty(), "bAutoActivate=false must remain stopped on registration");
	Require(PSC.PrimaryComponentTick.IsTickFunctionRegistered(), "preview registration must register tick");
	Fixture.Tick(0.2f);
	Require(PSC.GetNumActiveParticles() == 0, "inactive PSC must not emit");
	PSC.Activate();
	Require(PSC.IsActive() && PSC.GetEmitterInstances().Num() == 1, "Activate must initialize an emitter");
	Fixture.Tick(0.2f);
	Require(PSC.GetNumActiveParticles() == 2, "active PSC must spawn at its template rate");
	PSC.Deactivate();
	const int32 BeforeDrain = PSC.GetNumActiveParticles();
	Fixture.Tick(0.2f);
	Require(!PSC.IsActive() && PSC.GetNumActiveParticles() == BeforeDrain, "Deactivate must stop spawning while particles drain");
	Fixture.Tick(11.0f);
	Require(PSC.GetNumActiveParticles() == 0 && PSC.HasCompleted(), "deactivated PSC must finish its live particles");
	PSC.Activate(true);
	Fixture.Tick(0.2f);
	Require(PSC.GetNumActiveParticles() == 2, "completed PSC must reactivate");
	PSC.ResetParticles();
	Require(PSC.GetNumActiveParticles() == 0, "ResetParticles must clear live particles");
	Fixture.Tick(0.2f);
	Require(PSC.GetNumActiveParticles() == 2, "reset active PSC must continue emitting");
	PSC.UnregisterComponent();
	Require(!PSC.PrimaryComponentTick.IsTickFunctionRegistered() && PSC.GetEmitterInstances().IsEmpty(), "unregister must release emitters and tick");
	PSC.RegisterComponentWithWorld(Fixture.World.get());
	Fixture.Tick(0.2f);
	Require(PSC.GetNumActiveParticles() == 2, "active PSC must resume after re-registration");
	PSC.SetTemplate(MakeParticleTestSystem(20.0f));
	Require(PSC.GetNumActiveParticles() == 0, "template replacement must clear old particles");
	Fixture.Tick(0.2f);
	Require(PSC.GetNumActiveParticles() == 4, "template replacement must use the new spawn rate");
	PSC.Template = MakeParticleTestSystem(5.0f);
	FProperty TemplateProperty{};
	TemplateProperty.Name = "Template";
	PSC.PostEditChangeProperty(TemplateProperty);
	Fixture.Tick(0.2f);
	Require(PSC.GetNumActiveParticles() == 1, "property assignment must reset an already-assigned template pointer");
	PSC.SetTemplate(nullptr);
	Require(!PSC.IsActive() && PSC.GetEmitterInstances().IsEmpty(), "null template must stop and clear the component");
	std::unique_ptr<FParticleDynamicData> Empty{ PSC.CreateDynamicData() };
	Require(Empty->DynamicEmitterDataArray.IsEmpty(), "cleared template must not leave a render snapshot");
}

void TestScaleBoundsAndSnapshot()
{
	FParticleTestFixture Fixture(MakeParticleTestSystem(10.0f, 10.0f, true));
	auto& PSC = *Fixture.Component;
	Require(PSC.IsActive(), "auto activation must initialize on registration");
	PSC.SetWorldLocation(FVector(20, 30, 40), false);
	PSC.SetWorldScale3D(FVector(4, 2, 3));
	Fixture.Tick(0.1f);
	Require(PSC.GetNumActiveParticles() == 1, "bounds test needs one live particle");
	FParticleEmitterInstance* Instance = PSC.GetEmitterInstances()[0];
	FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(Instance->ParticleData + static_cast<uint64>(Instance->ParticleStride) * Instance->ParticleIndices[0]);
	Particle.Location = FVector(1, 2, 3);
	Particle.Size = FVector(2, 1, 1);
	PSC.OnTransformDirty();
	const FVector ExpectedCenter = PSC.GetWorldMatrix().TransformPosition(Particle.Location);
	const FBox WorldBounds = PSC.CalcLocalBounds().GetWorldAABB(PSC.GetWorldMatrix());
	const FVector ActualCenter = (WorldBounds.Min + WorldBounds.Max) * 0.5f;
	Require((ActualCenter - ExpectedCenter).SizeSquared() < 0.001f, "local particle center must be transformed exactly once");
	Require(WorldBounds.Max.X - ExpectedCenter.X > 5.6f, "square sprite bounds must include both scaled dimensions");
	float HitDistance = FLT_MAX;
	Require(PSC.LineTraceComponentLocal(FRay{ FVector(-100, 2, 3), FVector(1, 0, 0) }, HitDistance), "particle bounds must remain selectable");
	std::unique_ptr<FParticleDynamicData> Snapshot{ PSC.CreateDynamicData() };
	Require(Snapshot->DynamicEmitterDataArray.Num() == 1, "live emitter must produce a snapshot");
	const auto& Source = static_cast<FDynamicSpriteEmitterData*>(Snapshot->DynamicEmitterDataArray[0])->Source;
	Require(Source.Scale == FVector(4, 2, 3), "snapshot must retain world scale");
	PSC.KillParticlesForced();
	Require(Source.ActiveParticleCount == 1, "render snapshot must own an independent particle copy");
	PSC.SetWorldScale3D(FVector(0, 1, 1));
	std::unique_ptr<FParticleDynamicData> ZeroScale{ PSC.CreateDynamicData() };
	Require(ZeroScale->DynamicEmitterDataArray.IsEmpty(), "zero scale must not emit invalid sprite data");
	const FBox ZeroBounds = PSC.CalcLocalBounds();
	Require(ZeroBounds.Min == FVector::ZeroVector && ZeroBounds.Max == FVector::ZeroVector, "zero scale must avoid singular inverse bounds");
}

void TestDefaultPresets()
{
	UAssetManager::Get().CreateDefaultParticleSystem();
	UParticleSystem* Minsu = UAssetManager::GetAssetByPath<UParticleSystem>("DefaultParticleSystem");
	UParticleSystem* CY = UAssetManager::GetAssetByPath<UParticleSystem>("CYParticleSystem");
	Require(Minsu && CY && Minsu != CY, "both branch presets must remain available");
	const UParticleModuleRequired* MinsuRequired = Minsu->GetEmitters()[0]->GetLODLevel(0)->RequiredModule;
	const UParticleModuleRequired* CYRequired = CY->GetEmitters()[0]->GetLODLevel(0)->RequiredModule;
	Require(MinsuRequired->bUseLocalSpace && MinsuRequired->ScreenAlignment == PSA_Rectangle,
		"Minsu preset must retain component-relative rectangular sprites");
	Require(!CYRequired->bUseLocalSpace && CYRequired->SubImages_Horizontal == 8 && CYRequired->SubImages_Vertical == 8,
		"CY preset must retain world-space atlas sprites");
}

FShaderByteCode CompileParticleShader(const wchar_t* Path, const char* Entry, const char* Target)
{
	FShaderByteCode Result;
	ComPtr<ID3DBlob> Errors;
	const HRESULT Status = D3DCompileFromFile(Path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, Entry, Target,
		D3DCOMPILE_ENABLE_STRICTNESS, 0, Result.Blob.GetAddressOf(), Errors.GetAddressOf());
	if (FAILED(Status))
	{
		if (Errors) { std::cerr << static_cast<const char*>(Errors->GetBufferPointer()) << '\n'; }
		throw std::runtime_error("particle shader compilation failed");
	}
	return Result;
}

void TestShaders(FRenderDevice& Device)
{
	static_assert(sizeof(FParticleSpriteVertex) == 64);
	static_assert(offsetof(FParticleSpriteVertex, Color) == 48);
	for (const wchar_t* Path : { L"Resources/Shader/ParticleSpriteShader.hlsl", L"Resources/Shader/ParticleSpriteStructuredShader.hlsl" })
	{
		const FShaderByteCode VertexCode = CompileParticleShader(Path, "mainVS", "vs_5_0");
		const FShaderByteCode PixelCode = CompileParticleShader(Path, "mainPS", "ps_5_0");
		FVertexShader Vertex(Device.GetDevice(), VertexCode);
		FPixelShader Pixel(Device.GetDevice(), PixelCode);
		Require(Vertex.IsValid() && Vertex.GetLayout() && Pixel.IsValid(), "production shader reflection must create valid input layouts");
		ComPtr<ID3D11ShaderReflection> Reflection;
		Require(SUCCEEDED(D3DReflect(VertexCode.GetData(), VertexCode.GetSize(), IID_PPV_ARGS(Reflection.GetAddressOf()))), "shader reflection failed");
		D3D11_SHADER_INPUT_BIND_DESC Binding{};
		if (std::wcsstr(Path, L"Structured"))
		{
			Require(SUCCEEDED(Reflection->GetResourceBindingDescByName("ParticleInstances", &Binding)) && Binding.BindPoint == 22,
				"Minsu structured instances must retain SRV slot 22");
		}
		else
		{
			D3D11_SHADER_BUFFER_DESC Params{};
			Require(SUCCEEDED(Reflection->GetConstantBufferByName("ParticleSpriteParams")->GetDesc(&Params)) && Params.Size == sizeof(FParticleSpriteUniformParameters),
				"instance shader parameters must match the CPU layout");
		}
	}
}

class FTestParticleProxy : public FParticleSystemSceneProxy
{
public:
	using FParticleSystemSceneProxy::FParticleSystemSceneProxy;
	int32 FirstSorted(const FDynamicSpriteEmitterReplayDataBase& Source) const
	{
		SortSpriteParticles(Source, Source.ActiveParticleCount, FVector::ZeroVector);
		return ParticleOrder[0].ParticleIndex;
	}
};

FParticleSpriteVertex ReadFirstInstance(FRenderDevice& Device, const FRenderPacket& Packet)
{
	D3D11_BUFFER_DESC Description{};
	Packet.VertexFactory->InstanceBuffer->GetBuffer()->GetDesc(&Description);
	Description.Usage = D3D11_USAGE_STAGING;
	Description.BindFlags = 0;
	Description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Description.MiscFlags = 0;
	ComPtr<ID3D11Buffer> Staging;
	Require(SUCCEEDED(Device.GetDevice()->CreateBuffer(&Description, nullptr, Staging.GetAddressOf())), "readback staging creation failed");
	Device.GetContext()->CopyResource(Staging.Get(), Packet.VertexFactory->InstanceBuffer->GetBuffer());
	D3D11_MAPPED_SUBRESOURCE Mapped{};
	Require(SUCCEEDED(Device.GetContext()->Map(Staging.Get(), 0, D3D11_MAP_READ, 0, &Mapped)), "GPU instance readback failed");
	FParticleSpriteVertex Result;
	std::memcpy(&Result, Mapped.pData, sizeof(Result));
	Device.GetContext()->Unmap(Staging.Get(), 0);
	return Result;
}

template<typename TDraw>
uint64 QueryDrawPrimitives(FRenderDevice& Device, TDraw Draw)
{
	D3D11_QUERY_DESC Description{ D3D11_QUERY_PIPELINE_STATISTICS, 0 };
	ComPtr<ID3D11Query> Query;
	Require(SUCCEEDED(Device.GetDevice()->CreateQuery(&Description, Query.GetAddressOf())), "pipeline statistics query creation failed");
	Device.GetContext()->Begin(Query.Get());
	Draw();
	Device.GetContext()->End(Query.Get());
	Device.GetContext()->Flush();
	D3D11_QUERY_DATA_PIPELINE_STATISTICS Statistics{};
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	HRESULT Status;
	while ((Status = Device.GetContext()->GetData(Query.Get(), &Statistics, sizeof(Statistics), 0)) == S_FALSE)
	{
		Require(std::chrono::steady_clock::now() < Deadline, "GPU statistics query timed out");
		Sleep(1);
	}
	Require(SUCCEEDED(Status), "GPU statistics query failed");
	return Statistics.IAPrimitives;
}

void TestDrawRoutes(FRenderDevice& Device, const FRenderPacket& Packet)
{
	struct FTestViewConstants
	{
		FMatrix ViewProjection = FMatrix::Identity;
		FVector CameraWorldPos = FVector(-10, 0, 0);
		uint32 Padding = 0;
		FVector CameraForward = FVector(1, 0, 0);
		uint32 Cascades = 0;
	};
	FTestViewConstants View;
	FMatrix World = FMatrix::Identity;
	auto ViewBuffer = Device.CreateConstantBuffer(sizeof(View));
	auto WorldBuffer = Device.CreateConstantBuffer(sizeof(World));
	auto ParamsBuffer = Device.CreateConstantBuffer(sizeof(FParticleSpriteUniformParameters));
	RenderCommand::UpdateBufferData(ViewBuffer.get(), &View);
	RenderCommand::UpdateBufferData(WorldBuffer.get(), &World);
	RenderCommand::UpdateBufferData(ParamsBuffer.get(), Packet.MaterialParamData, sizeof(FParticleSpriteUniformParameters));
	RenderCommand::BindConstantBuffer(0, ViewBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(1, ParamsBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(2, WorldBuffer.get(), EShaderBindFlagBits::Vertex);
	Device.GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const FShaderByteCode VertexCode = CompileParticleShader(L"Resources/Shader/ParticleSpriteShader.hlsl", "mainVS", "vs_5_0");
	const FShaderByteCode PixelCode = CompileParticleShader(L"Resources/Shader/ParticleSpriteShader.hlsl", "mainPS", "ps_5_0");
	FVertexShader Vertex(Device.GetDevice(), VertexCode);
	FPixelShader Pixel(Device.GetDevice(), PixelCode);
	FShaderProgram Program(&Vertex, &Pixel);
	RenderCommand::BindShaderProgram(&Program);
	// No render target or window is needed to measure input-assembler primitives.
	Device.GetContext()->PSSetShader(nullptr, nullptr, 0);
	RenderCommand::BindVertexFactory(*Packet.VertexFactory);
	RenderCommand::BindIndexBuffer(Packet.IndexBuffer);
	const uint64 DrawCount = RenderCommand::GetDrawIndexedCount();
	Require(QueryDrawPrimitives(Device, [] { RenderCommand::DrawIndexedInstanced(6, 1); }) == 2, "one canonical instance must submit two triangles");
	Require(QueryDrawPrimitives(Device, [] { RenderCommand::DrawIndexedInstanced(6, 2); }) == 4, "two canonical instances must submit four triangles");

	FParticleSpriteInstanceData Instances[2];
	Instances[0].Center = FVector(0, 0, 0);
	Instances[1].Center = FVector(0, 1, 0);
	auto Structured = Device.CreateDynamicStructuredBuffer(sizeof(FParticleSpriteInstanceData), 2);
	Require(Structured && Structured->GetSRV(), "structured instance buffer creation failed");
	RenderCommand::UpdateBufferData(Structured.get(), Instances, sizeof(Instances));
	struct FStructuredQuadVertex { FVector Position; FVector2 UV; };
	const FStructuredQuadVertex Quad[] = {
		{ FVector(0, -0.5f, 0.5f), FVector2(0, 0) }, { FVector(0, 0.5f, 0.5f), FVector2(1, 0) },
		{ FVector(0, 0.5f, -0.5f), FVector2(1, 1) }, { FVector(0, -0.5f, -0.5f), FVector2(0, 1) }
	};
	auto QuadBuffer = Device.CreateStaticVertexBuffer(Quad, sizeof(Quad), sizeof(FStructuredQuadVertex));
	FVertexFactory StructuredFactory;
	StructuredFactory.VertexBuffer = QuadBuffer.get();
	StructuredFactory.InstanceData = Structured.get();
	RenderCommand::BindVertexFactory(StructuredFactory);
	ComPtr<ID3D11ShaderResourceView> BoundSRV;
	Device.GetContext()->VSGetShaderResources(22, 1, BoundSRV.GetAddressOf());
	Require(BoundSRV.Get() == Structured->GetSRV(), "generic structured instance path must bind SRV slot 22");
	ComPtr<ID3D11Buffer> BoundInstanceStream;
	UINT Stride = 0, Offset = 0;
	Device.GetContext()->IAGetVertexBuffers(1, 1, BoundInstanceStream.GetAddressOf(), &Stride, &Offset);
	Require(!BoundInstanceStream, "ordinary vertex factory must clear the previous instance stream");
	const FShaderByteCode StructuredVS = CompileParticleShader(L"Resources/Shader/ParticleSpriteStructuredShader.hlsl", "mainVS", "vs_5_0");
	const FShaderByteCode StructuredPS = CompileParticleShader(L"Resources/Shader/ParticleSpriteStructuredShader.hlsl", "mainPS", "ps_5_0");
	FVertexShader StructuredVertex(Device.GetDevice(), StructuredVS);
	FPixelShader StructuredPixel(Device.GetDevice(), StructuredPS);
	FShaderProgram StructuredProgram(&StructuredVertex, &StructuredPixel);
	RenderCommand::BindShaderProgram(&StructuredProgram);
	Device.GetContext()->PSSetShader(nullptr, nullptr, 0);
	Require(QueryDrawPrimitives(Device, [] { RenderCommand::DrawInstance(6, 1); }) == 2, "generic single-instance fallback must submit two triangles");
	Require(QueryDrawPrimitives(Device, [] { RenderCommand::DrawInstance(6, 2); }) == 4, "generic instanced draw must submit four triangles");
	Require(RenderCommand::GetDrawIndexedCount() - DrawCount == 4, "both draw routes must retain draw-call statistics");
}

void TestParticleOrderingAndViewBuffers(FRenderDevice& Device)
{
	UParticleSystem* System = MakeParticleTestSystem(10.0f);
	UParticleModuleRequired* Required = System->Emitters[0]->LODLevels[0]->RequiredModule;
	Required->SortMode = PSORTMODE_DistanceToView;
	Required->Material = NewObject<UMaterial>();
	Required->Material->ParamLayout = EMaterialParamLayout::ParticleSprite;
	UAssetManager::Get().RegisterAsset("ParticleSpriteMaterial", Required->Material);
	UMaterial* DefaultMaterial = Required->Material;
	FParticleTestFixture Fixture(System);
	Fixture.Tick(0.2f);
	FParticleEmitterInstance* Instance = Fixture.Component->GetEmitterInstances()[0];
	Require(Instance->ActiveParticles == 2, "ordering test needs two live particles");
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(Instance->ParticleData + static_cast<uint64>(Instance->ParticleStride) * Instance->ParticleIndices[Index]);
		Particle.Location = FVector(Index == 0 ? 1.0f : 9.0f, 0, 0);
		Particle.Flags = Index == 0 ? 1 : 2;
	}
	FTestParticleProxy Proxy(Fixture.Component.get());
	Proxy.UpdateTransform();
	std::unique_ptr<FParticleDynamicData> Snapshot{ Fixture.Component->CreateDynamicData() };
	auto& Source = static_cast<FDynamicSpriteEmitterData*>(Snapshot->DynamicEmitterDataArray[0])->Source;
	Source.SortMode = PSORTMODE_Age_OldestFirst;
	Require(Proxy.FirstSorted(Source) == 0, "oldest-first must sort smaller creation counters first");
	Source.SortMode = PSORTMODE_Age_NewestFirst;
	Require(Proxy.FirstSorted(Source) == 1, "newest-first must sort larger creation counters first");
	Proxy.PrepareDynamicData();
	FRenderQueue QueueA;
	FRenderQueue QueueB;
	TArray<int32> Indices;
	TArray<float> Distances;
	Proxy.GatherDynamicRenderPackets(QueueA, nullptr, FVector::ZeroVector, nullptr, nullptr, Indices, Distances);
	Proxy.GatherDynamicRenderPackets(QueueB, nullptr, FVector(10, 0, 0), nullptr, nullptr, Indices, Distances);
	Require(QueueA.GetPackets().Num() == 1 && QueueB.GetPackets().Num() == 1, "each view must submit one instanced emitter packet");
	const FRenderPacket& A = QueueA.GetPackets()[0];
	const FRenderPacket& B = QueueB.GetPackets()[0];
	Require(A.InstanceCount == 2 && A.IndexCount == 6, "sprite packet must instance one quad per particle");
	Require(A.VertexFactory->InstanceBuffer != B.VertexFactory->InstanceBuffer, "deferred views must retain separate sorted buffers");
	Require(NearlyEqual(ReadFirstInstance(Device, A).Position.X, 9.0f), "view B must not overwrite view A far-first particle order");
	Require(NearlyEqual(ReadFirstInstance(Device, B).Position.X, 1.0f), "view B must upload its own far-first particle order");
	TestDrawRoutes(Device, A);
	Required->Material = nullptr;
	Fixture.Tick(0.01f);
	Proxy.MarkRenderDynamicDataDirty();
	Proxy.InvalidateDynamicData();
	Proxy.PrepareDynamicData();
	FRenderQueue FallbackQueue;
	Proxy.GatherDynamicRenderPackets(FallbackQueue, nullptr, FVector::ZeroVector, nullptr, nullptr, Indices, Distances);
	Require(FallbackQueue.GetPackets().Num() == 1 && FallbackQueue.GetPackets()[0].Material == DefaultMaterial,
		"missing emitter material must retain the Minsu default sprite material fallback");
}

void TestGameActorReregistration()
{
	std::unique_ptr<UWorld> World{ NewObject<UWorld>() };
	World->SetWorldType(EWorldType::Game);
	Require(World->Init(), "game world initialization failed");
	AParticleSystemActor* Actor = World->SpawnActor<AParticleSystemActor>();
	UParticleSystemComponent* PSC = Actor->GetParticleSystemComponent();
	PSC->SetTemplate(MakeParticleTestSystem());
	Actor->BeginPlay();
	World->GetTickTaskManager().RunAllTickGroups(0.2f);
	Require(PSC->GetNumActiveParticles() == 2, "game actor must tick after BeginPlay");
	PSC->UnregisterComponent();
	Require(!PSC->PrimaryComponentTick.IsTickFunctionRegistered(), "game unregister must detach tick");
	PSC->RegisterComponentWithWorld(World.get());
	Require(PSC->PrimaryComponentTick.IsTickFunctionRegistered(), "game re-registration after BeginPlay must restore tick");
	World->GetTickTaskManager().RunAllTickGroups(0.2f);
	Require(PSC->GetNumActiveParticles() == 2, "re-registered game actor must resume simulation");
}
}

int main()
{
	std::cerr << "RUN: actor and lifecycle\n";
	try
	{
		TestActorAndLifecycle();
		std::cerr << "RUN: bounds and snapshots\n";
		TestScaleBoundsAndSnapshot();
		std::cerr << "RUN: emitter simulation\n";
		RunParticleSimulationTests();
		std::cerr << "RUN: loader roundtrip\n";
		TestParticleLoader();
		TestDefaultPresets();
		std::cerr << "RUN: D3D device\n";
		ComPtr<ID3D11Device> ProbeDevice;
		ComPtr<ID3D11DeviceContext> ProbeContext;
		Require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG,
			nullptr, 0, D3D11_SDK_VERSION, ProbeDevice.GetAddressOf(), nullptr, ProbeContext.GetAddressOf())),
			"D3D11 hardware device with SDK debug layers is required for GPU integration tests");
		ProbeContext.Reset();
		ProbeDevice.Reset();
		FRenderDevice Device;
		RenderCommand::Init(&Device);
		std::cerr << "RUN: shader compile and reflection\n";
		TestShaders(Device);
		std::cerr << "RUN: ordering, per-view buffers and GPU draws\n";
		TestParticleOrderingAndViewBuffers(Device);
		std::cerr << "RUN: game actor re-registration\n";
		TestGameActorReregistration();
		Device.GetContext()->ClearState();
		RenderCommand::Init(nullptr);
		std::cout << "PASS: particle actor, lifecycle, bounds, simulation, loader, shaders, sorting and per-view GPU buffers\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << "FAIL: " << Error.what() << '\n';
		return 1;
	}
}
