#include "EnginePCH.h"
#include "AssetManager.h"
#include "Render/Buffer.h"
#include "Render/Material.h"

#include "Render/Vertex.h"
#include "Render/Texture2D.h"
#include "Render/RenderCommand.h"

#include "Render/RenderResourceManager.h"

#include "Asset/ObjImporter/ObjImporter.h"
#include "Asset/FbxImporter/FbxAssetLoader.h"
#include "Particles/ParticleSystemLoader.h"

#include "Render/GeometryGenerator.h"
#include "Render/Mesh.h"
#include "Render/SkeletalMesh.h"
#include "Render/SkeletalMeshGenerator.h"
#include "UObject/UObjectGlobals.h"

#include "Render/ImageLoader.h"
#include "Core/ResourcePaths.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Color/ParticleModuleColor.h"
#include <algorithm>
#include <cctype>

namespace
{
FString GetAssetExtension(const FString& Path)
{
	const std::u8string UTF8Path{ Path.begin(), Path.end() };
	FString Extension{ std::filesystem::path{ UTF8Path }.extension().string() };
	for (char& Character : Extension)
	{
		Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
	}
	return Extension;
}

FString ToUTF8String(const std::filesystem::path& Path)
{
	const std::u8string UTF8Path = Path.generic_u8string();
	return FString{ UTF8Path.begin(), UTF8Path.end() };
}

UStaticMesh* CreateStaticMesh(const FStaticMeshData* MeshData)
{
	if (!MeshData || !MeshData->Vertices.Num() || !MeshData->Indices.Num())
	{
		return nullptr;
	}

	FStaticMeshData Optimized = *MeshData;

	// Mesh에 MaterialSlot 없을 경우 Default Material 할당
	if (Optimized.MaterialSlots.IsEmpty())
	{
		FStaticMaterialSlot DefaultSlot;
		DefaultSlot.Name = "Default";
		Optimized.MaterialSlots.Add(DefaultSlot);
	}

	// Mesh에 Section이 없을 경우 전체 메쉬을 섹션 하나로 세팅
	if (Optimized.Sections.IsEmpty())
	{
		FStaticMeshSection DefaultSection;
		DefaultSection.StartIndex = 0;
		DefaultSection.IndexCount = static_cast<uint32>(Optimized.Indices.Num());
		DefaultSection.MaterialSlotIndex = 0;

		Optimized.Sections.Add(DefaultSection);
	}

	Optimized.BuildTangentSpace();
	Optimized.OptimizeTriangleOrderForVertexCache();

	TUniquePtr<FVertexBuffer> VB = RenderCommand::CreateStaticVertexBuffer(Optimized.Vertices.GetData(), sizeof(FVertex) * static_cast<uint32>(Optimized.Vertices.size()), sizeof(FVertex));
	TUniquePtr<FIndexBuffer> IB = RenderCommand::CreateStaticIndexBuffer(Optimized.Indices.GetData(), static_cast<uint32>(Optimized.Indices.size()));
	if (VB == nullptr || IB == nullptr)
	{
		return nullptr;
	}

	UStaticMesh* Mesh = NewObject<UStaticMesh>();
	Mesh->MeshData = std::move(Optimized);

	// Vertex/Index GPU 업로드
	Mesh->VertexBuffer = std::move(VB);
	Mesh->IndexBuffer = std::move(IB);

	UMaterial* DefaultMaterial = UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
	if (!DefaultMaterial)
	{
		return nullptr;
	}

	// MaterialSlots를 실제 UMaterial로 변환
	for (const FStaticMaterialSlot& Slot : Mesh->MeshData.MaterialSlots)
	{
		UMaterial* Material = UMaterial::CreateInstance(DefaultMaterial);
		if (!Material)
		{
			return nullptr;
		}
		Material->DiffuseOrFactor = Slot.BaseColor;
		Material->SpecularColor = Slot.SpecularColor;
		Material->Shininess = Slot.Shininess;
		Material->SetBlendState(Material->DiffuseOrFactor.W < 1.0f ? EBlendState::AlphaBlend : EBlendState::Opaque);

		if (!Slot.DiffuseTexturePath.empty())
		{
			UTexture2D* Texture = UAssetManager::Get().LoadTexture(Slot.DiffuseTexturePath);

			if (Texture)
			{
				Material->Textures[0] = Texture;
			}
		}

		if (!Slot.NormalTexturePath.empty())
		{
			if (UTexture2D * NormalTexture{ UAssetManager::Get().LoadTexture(Slot.NormalTexturePath) })
			{
				Material->Textures[1] = NormalTexture;
			}
		}

		Mesh->Materials.Add(Material);
	}

	Mesh->MeshData.BuildTriangleBVH();
	Mesh->RebuildRenderData();

	return Mesh;
}

FString MakeAssetKey(const FString& Path)
{
	return fs::relative(Path, "Assets").generic_string();
}
}

