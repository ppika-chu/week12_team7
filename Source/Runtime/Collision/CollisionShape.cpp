#include "EnginePCH.h"
#include "CollisionShape.h"
#include "Collision/HitResult.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

FCollisionShape FCollisionShape::MakeBox(const FVector& HalfExtent)
{
	FCollisionShape Box{};
	Box.ShapeType = ECollisionShape::Box;
	Box.BoxHalfExtent = HalfExtent;
	return Box;
}

FCollisionShape FCollisionShape::MakeSphere(float Radius)
{
	FCollisionShape Sphere{};
	Sphere.ShapeType = ECollisionShape::Sphere;
	Sphere.Radius = Radius;
	return Sphere;
}

FCollisionShape FCollisionShape::MakeCapsule(float Radius, float HalfHeight)
{
	FCollisionShape Capsule{};
	Capsule.ShapeType = ECollisionShape::Capsule;
	Capsule.Radius = Radius;
	Capsule.HalfHeight = HalfHeight;
	return Capsule;
}

bool FCollisionShape::IsBox() const
{
	return ShapeType == ECollisionShape::Box;
}

bool FCollisionShape::IsSphere() const
{
	return ShapeType == ECollisionShape::Sphere;
}

bool FCollisionShape::IsCapsule() const
{
	return ShapeType == ECollisionShape::Capsule;
}

float FCollisionShape::GetCapsuleAxisHalfLength() const
{
	return std::max(0.0f, HalfHeight - Radius);
}

namespace Collision
{
double DotDouble(const FVector& A, const FVector& B);
FVector AddScaledVector(const FVector& Base, const FVector& Direction, double Scale);
bool LineTraceCylinderSide(const FVector& Start, const FVector& End, const FCapsuleSegment& Capsule, double& OutTime, FVector& OutNormal);
}

bool Collision::LineTrace(const FCollisionShape& Shape, const FVector& Start, const FVector& End, const FVector& Position, const FQuat& Rotation, FHitResult& OutHit)
{
	OutHit.Init(Start, End);

	double Time = 0.0;
	FVector Normal = FVector::ZeroVector;
	bool bHit = false;

	switch (Shape.ShapeType)
	{
	case ECollisionShape::Box:
		bHit = LineTraceBox(Start, End, ToOrientedBox(Shape, Position, Rotation), Time, Normal);
		break;
	case ECollisionShape::Sphere:
		bHit = LineTraceSphere(Start, End, Position, Shape.Radius, Time, Normal);
		break;
	case ECollisionShape::Capsule:
		bHit = LineTraceCapsule(Start, End, ToCapsuleSegment(Shape, Position, Rotation), Time, Normal);
		break;
	default:
		return false;
	}

	if (!bHit)
	{
		return false;
	}

	const FVector Delta = End - Start;
	const double NormalSquared = DotDouble(Normal, Normal);
	if (!(NormalSquared > 0.0 && DotDouble(Delta, Normal) < 0.0))
	{
		return false;
	}

	const double InverseLength = 1.0 / std::sqrt(NormalSquared);
	OutHit.bBlockingHit = true;
	OutHit.Time = static_cast<float>(Time);
	OutHit.Distance = static_cast<float>(std::sqrt(DotDouble(Delta, Delta)) * Time);
	OutHit.Location = AddScaledVector(Start, Delta, Time);
	OutHit.ImpactPoint = OutHit.Location;
	OutHit.Normal = FVector(
		static_cast<float>(Normal.X * InverseLength),
		static_cast<float>(Normal.Y * InverseLength),
		static_cast<float>(Normal.Z * InverseLength));
	OutHit.ImpactNormal = OutHit.Normal;
	return true;
}

