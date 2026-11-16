#include "EnginePCH.h"
#include "Distributions/Distribution.h"

void UDistribution::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);
	if (bIsLoading)
	{
		MarkDirty();
	}
}

void UDistribution::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);
	if (Record.GetUnderlyingArchive().IsLoading())
	{
		MarkDirty();
	}
}