UAssetManager& UAssetManager::Get()
{
	static UAssetManager* Instance = NewObject<UAssetManager>();
	return *Instance;
}

void UAssetManager::Init(const FAssetLoadProgress& OnProgress)
{
	FGeometryGenerator::CreateDefaultMeshDatas();
	// 머티리얼이 참조하므로 반드시 먼저 만든다
	Get().CreateDefaultTextures();
	Get().CreateDefaultMaterial();
	// .particle 파일의 머티리얼 참조를 읽기 전에 기본 스프라이트 머티리얼을 등록한다.
	Get().CreateParticleSpriteMaterial();
	Get().CreateParticleBeamMaterial();
	Get().CreateDefaultParticleSystem();
	// Resources는 스캔하지 않으므로 엔진이 쓰는 기본 폰트는 경로로 직접 등록한다.
	Get().LoadFontAtlas(ResourcePaths::DefaultFont, ResourcePaths::DefaultFontAtlas);
	Get().CreateDefaultMeshes();
	Get().ScanAssets("Assets", OnProgress);
	Get().CreateParticleMaterial();
	Get().CreateEditorArrowMaterial();
}

void UAssetManager::ScanAssets(const fs::path& AssetRoot, const FAssetLoadProgress& OnProgress)
{
	if (!fs::exists(AssetRoot))
	{
		HTR_LOG(Error, "Asset root not found: {}", AssetRoot.generic_string());
		return;
	}

	TArray<fs::path> Files;
	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(AssetRoot))
	{
		if (Entry.is_regular_file())
		{
			Files.Add(Entry.path());
		}
	}

	// .particle의 Mesh 참조를 역직렬화하기 전에 디스크의 메시도 등록한다.
	std::stable_partition(Files.begin(), Files.end(), [](const fs::path& Path)
	{
		return GetAssetExtension(ToUTF8String(Path)) != ".particle";
	});

	const int32 Total = Files.Num();
	for (int32 i = 0; i < Total; ++i)
	{
		FString Key = ToUTF8String(fs::relative(Files[i], AssetRoot));
		FString Path = ToUTF8String(Files[i]);
		AssetPathMap.Add(Key, Path);
		LoadAsset(Key, Path);

		if (OnProgress)
		{
			OnProgress(i + 1, Total, Key);
		}
	}
}

void UAssetManager::LoadAsset(const FString& Key, const FString& Path)
{
	const FString Extension{ GetAssetExtension(Path) };
	// 원본 FBX는 시작 시 파싱하지 않는다. 명시적 에디터 임포트 결과만 읽는다.
	if (Extension == ".fbxbin")
	{
		FString Error;
		if (!FFbxAssetLoader::LoadAsset(Path, Error)) HTR_LOG(Error, "FBX asset load failed: {}", Error);
		return;
	}
	if (Extension == ".png" || Extension == ".dds")
	{
		if (UTexture2D * Texture{ LoadTexture(Path) })
		{
			AssetMap[Key] = Texture;
		}
	}
	if (Extension == ".json")
	{
		const FString TexturePath{ fs::path{ Path }.replace_extension(".png").string() };
		if (UFont * Font{ LoadFontAtlas(Path, TexturePath) })
		{
			AssetMap[Key] = Font;
		}
	}
	if (Extension == ".obj")
	{
		LoadObjStaticMesh(Path);
	}
	if (Extension == ".particle")
	{
		FString Error;
		if (!FParticleSystemLoader::LoadAsset(Key, Path, Error)) HTR_LOG(Error, "Particle asset load failed: {}", Error);
		return;
	}
}