bool Collision::LineTraceBox(const FVector& Start, const FVector& End, const FOrientedBox& Box, double& OutTime, FVector& OutNormal)
{
	if (Box.Extent.X <= 0.0f || Box.Extent.Y <= 0.0f || Box.Extent.Z <= 0.0f)
	{
		return false;
	}

	const FVector Offset = Start - Box.Center;
	const FVector Delta = End - Start;
	double EnterTime = -DBL_MAX;
	double ExitTime = DBL_MAX;
	FVector EnterNormal = FVector::ZeroVector;

	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		const FVector& Axis = Box.Axis[AxisIndex];
		const double Origin = DotDouble(Offset, Axis);
		const double Direction = DotDouble(Delta, Axis);
		const double Extent = Box.Extent[AxisIndex];

		if ((Origin >= Extent && Direction >= 0.0) ||
			(Origin <= -Extent && Direction <= 0.0))
		{
			return false;
		}
		if (Direction == 0.0)
		{
			continue;
		}

		double NearTime = (-Extent - Origin) / Direction;
		double FarTime = (Extent - Origin) / Direction;
		FVector NearNormal = -Axis;
		if (NearTime > FarTime)
		{
			std::swap(NearTime, FarTime);
			NearNormal = Axis;
		}
		if (NearTime > EnterTime)
		{
			EnterTime = NearTime;
			EnterNormal = NearNormal;
		}
		ExitTime = std::min(ExitTime, FarTime);
		if (EnterTime > ExitTime)
		{
			return false;
		}
	}

	if (EnterTime < 0.0 || EnterTime > 1.0 || ExitTime <= EnterTime)
	{
		return false;
	}

	OutTime = EnterTime;
	OutNormal = EnterNormal;
	return true;
}

bool Collision::LineTraceSphere(const FVector& Start, const FVector& End, const FVector& Center, float Radius, double& OutTime, FVector& OutNormal)
{
	const FVector Delta = End - Start;
	const double LengthSquared = DotDouble(Delta, Delta);
	if (Radius <= 0.0f || LengthSquared == 0.0)
	{
		return false;
	}

	const FVector Offset = Start - Center;
	const double CenterTime = -DotDouble(Offset, Delta) / LengthSquared;
	const FVector ClosestOffset = AddScaledVector(Offset, Delta, CenterTime);
	const double Remaining = static_cast<double>(Radius) * Radius - DotDouble(ClosestOffset, ClosestOffset);
	if (Remaining <= 0.0)
	{
		return false;
	}

	const double Time = CenterTime - std::sqrt(Remaining / LengthSquared);
	if (!(Time >= 0.0 && Time <= 1.0))
	{
		return false;
	}

	const FVector CenterToHit = AddScaledVector(Offset, Delta, Time);
	OutTime = Time;
	OutNormal = CenterToHit;
	return true;
}

bool Collision::LineTraceCapsule(const FVector& Start, const FVector& End, const FCapsuleSegment& Capsule, double& OutTime, FVector& OutNormal)
{
	if (Capsule.Radius <= 0.0f)
	{
		return false;
	}

	const FVector AxisVector = Capsule.B - Capsule.A;
	const FVector StartOffset = Start - Capsule.A;
	const double AxisSquared = DotDouble(AxisVector, AxisVector);
	const double Ratio = AxisSquared > 0.0
		? std::clamp(DotDouble(StartOffset, AxisVector) / AxisSquared, 0.0, 1.0)
		: 0.0;
	const FVector AxisToStart = AddScaledVector(StartOffset, AxisVector, -Ratio);
	if (DotDouble(AxisToStart, AxisToStart) < static_cast<double>(Capsule.Radius) * Capsule.Radius)
	{
		return false;
	}

	bool bFound = false;
	auto KeepClosest = [&](double Time, const FVector& Normal)
	{
		if (!bFound || Time < OutTime)
		{
			bFound = true;
			OutTime = Time;
			OutNormal = Normal;
		}
	};

	double CandidateTime = 0.0;
	FVector CandidateNormal = FVector::ZeroVector;

	if (LineTraceSphere(Start, End, Capsule.A, Capsule.Radius, CandidateTime, CandidateNormal))
	{
		KeepClosest(CandidateTime, CandidateNormal);
	}
	if (LineTraceSphere(Start, End, Capsule.B, Capsule.Radius, CandidateTime, CandidateNormal))
	{
		KeepClosest(CandidateTime, CandidateNormal);
	}
	if (LineTraceCylinderSide(Start, End, Capsule, CandidateTime, CandidateNormal))
	{
		KeepClosest(CandidateTime, CandidateNormal);
	}
	return bFound;
}

