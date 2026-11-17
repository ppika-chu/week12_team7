#include "EnginePCH.h"

#include "Editor/Viewports/PreviewViewport.h"

#include "Render/RenderCommand.h"

#include <imgui.h>
#include <cmath>

void FPreviewViewport::Tick()
{
	// 렌더(RenderMultipleViewports)가 Tick 뒤에 오므로 여기서 타깃과 카메라를 맞춰 둔다
	UpdateRenderTargets();
	UpdateCamera();
}

bool FPreviewViewport::UpdateGizmo(const FVector2& MousePos)
{
	if (!ImGui::IsItemHovered() && !Gizmo.IsUsing())
	{
		return false;
	}

	// 누르는 순간에만 드래그를 시작한다. Alt + 좌클릭은 오빗
	const bool bMouseDown = Gizmo.IsUsing()
		? ImGui::IsMouseDown(ImGuiMouseButton_Left)
		: ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt;
	Gizmo.Update(
		ViewportClient.DeprojectScreen(MousePos),
		MousePos,
		ViewportClient.CalcViewProjection(),
		static_cast<int>(TargetWidth),
		static_cast<int>(TargetHeight),
		bMouseDown,
		ViewportClient.GetViewLocation(),
		ViewportClient.IsOrthographic());
	return true;
}

void FPreviewViewport::FocusOn(const FVector& Target, float Distance)
{
	FocusTarget = Target;
	FocusDistance = Distance;
	OrbitTarget = Target;
	// 언리얼 SAnimationEditorViewport의 초기 카메라(EditorViewportDefs::DefaultPerspectiveViewRotation(-15, -90, 0))와 같음
	OrbitYaw = -90.0f;
	OrbitPitch = -15.0f;
	OrbitDistance = Distance;
}

void FPreviewViewport::HandleCameraInput(bool bAllowOrbit)
{
	const ImGuiIO& IO = ImGui::GetIO();

	// 이미지 위에서 누른 채 드래그 중일 때만
	if (ImGui::IsItemActive())
	{
		// 언리얼처럼 Alt + 좌클릭만 오빗. 그냥 좌클릭은 본 선택 / 기즈모용으로 남겨 둔다
		if (IO.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left) && bAllowOrbit)
		{
			// 뒤집힌 상태에서는 화면 좌우도 반대이므로 Yaw 입력을 반전
			const bool bUpsideDown = std::cos(OrbitPitch * (3.14159265f / 180.0f)) < 0.0f;
			OrbitYaw += IO.MouseDelta.x * 0.3f * (bUpsideDown ? -1.0f : 1.0f);
			OrbitPitch -= IO.MouseDelta.y * 0.3f;
			// 계속 돌려도 값이 커지지 않게 -180 ~ 180으로 감싼다
			OrbitPitch = std::remainder(OrbitPitch, 360.0f);
		}
		else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			// 화면 기준 오른쪽, 위쪽으로 대상 점을 옮긴다. 멀리 있을수록 크게 움직인다
			const float YawRad = OrbitYaw * (3.14159265f / 180.0f);
			const float PitchRad = OrbitPitch * (3.14159265f / 180.0f);
			const FVector Right(-std::sin(YawRad), std::cos(YawRad), 0.0f);
			const FVector Up(-std::sin(PitchRad) * std::cos(YawRad), -std::sin(PitchRad) * std::sin(YawRad), std::cos(PitchRad));
			const float PanSpeed = OrbitDistance * 0.02f;
			OrbitTarget = OrbitTarget - Right * (IO.MouseDelta.x * PanSpeed) + Up * (IO.MouseDelta.y * PanSpeed);
		}
		else if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			const auto CalcForward = [this]()
			{
				const float YawRad = OrbitYaw * (3.14159265f / 180.0f);
				const float PitchRad = OrbitPitch * (3.14159265f / 180.0f);
				return FVector(std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad), std::sin(PitchRad));
			};

			// 제자리 회전: 카메라 위치는 고정하고 방향만 돌린 뒤, 그 앞쪽으로 Target을 다시 잡는다
			FVector Location = OrbitTarget - CalcForward() * OrbitDistance;
			// 오빗으로 뒤집힌 상태였다면, 같은 방향을 보는 바로 선 값으로 바꾼다 (Pitch' = 180 - Pitch, Yaw' = Yaw + 180)
			// 언리얼 Fly 카메라처럼 우클릭 회전은 뒤집히지 않게 ±89로 막는다
			if (std::cos(OrbitPitch * (3.14159265f / 180.0f)) < 0.0f)
			{
				OrbitPitch = std::remainder(180.0f - OrbitPitch, 360.0f);
				OrbitYaw += 180.0f;
			}
			OrbitYaw += IO.MouseDelta.x * 0.3f;
			OrbitPitch -= IO.MouseDelta.y * 0.3f;
			OrbitPitch = OrbitPitch > 89.0f ? 89.0f : (OrbitPitch < -89.0f ? -89.0f : OrbitPitch);
			const FVector Forward = CalcForward();

			// WASD: 바라보는 방향 기준 앞뒤 / 좌우, QE: 월드 위아래 (언리얼과 동일)
			const float YawRad = OrbitYaw * (3.14159265f / 180.0f);
			const FVector Right(-std::sin(YawRad), std::cos(YawRad), 0.0f);
			const float Axis[3] = {
				(ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_S) ? 1.0f : 0.0f),
				(ImGui::IsKeyDown(ImGuiKey_D) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_A) ? 1.0f : 0.0f),
				(ImGui::IsKeyDown(ImGuiKey_E) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0f : 0.0f)
			};
			// 메시 크기에 맞춰 1초에 오빗 거리만큼 이동
			const float MoveSpeed = OrbitDistance * IO.DeltaTime;
			Location = Location + (Forward * Axis[0] + Right * Axis[1] + FVector(0.0f, 0.0f, 1.0f) * Axis[2]) * MoveSpeed;

			OrbitTarget = Location + Forward * OrbitDistance;
		}
	}

	if (ImGui::IsItemHovered())
	{
		if (IO.MouseWheel != 0.0f)
		{
			// 한 칸마다 10%씩. 대상을 뚫고 지나가지 않게 최소 거리 유지
			OrbitDistance *= std::pow(0.9f, IO.MouseWheel);
			OrbitDistance = OrbitDistance < 0.1f ? 0.1f : OrbitDistance;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_F))
		{
			FocusOn(FocusTarget, FocusDistance);
		}
	}
}