void UAssetManager::CreateDefaultTextures()
{
	// 텍스처가 지정되지 않은 머티리얼이 검게 나오지 않도록 하는 1x1 흰색 텍스처.
	// 셰이더에서 곱해도 결과가 변하지 않으므로 "텍스처 없음"의 기본값으로 쓴다.
	const uint32 WhitePixel{ 0xFFFFFFFF };

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = 1;
	Desc.Height = 1;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TUniquePtr<FTexture2D> Resource{ RenderCommand::CreateTexture2D(Desc, &WhitePixel) };
	if (!Resource)
	{
		return;
	}

	UTexture2D* WhiteTexture{ NewObject<UTexture2D>() };
	WhiteTexture->SetResource(std::move(Resource));
	WhiteTexture->SetPath("WhiteTexture");

	AssetMap["WhiteTexture"] = WhiteTexture;

	const FVector4 NormalPixel{ 0.5f, 0.5f, 1.0f, 1.0f };
	Desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	TUniquePtr<FTexture2D> NormalResource{ RenderCommand::CreateTexture2D(Desc, &NormalPixel) };
	if (!NormalResource)
	{
		return;
	}

	UTexture2D* FlatNormalTexture{ NewObject<UTexture2D>() };
	FlatNormalTexture->SetResource(std::move(NormalResource));
	FlatNormalTexture->SetPath("FlatNormalTexture");
	AssetMap["FlatNormalTexture"] = FlatNormalTexture;
}

void UAssetManager::CreateDefaultMeshes()
{
	// 메시 데이터 업로드
	FStaticMeshData* CubeData = FGeometryGenerator::GetMeshData("Cube");
	RegisterAsset("Cube", CreateStaticMesh(CubeData));

	FStaticMeshData* ConeData = FGeometryGenerator::GetMeshData("Cone");
	RegisterAsset("Cone", CreateStaticMesh(ConeData));

	FStaticMeshData* SphereData = FGeometryGenerator::GetMeshData("Sphere");
	RegisterAsset("Sphere", CreateStaticMesh(SphereData));

	FStaticMeshData* CylinderData = FGeometryGenerator::GetMeshData("Cylinder");
	RegisterAsset("Cylinder", CreateStaticMesh(CylinderData));

	FStaticMeshData* PlaneData = FGeometryGenerator::GetMeshData("Plane");
	RegisterAsset("Plane", CreateStaticMesh(PlaneData));

	// 아래는 기즈모·라이트 화살표 같은 엔진 내부용 메시라 에디터 메시 선택 목록에서 숨긴다.
	const auto RegisterInternalMesh = [this](const char* Name)
	{
		UStaticMesh* Mesh = CreateStaticMesh(FGeometryGenerator::GetMeshData(Name));
		if (Mesh)
		{
			Mesh->bInternalOnly = true;
		}
		RegisterAsset(Name, Mesh);
	};
	RegisterInternalMesh("Arrow");
	RegisterInternalMesh("Ring");
	RegisterInternalMesh("ScaleBar");
	RegisterInternalMesh("GizmoSphere");

	const FParticleVertex Vertices[] = {
		{ FVector(0.0f, -0.5f, 0.5f), FVector2(0.0f, 0.0f) },
		{ FVector(0.0f, 0.5f, 0.5f), FVector2(1.0f, 0.0f) },
		{ FVector(0.0f, 0.5f, -0.5f), FVector2(1.0f, 1.0f) },
		{ FVector(0.0f, -0.5f, -0.5f), FVector2(0.0f, 1.0f) }
	};

	const uint32 Indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	UStaticMesh* Mesh = NewObject<UStaticMesh>();
	Mesh->VertexBuffer = RenderCommand::CreateStaticVertexBuffer(
		Vertices,
		sizeof(Vertices), sizeof(FParticleVertex));
	Mesh->IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
		Indices,
		ARRAYSIZE(Indices));

	FVertex Vertex{};
	Vertex.Position = FVector(0.0f, -0.5f, 0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, 0.5f, 0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, 0.5f, -0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, -0.5f, -0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);

	Mesh->MeshData.Indices = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };

	Mesh->MeshData.AABB.Min = FVector(-0.001f, -0.5, -0.5);
	Mesh->MeshData.AABB.Max = FVector(0.001f, 0.5, 0.5);
	Mesh->RebuildRenderData();
	Mesh->bInternalOnly = true; // 빌보드·파티클 전용 쿼드

	RegisterAsset("ParticleQuad", Mesh);

	// 스키닝 테스트용 팔 메시 (본 3개). FBX 없이 스켈레탈 파이프라인을 검증한다.
	{
		USkeletalMesh* TestArm = NewObject<USkeletalMesh>();
		FString Error;
		if (FSkeletalMeshGenerator::BuildTestArm(*TestArm, Error))
		{
			TestArm->InitResources();
			// 확인이 필요하면 주석 해제 (역바인드 행렬까지 찍힌다)
			//FSkeletalMeshGenerator::LogSkinData(*TestArm);
			RegisterAsset("TestArm", TestArm);
		}
		else
		{
			HTR_LOG(Error, "[SkelTest] TestArm Initialize failed: {}", Error);
		}
	}
}