bool Collision::LineTraceCylinderSide(const FVector& Start, const FVector& End, const FCapsuleSegment& Capsule, double& OutTime, FVector& OutNormal)
{
	const FVector AxisVector = Capsule.B - Capsule.A;
	const double AxisLength = std::sqrt(DotDouble(AxisVector, AxisVector));
	if (AxisLength == 0.0)
	{
		return false;
	}

	const FVector Axis(
		static_cast<float>(AxisVector.X / AxisLength),
		static_cast<float>(AxisVector.Y / AxisLength),
		static_cast<float>(AxisVector.Z / AxisLength));

	const FVector Offset = Start - Capsule.A;
	const FVector Delta = End - Start;
	const double AlongStart = DotDouble(Offset, Axis);
	const double AlongDelta = DotDouble(Delta, Axis);
	const FVector RadialStart = AddScaledVector(Offset, Axis, -AlongStart);
	const FVector RadialDelta = AddScaledVector(Delta, Axis, -AlongDelta);
	const double RadialLengthSquared = DotDouble(RadialDelta, RadialDelta);
	if (RadialLengthSquared == 0.0)
	{
		return false;
	}

	const double CenterTime = -DotDouble(RadialStart, RadialDelta) / RadialLengthSquared;
	const FVector ClosestRadial = AddScaledVector(RadialStart, RadialDelta, CenterTime);
	const double Remaining = static_cast<double>(Capsule.Radius) * Capsule.Radius - DotDouble(ClosestRadial, ClosestRadial);
	if (Remaining <= 0.0)
	{
		return false;
	}

	const double Time = CenterTime - std::sqrt(Remaining / RadialLengthSquared);
	const double Along = AlongStart + AlongDelta * Time;
	if (!(Time >= 0.0 && Time <= 1.0 && Along >= 0.0 && Along <= AxisLength))
	{
		return false;
	}

	OutTime = Time;
	OutNormal = AddScaledVector(RadialStart, RadialDelta, Time);
	return true;
}

double Collision::DotDouble(const FVector& A, const FVector& B)
{
	return static_cast<double>(A.X) * B.X +
		static_cast<double>(A.Y) * B.Y +
		static_cast<double>(A.Z) * B.Z;
}

FVector Collision::AddScaledVector(const FVector& Base, const FVector& Direction, double Scale)
{
	return FVector(
		static_cast<float>(static_cast<double>(Base.X) + static_cast<double>(Direction.X) * Scale),
		static_cast<float>(static_cast<double>(Base.Y) + static_cast<double>(Direction.Y) * Scale),
		static_cast<float>(static_cast<double>(Base.Z) + static_cast<double>(Direction.Z) * Scale));
}

Collision::FCapsuleSegment Collision::ToCapsuleSegment(const FCollisionShape& Shape, const FVector& Pos, const FQuat& Rot)
{
	FVector Temp = Rot.GetUpVector() * Shape.GetCapsuleAxisHalfLength();
	return FCapsuleSegment{
		Pos - Temp,
		Pos + Temp,
		Shape.Radius
	};
}

Collision::FOrientedBox Collision::ToOrientedBox(const FCollisionShape& Shape, const FVector& Pos, const FQuat& Rot)
{
	return FOrientedBox{
		Pos,
		{ Rot.GetForwardVector(), Rot.GetRightVector(), Rot.GetUpVector() },
		Shape.BoxHalfExtent
	};
}

bool Collision::Overlap(const FCollisionShape& A, const FVector& PosA, const FQuat& RotA,
	const FCollisionShape& B, const FVector& PosB, const FQuat& RotB,
	FCollisionContact* OutContact)
{
	if ((A.IsSphere() || A.IsCapsule()) && (B.IsSphere() || B.IsCapsule()))
	{
		FCapsuleSegment CapA = Collision::ToCapsuleSegment(A, PosA, RotA);
		FCapsuleSegment CapB = Collision::ToCapsuleSegment(B, PosB, RotB);

		return OverlapCapsuleCapsule(CapA, CapB, OutContact);
	}
	else if ((A.IsSphere() || A.IsCapsule()) && B.IsBox())
	{
		FCapsuleSegment CapA = Collision::ToCapsuleSegment(A, PosA, RotA);
		FOrientedBox BoxB = Collision::ToOrientedBox(B, PosB, RotB);

		return OverlapCapsuleBox(CapA, BoxB, OutContact);
	}
	else if (A.IsBox() && (B.IsSphere() || B.IsCapsule()))
	{
		FOrientedBox BoxA = Collision::ToOrientedBox(A, PosA, RotA);
		FCapsuleSegment CapB = Collision::ToCapsuleSegment(B, PosB, RotB);

		const bool bOverlap = OverlapCapsuleBox(CapB, BoxA, OutContact);
		if (bOverlap && OutContact)
		{
			OutContact->Normal = -OutContact->Normal;
			OutContact->Point += OutContact->Normal * OutContact->PenetrationDepth;
		}
		return bOverlap;
	}
	else if (A.IsBox() && B.IsBox())
	{
		FOrientedBox BoxA = Collision::ToOrientedBox(A, PosA, RotA);
		FOrientedBox BoxB = Collision::ToOrientedBox(B, PosB, RotB);

		return OverlapBoxBox(BoxA, BoxB, OutContact);
	}

	return false;
}

