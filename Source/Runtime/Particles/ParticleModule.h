#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

class UParticleModuleTypeDataBase;
class UParticleEmitter;
struct FParticleEmitterInstance;
struct FBaseParticle;

// 모듈은 설정값과 함수만 갖는다.
// 한 모듈 객체를 여러 컴포넌트의 인스턴스가 공유하므로, 실행 중에 바뀌는 값을 멤버에 두면 안 된다.
class UParticleModule : public UObject
{
	DECLARE_CLASS(UParticleModule, UObject)

	REFLECT_START(ClassName)
		PROPERTY(bEnabled) PROPERTY_TOOLTIP("Skip this module when off. Values are kept.")
	REFLECT_END()

public:
	// 모든 호출에 공통으로 넘기는 정보
	struct FContext
	{
		FParticleEmitterInstance& Owner;

		explicit FContext(FParticleEmitterInstance& InOwner)
			: Owner(InOwner)
		{
		}
	};

	// 파티클 하나가 막 생성될 때
	struct FSpawnContext : FContext
	{
		int32 Offset;                 // 이 모듈 payload의 시작 위치 (파티클 칸 기준 바이트)
		float SpawnTime;              // 생성 시점부터 이번 프레임 끝까지 남은 시간
		FBaseParticle* ParticleBase;  // 생성 중인 파티클

		FSpawnContext(FParticleEmitterInstance& InOwner, int32 InOffset, float InSpawnTime, FBaseParticle* InParticleBase)
			: FContext(InOwner), Offset(InOffset), SpawnTime(InSpawnTime), ParticleBase(InParticleBase)
		{
		}
	};

	// 살아 있는 파티클 전체를 갱신할 때
	struct FUpdateContext : FContext
	{
		int32 Offset;
		float DeltaTime;

		FUpdateContext(FParticleEmitterInstance& InOwner, int32 InOffset, float InDeltaTime)
			: FContext(InOwner), Offset(InOffset), DeltaTime(InDeltaTime)
		{
		}
	};

	UParticleModule() = default;
	virtual ~UParticleModule() = default;
	virtual void InitializeDefaults() {}
	// 에디터에서 새로 추가했을 때 바로 보이도록 시작값을 넣는다. 기본은 아무것도 안 한다 (UE UParticleModule::SetToSensibleDefaults)
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner) {}
	// UE와 같이 값이 바뀌면 이 모듈이 속한 시스템의 이미터들을 다시 Build한다 (payload 배치가 바뀔 수 있다)
	void PostEditChangeProperty(const FProperty& Property) override;

	// bSpawnModule이 true이면 파티클이 생성될 때 한 번 호출된다.
	virtual void Spawn(const FSpawnContext& Context) {}
	// bUpdateModule이 true이면 매 프레임 살아 있는 파티클 전체에 대해 호출된다.
	virtual void Update(const FUpdateContext& Context) {}
	// bFinalUpdateModule이 true이면 바운딩 박스 계산까지 끝난 뒤 호출된다.
	virtual void FinalUpdate(const FUpdateContext& Context) {}

	// 파티클마다 추가로 필요한 바이트 수 (payload). 0이면 payload가 없다.
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) { return 0; }
	// 이미터 인스턴스마다 추가로 필요한 바이트 수 (인스턴스 데이터). 0이면 없다.
	virtual uint32 RequiredBytesPerInstance() { return 0; }
	// 인스턴스 데이터를 초기화한다. 처리하지 않으면 0xffffffff를 돌려준다.
	virtual uint32 PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData) { return 0xffffffff; }

public:
	bool bSpawnModule = false;
	bool bUpdateModule = false;
	bool bFinalUpdateModule = false;
	bool bEnabled = true;
};