void UAssetManager::CreateDefaultMaterial()
{
	UMaterial* DefaultMat{ NewObject<UMaterial>() };
	DefaultMat->Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/UberLit.hlsl");
	DefaultMat->Textures.Add(GetAssetByPath<UTexture2D>("WhiteTexture"));
	DefaultMat->Textures.Add(GetAssetByPath<UTexture2D>("FlatNormalTexture"));
	DefaultMat->ParamLayout = EMaterialParamLayout::StaticMesh;
	DefaultMat->ParamBuffer = RenderCommand::CreateConstantBuffer(sizeof(FStaticMeshMaterialParams));
	RegisterAsset("DefaultMaterial", DefaultMat);
}

void UAssetManager::CreateParticleMaterial()
{
	UMaterial* ParticleMat = NewObject<UMaterial>();
	ParticleMat->Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ParticleSubUVShader.hlsl");
	ParticleMat->Textures.Add(LoadTexture(ResourcePaths::DefaultParticleTexture));
	ParticleMat->BlendState = EBlendState::AlphaBlend;
	ParticleMat->DepthStencilState = EDepthStencilState::ReadOnly;
	ParticleMat->ParamLayout = EMaterialParamLayout::ParticleSubUV;
	ParticleMat->ParamBuffer = RenderCommand::CreateConstantBuffer(256);
	RegisterAsset("SubUVMaterial", ParticleMat);
}

// CY의 인스턴싱 레이아웃과 Minsu의 흰색·Unlit 기본 표현을 함께 쓴다.
void UAssetManager::CreateParticleSpriteMaterial()
{
	UMaterial* SpriteMat = NewObject<UMaterial>();
	SpriteMat->Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ParticleSpriteShader.hlsl");
	SpriteMat->Textures.Add(GetAssetByPath<UTexture2D>("WhiteTexture"));
	SpriteMat->BlendState = EBlendState::AlphaBlend;
	SpriteMat->DepthStencilState = EDepthStencilState::ReadOnly;
	SpriteMat->SamplerState = ESamplerState::LinearClamp;
	SpriteMat->ShadingModel = ELightingModel::Unlit;
	SpriteMat->ParamLayout = EMaterialParamLayout::ParticleSprite;
	SpriteMat->ParamBuffer = RenderCommand::CreateConstantBuffer(sizeof(FParticleSpriteUniformParameters));
	RegisterAsset("ParticleSpriteMaterial", SpriteMat);

	UMaterial* AdditiveMat = UMaterial::CreateInstance(SpriteMat);
	AdditiveMat->SetBlendState(EBlendState::Additive);
	RegisterAsset("ParticleSpriteAdditiveMaterial", AdditiveMat);

	// CY 브랜치의 아틀라스 예제도 별도 에셋으로 보존한다.
	UMaterial* AtlasMat = NewObject<UMaterial>();
	AtlasMat->Shader = SpriteMat->Shader;
	AtlasMat->Textures.Add(LoadTexture(ResourcePaths::DefaultParticleTexture));
	AtlasMat->BlendState = EBlendState::AlphaBlend;
	AtlasMat->DepthStencilState = EDepthStencilState::ReadOnly;
	AtlasMat->ParamLayout = EMaterialParamLayout::ParticleSprite;
	AtlasMat->ParamBuffer = RenderCommand::CreateConstantBuffer(sizeof(FParticleSpriteUniformParameters));
	RegisterAsset("CYParticleSpriteMaterial", AtlasMat);
}