bool Collision::OverlapCapsuleCapsule(const FCapsuleSegment& A, const FCapsuleSegment& B, FCollisionContact* OutContact)
{
	FVector C1, C2;
	float SumR = A.Radius + B.Radius;
	float DistSq = ClosestPointsSegmentSegment(A.A, A.B, B.A, B.B, C1, C2);
	bool bIsOverlap = DistSq <= SumR * SumR;
	if (bIsOverlap && OutContact)
	{
		constexpr float Epsilon = 1e-6f;
		FVector D = C1 - C2;
		float Len = D.Length();
		FVector Normal(0.0f, 0.0f, 1.0f);
		if (Len > Epsilon)
		{
			Normal = D / Len;
		}
		else
		{
			const FVector CenterDiff = (A.A + A.B - B.A - B.B) * 0.5f;
			const FVector Perp = (A.B - A.A).Cross(B.B - B.A);
			const float PerpLen = Perp.Length();
			const float CenterLen = CenterDiff.Length();
			if (PerpLen > Epsilon)
			{
				Normal = (Perp.Dot(CenterDiff) < 0.0f ? -Perp : Perp) / PerpLen;
			}
			else if (CenterLen > Epsilon)
			{
				Normal = CenterDiff / CenterLen;
			}
		}

		OutContact->Normal = Normal;
		OutContact->PenetrationDepth = A.Radius + B.Radius - Len;
		OutContact->Point = C2 + Normal * B.Radius;
	}

	return bIsOverlap;
}

bool Collision::OverlapCapsuleBox(const FCapsuleSegment& Capsule, const FOrientedBox& Box, FCollisionContact* OutContact)
{
	const FVector Dir = Capsule.B - Capsule.A;
	auto DistSqAt = [&](float T)
	{
		const FVector P = Capsule.A + Dir * T;
		const FVector D = P - ClosestPointOnBox(P, Box);
		return D.Dot(D);
	};

	float Lo = 0.0f;
	float Hi = 1.0f;
	for (int32 Iter = 0; Iter < 30; ++Iter)
	{
		const float M1 = Lo + (Hi - Lo) / 3.0f;
		const float M2 = Hi - (Hi - Lo) / 3.0f;
		if (DistSqAt(M1) <= DistSqAt(M2))
		{
			Hi = M2;
		}
		else
		{
			Lo = M1;
		}
	}

	const FVector P = Capsule.A + Dir * ((Lo + Hi) * 0.5f);
	const FVector Q = ClosestPointOnBox(P, Box);
	const FVector D = P - Q;
	const float DistSq = D.Dot(D);
	const bool bOverlap = DistSq <= Capsule.Radius * Capsule.Radius;
	if (!bOverlap || !OutContact)
	{
		return bOverlap;
	}

	const float Len = std::sqrt(DistSq);
	if (Len > 1e-6f)
	{
		OutContact->Normal = D / Len;
		OutContact->PenetrationDepth = Capsule.Radius - Len;
		OutContact->Point = Q;
		return true;
	}

	const FVector Local = P - Box.Center;
	int32 MinAxis = 0;
	float MinGap = FLT_MAX;
	float MinCoord = 0.0f;
	for (int32 i = 0; i < 3; ++i)
	{
		const float S = Local.Dot(Box.Axis[i]);
		const float Gap = Box.Extent[i] - std::abs(S);
		if (Gap < MinGap)
		{
			MinGap = Gap;
			MinAxis = i;
			MinCoord = S;
		}
	}

	OutContact->Normal = Box.Axis[MinAxis] * (MinCoord >= 0.0f ? 1.0f : -1.0f);
	OutContact->PenetrationDepth = Capsule.Radius + MinGap;
	OutContact->Point = P + OutContact->Normal * MinGap;
	return true;
}

