#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"

class UParticleSystem;
class UParticleEmitter;
class UParticleModule;
class UObject;

// .particle 파일과 UParticleSystem 사이의 변환, 그리고 에셋 매니저 등록을 담당한다.
class FParticleSystemLoader
{
public:
	static UParticleSystem* LoadAsset(const FString& Key, const FString& Path, FString& Error);
	static bool SaveAsset(UParticleSystem* System, const FString& Path, FString& Error);
	// Source를 통째로 복사해 DestSystem의 InsertIndex 자리에 넣는다 (UE FCascade::DuplicateEmitter, 공유 없음)
	static UParticleEmitter* DuplicateEmitter(UParticleEmitter* Source, UParticleSystem* DestSystem, int32 InsertIndex, FString& Error);
	// 모듈 하나를 분포까지 복사한다. Outer는 넣을 LOD (UE FCascade::CopyModuleToEmitter)
	static UParticleModule* DuplicateModule(UParticleModule* Source, UObject* Outer, FString& Error);
};