void UAssetManager::CreateParticleBeamMaterial()
{
	UMaterial* Material = NewObject<UMaterial>();
	Material->Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ParticleBeamShader.hlsl");
	Material->Textures.Add(GetAssetByPath<UTexture2D>("WhiteTexture"));
	Material->BlendState = EBlendState::AlphaBlend;
	Material->DepthStencilState = EDepthStencilState::ReadOnly;
	Material->ShadingModel = ELightingModel::Unlit;
	RegisterAsset("ParticleBeamMaterial", Material);
}

// Minsu 브랜치의 기본 프리셋. 값을 바꿔 파티클 표현을 수정할 수 있다.
void UAssetManager::CreateDefaultParticleSystem()
{
	UParticleSystem* System = NewObject<UParticleSystem>();
	UParticleLODLevel* LOD = System->AddEmitter()->AddLODLevel();
	LOD->RequiredModule->Material = GetAssetByPath<UMaterial>("ParticleSpriteMaterial");
	// Minsu 프리셋은 컴포넌트를 따라 움직이는 로컬 공간의 직사각형 스프라이트다.
	LOD->RequiredModule->bUseLocalSpace = true;
	LOD->RequiredModule->ScreenAlignment = PSA_Rectangle;
	Cast<UDistributionFloatConstant>(LOD->SpawnModule->Rate.Distribution)->SetConstant(12.0f);

	auto* Lifetime = LOD->AddModule<UParticleModuleLifetime>();
	delete Lifetime->Lifetime.Distribution;
	auto* LifetimeValue = NewObject<UDistributionFloatConstant>(Lifetime);
	LifetimeValue->SetConstant(1.0f);
	Lifetime->Lifetime.Distribution = LifetimeValue;

	auto* Velocity = LOD->AddModule<UParticleModuleVelocity>();
	delete Velocity->StartVelocity.Distribution;
	auto* VelocityValue = NewObject<UDistributionVectorConstant>(Velocity);
	VelocityValue->SetConstant(FVector(0.0f, 0.0f, 2.0f));
	Velocity->StartVelocity.Distribution = VelocityValue;

	auto* Size = LOD->AddModule<UParticleModuleSize>();
	delete Size->StartSize.Distribution;
	auto* SizeValue = NewObject<UDistributionVectorConstant>(Size);
	SizeValue->SetConstant(FVector(0.08f, 0.08f, 1.0f));
	Size->StartSize.Distribution = SizeValue;

	auto* SizeMultiplyLife = LOD->AddModule<UParticleModuleSizeMultiplyLife>();
	delete SizeMultiplyLife->LifeMultiplier.Distribution;
	auto* SizeCurve = NewObject<UDistributionVectorConstantCurve>(SizeMultiplyLife);
	SizeCurve->AddPoint(0.0f, FVector(0.35f));
	SizeCurve->AddPoint(0.2f, FVector(1.0f));
	SizeCurve->AddPoint(1.0f, FVector(0.1f));
	SizeMultiplyLife->LifeMultiplier.Distribution = SizeCurve;

	auto* Color = LOD->AddModule<UParticleModuleColorOverLife>();
	delete Color->ColorOverLife.Distribution;
	auto* ColorCurve = NewObject<UDistributionVectorConstantCurve>(Color);
	ColorCurve->AddPoint(0.0f, FVector(1.0f, 0.0f, 0.0f));
	ColorCurve->AddPoint(0.5f, FVector(0.0f, 1.0f, 0.0f));
	ColorCurve->AddPoint(1.0f, FVector(0.0f, 0.0f, 1.0f));
	Color->ColorOverLife.Distribution = ColorCurve;

	delete Color->AlphaOverLife.Distribution;
	auto* AlphaCurve = NewObject<UDistributionFloatConstantCurve>(Color);
	AlphaCurve->AddPoint(0.0f, 0.0f);
	AlphaCurve->AddPoint(0.1f, 1.0f);
	AlphaCurve->AddPoint(0.7f, 1.0f);
	AlphaCurve->AddPoint(1.0f, 0.0f);
	Color->AlphaOverLife.Distribution = AlphaCurve;
	System->UpdateAndBuild();
	RegisterAsset("DefaultParticleSystem", System);

	// CY 브랜치의 범위 분포, 8x8 아틀라스 프리셋.
	UParticleSystem* CYSystem = NewObject<UParticleSystem>();
	UParticleEmitter* CYEmitter = CYSystem->AddEmitter();
	UParticleLODLevel* CYLOD = CYEmitter->AddLODLevel();
	CYLOD->RequiredModule->Material = GetAssetByPath<UMaterial>("CYParticleSpriteMaterial");
	CYLOD->RequiredModule->SubImages_Horizontal = 8;
	CYLOD->RequiredModule->SubImages_Vertical = 8;

	auto* CYLifetime = CYLOD->AddModule<UParticleModuleLifetime>();
	auto* LifetimeRange = Cast<UDistributionFloatUniform>(CYLifetime->Lifetime.Distribution);
	LifetimeRange->Min = 1.0f;
	LifetimeRange->Max = 1.5f;

	auto* CYSize = CYLOD->AddModule<UParticleModuleSize>();
	auto* SizeRange = Cast<UDistributionVectorUniform>(CYSize->StartSize.Distribution);
	SizeRange->Min = FVector(0.2f, 0.2f, 0.2f);
	SizeRange->Max = FVector(0.4f, 0.4f, 0.4f);

	auto* CYVelocity = CYLOD->AddModule<UParticleModuleVelocity>();
	auto* VelocityRange = Cast<UDistributionVectorUniform>(CYVelocity->StartVelocity.Distribution);
	VelocityRange->Min = FVector(-0.3f, -0.3f, 1.0f);
	VelocityRange->Max = FVector(0.3f, 0.3f, 2.0f);
	CYLOD->AddModule<UParticleModuleColorOverLife>();
	CYEmitter->EmitterName = "Default";
	CYSystem->UpdateAndBuild();
	RegisterAsset("CYParticleSystem", CYSystem);
}

