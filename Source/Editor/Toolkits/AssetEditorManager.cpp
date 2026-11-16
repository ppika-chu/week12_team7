#include "EnginePCH.h"

#include "Editor/Toolkits/AssetEditorManager.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Editor/AnimationEditor/AnimationEditor.h"
#include "Editor/ParticleEditor/ParticleSystemEditor.h"
#include "Editor/SkeletalMeshEditor/SkeletalMeshEditor.h"
#include "Particles/ParticleSystem.h"
#include "Render/SkeletalMesh.h"

bool FAssetEditorManager::Init()
{
	return true;
}

void FAssetEditorManager::Tick(float DeltaTime)
{
	// 지난 프레임에 닫힌 창 정리. ImGui가 그 창의 SRV를 다 그린 뒤라 안전하다
	for (int32 i = Editors.Num() - 1; i >= 0; --i)
	{
		if (!Editors[i]->IsOpen())
		{
			Editors.RemoveAt(i);
		}
	}

	// 지우는 시점이 위와 같아서 여기서 교체한다
	ReopenRetiredEditors();

	// FEditorUI는 Window 메뉴로 꺼진 패널도 Tick을 부르므로 여기서 막는다
	if (!IsOpen())
	{
		return;
	}

	for (auto& Editor : Editors)
	{
		Editor->Tick(DeltaTime);
	}
}

void FAssetEditorManager::OnRender()
{
	for (auto& Editor : Editors)
	{
		Editor->OnRender();
	}
}

bool FAssetEditorManager::OpenEditor(URenderAsset* Asset)
{
	if (!Asset)
	{
		return false;
	}

	if (FocusOpenEditor(Asset))
	{
		// Window 메뉴로 꺼 두었으면 에디터들이 Tick / 렌더되지 않으므로 다시 켠다
		SetOpen(true);
		return true;
	}

	TUniquePtr<FAssetEditorToolkit> NewEditor = CreateEditor(Asset, NextEditorId);
	if (!NewEditor)
	{
		return false;
	}

	++NextEditorId;
	Editors.Add(std::move(NewEditor));
	SetOpen(true);
	return true;
}

TUniquePtr<FAssetEditorToolkit> FAssetEditorManager::CreateEditor(URenderAsset* Asset, int32 EditorId) const
{
	if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset))
	{
		return MakeUnique<FSkeletalMeshEditor>(Mesh, EditorId);
	}

	if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
	{
		// 클립에는 메시가 없으므로 같은 스켈레톤의 메시로 보여 준다 (UE USkeleton::FindCompatibleMesh)
		USkeletalMesh* PreviewMesh = Sequence->GetSkeleton() ? Sequence->GetSkeleton()->FindCompatibleMesh() : nullptr;
		if (!PreviewMesh)
		{
			HTR_LOG(Warning, "[Animation] No skeletal mesh uses the skeleton of {}", Sequence->GetPath());
			return nullptr;
		}
		return MakeUnique<FAnimationEditor>(Sequence, PreviewMesh, EditorId);
	}

	if (UParticleSystem* System = Cast<UParticleSystem>(Asset))
	{
		return MakeUnique<FParticleSystemEditor>(System, EditorId);
	}

	return nullptr;
}

bool FAssetEditorManager::FocusOpenEditor(const URenderAsset* Asset)
{
	for (auto& Editor : Editors)
	{
		if (Editor->GetEditedAsset() == Asset)
		{
			Editor->Focus();
			return true;
		}
	}
	return false;
}

void FAssetEditorManager::ReopenRetiredEditors()
{
	for (auto& Editor : Editors)
	{
		if (!Editor->IsEditedAssetRetired())
		{
			continue;
		}

		if (TUniquePtr<FAssetEditorToolkit> NewEditor = Editor->CreateReplacementEditor(NextEditorId))
		{
			++NextEditorId;
			Editor = std::move(NewEditor);
		}
	}
}

bool FAssetEditorManager::HasFocusedEditor() const
{
	// 패널이 꺼지면 OnRender가 안 불려 에디터의 포커스 값이 갱신되지 않으므로 여기서 막는다
	if (!IsOpen())
	{
		return false;
	}

	for (const auto& Editor : Editors)
	{
		if (Editor->IsFocused())
		{
			return true;
		}
	}
	return false;
}

void FAssetEditorManager::GetPreviewsToRender(TArray<const FAssetEditorToolkit*>& OutEditors) const
{
	OutEditors.Reset();

	// 패널이 꺼지면 OnRender가 안 불려 에디터의 가시성이 갱신되지 않으므로 여기서 막는다
	if (!IsOpen())
	{
		return;
	}

	for (const auto& Editor : Editors)
	{
		if (Editor->ShouldRenderPreview())
		{
			OutEditors.Add(Editor.get());
		}
	}
}
