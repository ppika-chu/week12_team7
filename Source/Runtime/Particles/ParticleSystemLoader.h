#pragma once

#include "Core/Types.h"

class UParticleSystem;

// .particle 파일과 UParticleSystem 사이의 변환, 그리고 에셋 매니저 등록을 담당한다.
// 에셋 매니저는 확장자만 보고 이 로더에 넘긴다 (FFbxAssetLoader와 같은 역할 분담).
class FParticleSystemLoader
{
public:
	// 파일을 읽어 조립, 빌드, 등록까지 마친다. 실패하면 nullptr를 반환하고 Error를 채운다.
	// Key는 에셋 매니저 검색 키(Assets 기준 상대 경로), Path는 실제 파일 경로.
	static UParticleSystem* LoadAsset(const FString& Key, const FString& Path, FString& Error);

	// 현재 구조를 파일로 저장한다.
	static bool SaveAsset(UParticleSystem* System, const FString& Path, FString& Error);
};