void UAssetManager::CreateEditorArrowMaterial()
{
	UMaterial* BaseMaterial{ GetAssetByPath<UMaterial>("DefaultMaterial") };
	UMaterial* ArrowMaterial{ UMaterial::CreateInstance(BaseMaterial) };
	ArrowMaterial->Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/EditorArrowShader.hlsl");
	ArrowMaterial->DiffuseOrFactor = FVector4{ 1.0f, 1.0f, 1.0f, 1.0f };
	RegisterAsset("EditorArrowMaterial", ArrowMaterial);
}

void UAssetManager::Shutdown()
{
	Get().AssetMap.Empty();
}


bool UAssetManager::FindAssetFilePath(const FString& Key, FString& OutPath) const
{
	const FString* Found = AssetPathMap.Find(Key);
	if (!Found)
	{
		return false;
	}
	OutPath = *Found;
	return true;
}

void UAssetManager::RegisterAsset(const FString& Key, URenderAsset* Asset)
{
	Asset->SetPath(Key);
	AssetMap[Key] = Asset;
}

void UAssetManager::PrepareAssetRegistration(const TMap<FString, URenderAsset*>& Replacements, TMap<FString, URenderAsset*>& Prepared) const
{
	Prepared = AssetMap;
	for (const auto& [Key, Asset] : Replacements) Prepared[Key] = Asset;
}

