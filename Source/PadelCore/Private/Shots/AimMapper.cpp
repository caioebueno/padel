#include "Shots/AimMapper.h"

#include "CourtSpec.h"

namespace Padel::Shots
{
	double FAimMapper::Depth(const FVec2& Aim, const FShotParams& Shot)
	{
		const FVec2 A = Limit2(Aim, 1.0);
		return A.Y >= 0.0 ? Lerp(Shot.DepthNeutralM, Shot.DepthDeepM, A.Y) : Lerp(Shot.DepthNeutralM, Shot.DepthShortM, -A.Y);
	}

	FVec3 FAimMapper::Target(const FVec2& Aim, const FShotParams& Shot, double TeamSign)
	{
		const FVec2 A = Limit2(Aim, 1.0);
		// The opponents' half is the other sign of X.
		const FVec3 Across = Scale(RightAxis(TeamSign), A.X * Shot.LateralMaxM);
		return Vec(-TeamSign * Depth(Aim, Shot), Across.Y, 0.0);
	}

	FVec3 FAimMapper::ServeTarget(const FVec2& Aim, const FShotParams& Shot, double TeamSign, double ServerY)
	{
		const FVec2 A = Limit2(Aim, 1.0);
		// Diagonal: the box on the other side of the centre line, in world Y.
		const double ServerSide = ServerY != 0.0 ? Sign(ServerY) : RightAxis(TeamSign).Y;
		const double BoxCentreY = -ServerSide * Court::HalfWidth / 2.0;
		const double Y = BoxCentreY + RightAxis(TeamSign).Y * A.X * Shot.LateralMaxM;
		return Vec(-TeamSign * Depth(Aim, Shot), Y, 0.0);
	}
}