bool Collision::OverlapBoxBox(const FOrientedBox& A, const FOrientedBox& B, FCollisionContact* OutContact)
{
	constexpr float Epsilon = 1e-6f;

	float R[3][3];
	float AbsR[3][3];
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			R[i][j] = A.Axis[i].Dot(B.Axis[j]);
			AbsR[i][j] = std::abs(R[i][j]) + Epsilon;
		}
	}

	const FVector Offset = B.Center - A.Center;
	const float T[3] = { Offset.Dot(A.Axis[0]), Offset.Dot(A.Axis[1]), Offset.Dot(A.Axis[2]) };

	const FVector& EA = A.Extent;
	const FVector& EB = B.Extent;

	float MinOverlap = FLT_MAX;
	FVector MinAxis = A.Axis[0];
	auto TrackMin = [&](float Overlap, const FVector& Axis)
	{
		if (OutContact && Overlap < MinOverlap)
		{
			MinOverlap = Overlap;
			MinAxis = Axis;
		}
	};

	for (int32 i = 0; i < 3; ++i)
	{
		const float RA = EA[i];
		const float RB = EB[0] * AbsR[i][0] + EB[1] * AbsR[i][1] + EB[2] * AbsR[i][2];
		if (std::abs(T[i]) > RA + RB)
		{
			return false;
		}
		TrackMin(RA + RB - std::abs(T[i]), A.Axis[i]);
	}

	for (int32 j = 0; j < 3; ++j)
	{
		const float RA = EA[0] * AbsR[0][j] + EA[1] * AbsR[1][j] + EA[2] * AbsR[2][j];
		const float RB = EB[j];
		const float Dist = T[0] * R[0][j] + T[1] * R[1][j] + T[2] * R[2][j];
		if (std::abs(Dist) > RA + RB)
		{
			return false;
		}
		TrackMin(RA + RB - std::abs(Dist), B.Axis[j]);
	}

	for (int32 i = 0; i < 3; ++i)
	{
		const int32 I1 = (i + 1) % 3;
		const int32 I2 = (i + 2) % 3;
		for (int32 j = 0; j < 3; ++j)
		{
			const int32 J1 = (j + 1) % 3;
			const int32 J2 = (j + 2) % 3;

			const float RA = EA[I1] * AbsR[I2][j] + EA[I2] * AbsR[I1][j];
			const float RB = EB[J1] * AbsR[i][J2] + EB[J2] * AbsR[i][J1];
			const float Dist = T[I2] * R[I1][j] - T[I1] * R[I2][j];
			if (std::abs(Dist) > RA + RB)
			{
				return false;
			}

			const float AxisLen = std::sqrt(std::max(0.0f, 1.0f - R[i][j] * R[i][j]));
			if (AxisLen > 1e-3f)
			{
				TrackMin((RA + RB - std::abs(Dist)) / AxisLen, A.Axis[i].Cross(B.Axis[j]) / AxisLen);
			}
		}
	}

	if (OutContact)
	{
		const FVector Normal = MinAxis.Dot(A.Center - B.Center) < 0.0f ? -MinAxis : MinAxis;
		OutContact->Normal = Normal;
		OutContact->PenetrationDepth = MinOverlap;
		OutContact->Point = ClosestPointOnBox(A.Center, B);
	}

	return true;
}

FVector Collision::ClosestPointOnSegment(const FVector& P, const FVector& A, const FVector& B)
{
	FVector BA = B - A;

	if (BA.Dot(BA) < SMALL_NUMBER)
	{
		return A;
	}

	FVector PA = P - A;

	float T = PA.Dot(BA) / BA.Dot(BA);
	T = std::clamp(T, 0.0f, 1.0f);

	FVector C = A + BA * T;

	return C;
}

