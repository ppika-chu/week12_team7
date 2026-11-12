#pragma once

#include "ShapeComponent.h"
#include "BoxComponent.generated.h"

UCLASS()
class UBoxComponent : public UShapeComponent
{
	GENERATED_BODY()

public:
	virtual FCollisionShape GetCollisionShape() const override;
	virtual FBox CalcLocalBounds() const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

	void SetBoxExtent(const FVector& InExtent);
	FVector GetScaledBoxExtent() const;

protected:
	UPROPERTY(EditAnywhere)
	FVector BoxExtent{ 0.5f, 0.5f, 0.5f };
};
