#pragma once

#include <cstring>

// 시드를 가진 난수 생성기 (UE Core/Math/RandomStream.h).
// 같은 시드로 시작하면 항상 같은 순서의 난수가 나온다.
// 파티클 컴포넌트가 하나씩 갖고 있으면 이펙트를 다시 재생해도 같은 모양이 나오게 만들 수 있다.
struct FRandomStream
{
public:
	FRandomStream() = default;

	explicit FRandomStream(int32 InSeed)
	{
		Initialize(InSeed);
	}

	void Initialize(int32 InSeed)
	{
		InitialSeed = InSeed;
		Seed = static_cast<uint32>(InSeed);
	}

	// 처음 시드로 되돌린다. 이후 난수가 처음부터 같은 순서로 다시 나온다.
	void Reset()
	{
		Seed = static_cast<uint32>(InitialSeed);
	}

	int32 GetInitialSeed() const { return InitialSeed; }

	// [0, 1) 균등 난수
	float GetFraction() const
	{
		MutateSeed();

		// 지수부를 1.0으로 고정하고 가수부에 시드의 상위 23비트를 넣으면 [1, 2) 범위의 float가 된다.
		const uint32 Bits = 0x3F800000U | (Seed >> 9);
		float Result;
		std::memcpy(&Result, &Bits, sizeof(float));
		return Result - 1.0f;
	}

	// [0, 1) 균등 난수. GetFraction과 같다.
	float FRand() const
	{
		return GetFraction();
	}

	// [Min, Max) 균등 난수
	float FRandRange(float Min, float Max) const
	{
		return Min + (Max - Min) * GetFraction();
	}

	uint32 GetUnsignedInt() const
	{
		MutateSeed();
		return Seed;
	}

private:
	// 선형 합동 생성기. UE와 같은 상수를 쓴다.
	void MutateSeed() const
	{
		Seed = (Seed * 196314165U) + 907633515U;
	}

private:
	int32 InitialSeed = 0;
	mutable uint32 Seed = 0;
};
