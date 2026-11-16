#pragma once
#include "Editor/Gizmo/Gizmo.h"
#include "Editor/Persona/AnimationEditorPreviewScene.h"
#include "Editor/Toolkits/AssetEditorToolkit.h"

class URenderAsset;
class USkeletalMesh;
class UTexture2D;
class USkeletalMeshComponent;
class FLineBatcher;

// 뷰포트에 본을 어디까지 그릴지
enum class EBoneDrawMode : uint8
{
	None,
	Selected,   // 선택한 본과 부모, 자식
	All,
};

enum class ESkinWeightViewMode : uint8
{
	Material, // 기본 스킨 색상
	Greyscale,
	Turbo,
	Jet,
	Viridis,
	Inferno,
};

// 스켈레탈 메시 에디터와 애니메이션 에디터의 공용 부모. 스켈레톤 트리, 본 그리기, Skin Weight 보기를 가진다
// (UE는 FPersonaToolkit + ISkeletonTree로 나뉘어 있다).
class FPersonaAssetEditor : public FAssetEditorToolkit
{
public:
	FPersonaAssetEditor(USkeletalMesh* InMesh, int32 InEditorId);
	~FPersonaAssetEditor() override;

	// 안 보이면 skip
	void Tick(float DeltaTime) override;

	// 편집 에셋이나 프리뷰 메시 중 하나라도 재임포트되었는지
	bool IsEditedAssetRetired() const override;
	// 프리뷰에 쓰는 메시. 재임포트되면 IsRetired()가 true
	USkeletalMesh* GetMesh() const { return Mesh; }
	// 재임포트로 에디터를 새로 만들 때 선택을 옮긴다. 본 구성이 바뀌었을 수 있어 이름으로
	FString GetSelectedBoneName() const;
	virtual void SelectBoneByName(const FString& BoneName);

	// 현재 포즈의 본을 구 + 사각뿔 선으로 쌓는다
	void DrawPreviewOverlay(FLineBatcher& LineBatcher) const override;

protected:
	FAdvancedPreviewScene* GetPreviewScene() const override { return PreviewScene.get(); }
	// Show 메뉴 + 보기 콤보
	void DrawViewportToolbar() override;
	// 본 / 정점 / 삼각형 수 + GetDisplayInfo
	FString GetViewportStats() const override;
	// 뷰포트 왼쪽 위 통계 아래에 붙일 줄. 언리얼 FAnimationViewportClient::GetDisplayInfo
	virtual FString GetDisplayInfo() const { return FString(); }
	// 뷰포트 보기 콤보(Lit / Skin Weights) 안에 파생 에디터가 하위 메뉴를 더 붙인다. 기본은 없음
	virtual void DrawViewModeMenuExtras() {}
	// 본 계층을 트리로 그리고, 클릭하면 SelectedBoneIndex 갱신. 언리얼도 두 에디터가 스켈레톤 트리를 같이 쓴다
	void DrawSkeletonTree();
	// 본 하나와 그 자식들을 재귀로 그린다
	void DrawBoneTreeNode(int32 BoneIndex);
	// Skin Weight 컬러 램프 동기화
	void SyncPreviewSkinWeightView();

	// Local * Parent 누적. 프리뷰 액터가 원점이라 Component 공간 = 프리뷰 월드 공간
	const TArray<FMatrix>& GetComponentSpaceTransforms() const;
	// Ancestor가 Bone의 부모 / 조상인지 (자기 자신은 false)
	bool IsAncestorOf(int32 Ancestor, int32 Bone) const;
	// 지금 모드에서 이 본을 그리는지. 그리는 본만 뷰포트에서 고를 수 있다
	bool ShouldDrawBone(int32 BoneIndex) const;
	// 관절 위치 (Component 공간 = 프리뷰 월드 공간)
	FVector GetBoneLocation(int32 BoneIndex) const;

	static constexpr float BoneDrawSize = 0.01f;    // 관절 구 반지름 (m). UPersonaOptions 기본값 1cm

	USkeletalMesh* Mesh = nullptr;  // 소유는 UAssetManager. 재임포트되어도 지워지지 않는다
	TUniquePtr<FAnimationEditorPreviewScene> PreviewScene;

	TArray<TArray<int32>> BoneChildren;         // 트리 / 자식 연결용. 생성자에서 한 번 만든다
	TArray<int32> RootBones;                    // ParentIndex가 -1인 본들

	EBoneDrawMode BoneDrawMode = EBoneDrawMode::All;
	int32 SelectedBoneIndex = -1;
	bool bRevealSelectedInTree = false;         // 뷰포트에서 고르면 다음 트리 그릴 때 펼치고 스크롤

	// 언리얼 스켈레톤 트리처럼 정점 가중치가 있는 본과 없는 본을 다른 아이콘으로. 생성자에서 한 번 만든다
	TArray<uint8> BoneHasWeights;               // bool. TArray가 std::vector라 vector<bool>을 피한다
	UTexture2D* BoneIcon = nullptr;             // 소유는 UAssetManager
	UTexture2D* BoneNonWeightedIcon = nullptr;

	ESkinWeightViewMode SkinWeightViewMode = ESkinWeightViewMode::Material;
};