void UAssetManager::CommitAssetRegistration(TMap<FString, URenderAsset*>& Prepared) noexcept
{
	for (const auto& [Key, Previous] : AssetMap)
	{
		URenderAsset** Next = Prepared.Find(Key);
		if (Previous && Next && *Next != Previous) Previous->SetRetired(true);
	}
	AssetMap.Swap(Prepared);
}

UTexture2D* UAssetManager::LoadTexture(const FString& InPath, bool GenerateMips)
{
	if (URenderAsset * *Found{ AssetMap.Find(InPath) })
	{
		return Cast<UTexture2D>(*Found);
	}

	TUniquePtr<FTexture2D> Resource{};
	if (GetAssetExtension(InPath) == ".dds")
	{
		Resource = RenderCommand::LoadDDSTexture(InPath);
	}
	else
	{
		const FImageData Data{ ImageLoader::LoadAuto(InPath) };
		if (!Data.IsValid())
		{
			return nullptr;
		}

		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Data.Width;
		Desc.Height = Data.Height;
		Desc.ArraySize = 1;
		Desc.Format = Data.Format;
		Desc.SampleDesc.Count = 1;
		Desc.MipLevels = GenerateMips ? 0 : 1;
		Desc.Usage = GenerateMips ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (GenerateMips ? D3D11_BIND_RENDER_TARGET : 0);
		Desc.MiscFlags = GenerateMips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
		Resource = RenderCommand::CreateTexture2D(Desc, Data);
	}

	if (!Resource || !Resource->GetSRV())
	{
		return nullptr;
	}

	UTexture2D* Asset{ NewObject<UTexture2D>() };
	Asset->SetResource(std::move(Resource));
	RegisterAsset(InPath, Asset);

	return Asset;
}

UTexture2D* UAssetManager::CreateTransientTexture(const FImageData& Image, UObject* Outer)
{
	if (!Image.IsValid())
	{
		return nullptr;
	}
	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Image.Width;
	Desc.Height = Image.Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = Image.Format;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TUniquePtr<FTexture2D> Resource = RenderCommand::CreateTexture2D(Desc, Image);
	if (!Resource || !Resource->GetSRV())
	{
		return nullptr;
	}
	UTexture2D* Texture = NewObject<UTexture2D>(Outer);
	Texture->SetResource(std::move(Resource));
	return Texture;
}

UFont* UAssetManager::LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath)
{
	UFont* Font = NewObject<UFont>();

	if (!Font->LoadFontAtlasJson(JsonPath))
	{
		return nullptr;
	}

	Font->AtlasTexture = LoadTexture(AtlasTexturePath, false);
	if (!Font->AtlasTexture)
	{
		return nullptr;
	}

	RegisterAsset(JsonPath, Font);
	return Font;
}

UStaticMesh* UAssetManager::LoadObjStaticMesh(const FString& Path)
{
	const FString Key = MakeAssetKey(Path);
	if (UStaticMesh* Cached = GetAssetByPath<UStaticMesh>(Key))
	{
		return Cached;
	}

	TUniquePtr<FStaticMeshData> Data = FObjImporter::LoadStaticMeshData(Path);
	UStaticMesh* Mesh = CreateStaticMesh(Data.get()); // Data가 nullptr이면 nullptr 반환
	if (!Mesh)
	{
		return nullptr;
	}

	Get().RegisterAsset(Key, Mesh);
	return Mesh;
}