void FPreviewViewport::UpdateRenderTargets()
{
	if (DesiredWidth == 0 || DesiredHeight == 0)
	{
		return;
	}
	if (DesiredWidth == TargetWidth && DesiredHeight == TargetHeight)
	{
		return;
	}

	// PIE 창 타깃과 같은 구성. Fog가 깊이를 읽으므로 깊이는 TYPELESS로 만든다
	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = DesiredWidth;
	Desc.Height = DesiredHeight;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	ColorTarget = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	DepthTarget = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	HDRTarget = RenderCommand::CreateTexture2D(Desc);

	RenderingInfo.ColorRenderTargets.Reset();
	RenderingInfo.ViewportSetting.Width = DesiredWidth;
	RenderingInfo.ViewportSetting.Height = DesiredHeight;
	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = ColorTarget.get();
	RenderingInfo.ColorRenderTargets.Add(ColorDesc);
	RenderingInfo.DepthStencil.Texture = DepthTarget.get();

	TargetWidth = DesiredWidth;
	TargetHeight = DesiredHeight;
	ViewportClient.SetViewRect({ 0.0f, 0.0f, static_cast<float>(TargetWidth), static_cast<float>(TargetHeight) });
}

void FPreviewViewport::UpdateCamera()
{
	// 구면좌표 → 바라보는 방향. 카메라 위치 계산에 쓴다
	const float YawRad = OrbitYaw * (3.14159265f / 180.0f);
	const float PitchRad = OrbitPitch * (3.14159265f / 180.0f);
	const FVector Forward(std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad), std::sin(PitchRad));
	const FVector Location = OrbitTarget - Forward * OrbitDistance;

	// 이 엔진의 FRotator는 Pitch +가 아래라서 부호를 뒤집는다
	const FQuat Rotation = FRotator(-OrbitPitch, OrbitYaw, 0.0f).Quaternion();

	// 카메라 → 오빗 대상 → 원점 → 장면 끝까지 더한 값. 어떤 줌과 패닝에서도 장면에서 가장 먼 점이 이 안에 든다
	const float FarClip = OrbitDistance + OrbitTarget.Size() + SceneRadius;
	const FCameraProjection Projection{ EProjectionMode::Perspective, 60.0f, 10.0f, 0.01f, FarClip };
	ViewportClient.SetCameraOverride(Location, Forward, Projection, &Rotation);
}
