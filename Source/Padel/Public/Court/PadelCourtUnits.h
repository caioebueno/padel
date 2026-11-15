// The one place where PadelCore's metres become Unreal's centimetres (CLAUDE.md,
// "Coordinates and units"). Both use the same axes: X along the length, Y across, Z up.

#pragma once

#include "CoreMinimal.h"
#include "CourtSpec.h"

namespace Padel
{
	/** Simulation position in metres -> world position in Unreal units (cm). */
	inline FVector MetresToWorld(const FVector& Metres)
	{
		return Metres * Court::UnrealUnitsPerMetre;
	}

	/** World position in Unreal units (cm) -> simulation position in metres. */
	inline FVector WorldToMetres(const FVector& World)
	{
		return World / Court::UnrealUnitsPerMetre;
	}
}
