// PadelCore's FVec3 (metres) <-> Unreal FVector (cm), going through PadelCourtUnits.h
// so the metres-to-centimetres factor stays in one place (CLAUDE.md).

#pragma once

#include "CoreMinimal.h"
#include "Court/PadelCourtUnits.h"
#include "Shots/ShotMath.h"

namespace Padel
{
	inline FVector ToWorld(const Court::FVec3& Metres)
	{
		return MetresToWorld(FVector(Metres.X, Metres.Y, Metres.Z));
	}

	inline Court::FVec3 ToMetres(const FVector& World)
	{
		const FVector M = WorldToMetres(World);
		return Court::FVec3{ M.X, M.Y, M.Z };
	}
}