bool UAssetManager::ReorientStaticMesh(UStaticMesh& Mesh, const std::function<FVector(const FVector&)>& Rotate)
{
	if (Mesh.GetLODCount() > 1 || Mesh.MeshData.Vertices.IsEmpty())
	{
		return false; // LOD가 이미 옛 방향으로 만들어졌으면 서로 어긋난다
	}

	FStaticMeshData& Data = Mesh.MeshData;
	FBox Bounds{ FVector(FLT_MAX, FLT_MAX, FLT_MAX), FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX) };
	for (FVertex& Vertex : Data.Vertices)
	{
		Vertex.Position = Rotate(Vertex.Position);
		Vertex.Normal = Rotate(Vertex.Normal);
		Bounds.Min = FVector(std::min(Bounds.Min.X, Vertex.Position.X), std::min(Bounds.Min.Y, Vertex.Position.Y), std::min(Bounds.Min.Z, Vertex.Position.Z));
		Bounds.Max = FVector(std::max(Bounds.Max.X, Vertex.Position.X), std::max(Bounds.Max.Y, Vertex.Position.Y), std::max(Bounds.Max.Z, Vertex.Position.Z));
	}
	Data.AABB = Bounds;
	Data.BuildTangentSpace();

	// 인덱스는 그대로다 (회전은 삼각형 감기 방향을 바꾸지 않는다). 정점 버퍼만 다시 올린다.
	TUniquePtr<FVertexBuffer> VB = RenderCommand::CreateStaticVertexBuffer(Data.Vertices.GetData(),
		sizeof(FVertex) * static_cast<uint32>(Data.Vertices.Num()), sizeof(FVertex));
	if (!VB)
	{
		return false;
	}
	Mesh.VertexBuffer = std::move(VB);
	Mesh.RebuildRenderData();

	Data.BuildTriangleBVH(); // 피킹용 삼각형 BVH도 새 좌표로
	return true;
}

void UAssetManager::ForEachAsset(const std::function<void(URenderAsset& Asset)>& Func)
{
	for (auto& [Key, Asset] : Get().AssetMap)
	{
		if (Asset && !Asset->IsRetired())
		{
			Func(*Asset);
		}
	}
}

void UAssetManager::ForEachObjStaticMesh(const std::function<void(const FString& Key, UStaticMesh& Mesh)>& Func)
{
	for (auto& [Key, Asset] : Get().AssetMap)
	{
		if (!Asset || !Asset->IsA<UStaticMesh>() || !fs::path(Key).extension().string().ends_with(".obj"))
		{
			continue;
		}
		Func(Key, *Cast<UStaticMesh>(Asset));
	}
}

FLODGenerateResult UAssetManager::GenerateStaticMeshLODs(UStaticMesh& Mesh, const FLODGenerateRequest& Request)
{
	TArray<FStaticMeshData> Generated;
	FLODGenerateResult Result = FStaticMeshLODGenerator::Generate(Mesh.GetMeshData(), Generated);

	if (!Result.bSuccess)
	{
		return Result;
	}

	if (!(Request.ScreenThresholds[0] >
				Request.ScreenThresholds[1] &&
			Request.ScreenThresholds[1] >
				Request.ScreenThresholds[2] &&
			Request.ScreenThresholds[2] > 0.0f))
	{
		Result.bSuccess = false;
		Result.FailureReason = "Screen thresholds must descend";
		return Result;
	}

	TArray<FStaticMeshLODResource> NewResources;

	for (FStaticMeshData& Data : Generated)
	{
		FStaticMeshLODResource Resource;
		Resource.Data = std::move(Data);

		Resource.Data.BuildTangentSpace();
		Resource.Data.OptimizeTriangleOrderForVertexCache();

		Resource.VertexBuffer = RenderCommand::CreateStaticVertexBuffer(
			Resource.Data.Vertices.GetData(),
			sizeof(FVertex) *
				static_cast<uint32>(Resource.Data.Vertices.Num()),
			sizeof(FVertex));

		Resource.IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
			Resource.Data.Indices.GetData(),
			static_cast<uint32>(Resource.Data.Indices.Num()));

		if (!Resource.VertexBuffer || !Resource.IndexBuffer)
		{
			Result.bSuccess = false;
			Result.FailureReason = "LOD GPU buffer creation failed";
			return Result;
		}

		NewResources.Add(std::move(Resource));
	}

	// bSaveToAsset이면 이 지점에서 파일 저장.
	// 저장 실패 시 기존 Mesh.AdditionalLODs는 그대로 둔다.

	Mesh.AdditionalLODs = std::move(NewResources);
	Mesh.ScreenThresholds = Request.ScreenThresholds;
	Mesh.RebuildRenderData();
	return Result;
}