float Collision::ClosestPointsSegmentSegment(const FVector& P1, const FVector& Q1, const FVector& P2, const FVector& Q2, FVector& OutC1, FVector& OutC2)
{
	FVector D1 = Q1 - P1;
	FVector D2 = Q2 - P2;
	FVector R = P1 - P2;

	float A = D1.Dot(D1);
	float E = D2.Dot(D2);
	float F = D2.Dot(R);

	if (A < SMALL_NUMBER && E < SMALL_NUMBER)
	{
		OutC1 = P1;
		OutC2 = P2;
		return R.Dot(R);
	}
	else if (A < SMALL_NUMBER)
	{
		OutC1 = P1;
		OutC2 = P2 + D2 * std::clamp(F / E, 0.0f, 1.0f);
		FVector D = OutC2 - OutC1;
		return D.Dot(D);
	}
	else if (E < SMALL_NUMBER)
	{
		float C = D1.Dot(R);
		OutC1 = P1 + D1 * std::clamp(-C / A, 0.0f, 1.0f);
		OutC2 = P2;
		FVector D = OutC2 - OutC1;
		return D.Dot(D);
	}
	else
	{
		float B = D1.Dot(D2);
		float C = D1.Dot(R);
		float Denom = A * E - B * B;

		float S = Denom > SMALL_NUMBER ? std::clamp((B * F - C * E) / Denom, 0.0f, 1.0f) : 0.0f;
		float T = (B * S + F) / E;

		if (T < 0.0f)
		{
			T = 0.0f;
			S = std::clamp(-C / A, 0.0f, 1.0f);
		}
		else if (T > 1.0f)
		{
			T = 1.0f;
			S = std::clamp((B - C) / A, 0.0f, 1.0f);
		}

		OutC1 = P1 + D1 * S;
		OutC2 = P2 + D2 * T;
		FVector D = OutC2 - OutC1;
		return D.Dot(D);
	}
}

FVector Collision::ClosestPointOnBox(const FVector& P, const FOrientedBox& Box)
{
	const FVector D = P - Box.Center;

	FVector Result = Box.Center;
	for (int32 i = 0; i < 3; ++i)
	{
		const float S = std::clamp(D.Dot(Box.Axis[i]), -Box.Extent[i], Box.Extent[i]);
		Result += Box.Axis[i] * S;
	}
	return Result;
}

Collision::FSweepResult Collision::Sweep(const FCollisionShape& Shape, const FVector& Start, const FVector& Delta, const FQuat& Rot,
	const TArray<FSweepTarget>& Targets)
{
	FSweepResult Result;

	const float DeltaLen = Delta.Length();
	if (DeltaLen < SMALL_NUMBER || Targets.Num() == 0)
	{
		return Result;
	}

	TArray<uint8> Ignored;
	Ignored.SetNum(Targets.Num());

	auto FirstOverlapAt = [&](float T, FCollisionContact* OutContact) -> int32
	{
		const FVector Pos = Start + Delta * T;
		for (int32 i = 0; i < static_cast<int32>(Targets.Num()); ++i)
		{
			if (Ignored[i])
			{
				continue;
			}
			const FSweepTarget& Target = Targets[i];
			if (Overlap(Shape, Pos, Rot, Target.Shape, Target.Pos, Target.Rot, OutContact))
			{
				return i;
			}
		}
		return -1;
	};

	for (int32 i = 0; i < static_cast<int32>(Targets.Num()); ++i)
	{
		FCollisionContact Contact;
		const FSweepTarget& Target = Targets[i];
		if (!Overlap(Shape, Start, Rot, Target.Shape, Target.Pos, Target.Rot, &Contact))
		{
			continue;
		}

		if (Delta.Dot(Contact.Normal) >= 0.0f)
		{
			Ignored[i] = 1;
			continue;
		}

		Result.bBlockingHit = true;
		Result.bStartPenetrating = true;
		Result.Time = 0.0f;
		Result.TargetIndex = i;
		Result.Contact = Contact;
		return Result;
	}

	float MinHalfSize = Shape.Radius;
	if (Shape.IsBox())
	{
		MinHalfSize = std::min({ Shape.BoxHalfExtent.X, Shape.BoxHalfExtent.Y, Shape.BoxHalfExtent.Z });
	}
	const int32 Steps = MinHalfSize > KINDA_SMALL_NUMBER
		? std::clamp(static_cast<int32>(std::ceil(DeltaLen / MinHalfSize)), 1, 64)
		: 1;

	for (int32 Step = 0; Step < Steps; ++Step)
	{
		float Lo = static_cast<float>(Step) / Steps;
		float Hi = static_cast<float>(Step + 1) / Steps;
		if (FirstOverlapAt(Hi, nullptr) < 0)
		{
			continue;
		}

		for (int32 Iter = 0; Iter < 10; ++Iter)
		{
			const float Mid = (Lo + Hi) * 0.5f;
			if (FirstOverlapAt(Mid, nullptr) >= 0)
			{
				Hi = Mid;
			}
			else
			{
				Lo = Mid;
			}
		}

		Result.bBlockingHit = true;
		Result.TargetIndex = FirstOverlapAt(Hi, &Result.Contact);
		Result.Time = std::max(0.0f, Lo - 1e-3f / DeltaLen);
		return Result;
	}

	return Result;
}
