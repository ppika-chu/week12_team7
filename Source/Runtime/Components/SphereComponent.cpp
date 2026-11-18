#include "EnginePCH.h"
#include "SphereComponent.h"
#include "Collision/CollisionShape.h"

FCollisionShape USphereComponent::GetCollisionShape() const
{
	return FCollisionShape::MakeSphere(GetScaledSphereRadius());
}

FBox USphereComponent::CalcLocalBounds() const
{
	return FBox{ FVector{ -SphereRadius, -SphereRadius, -SphereRadius },
		FVector{ SphereRadius, SphereRadius, SphereRadius } };
}

void USphereComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	const float PreviousRadius = SphereRadius;
	SphereRadius = std::max(0.0f, SphereRadius);
	if (Property.Name == "SphereRadius" || SphereRadius != PreviousRadius)
	{
		MarkBlockingShapeBoundsDirty();
	}
}

void USphereComponent::SetSphereRadius(float InRadius)
{
	SphereRadius = std::max(0.0f, InRadius);
	MarkBlockingShapeBoundsDirty();
}

float USphereComponent::GetScaledSphereRadius() const
{
	return SphereRadius * GetShapeScale();
}
