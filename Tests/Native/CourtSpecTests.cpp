// GDD section 2: court measurements and helpers.
#include "doctest.h"
#include "CourtSpec.h"

#include <string>

using namespace Padel::Court;
using doctest::Approx;

TEST_CASE("Court.Spec: playing area is 20 by 10")
{
	CHECK(Length == 20.0);
	CHECK(Width == 10.0);
	CHECK(HalfLength == 10.0);
	CHECK(HalfWidth == 5.0);
}

TEST_CASE("Court.Spec: net is 0.88 in the centre and 0.92 at the posts")
{
	CHECK(NetHeightAt(0.0) == Approx(0.88));
	CHECK(NetHeightAt(5.0) == Approx(0.92));
	CHECK(NetHeightAt(-5.0) == Approx(0.92));
	// Clamped outside the court.
	CHECK(NetHeightAt(7.0) == Approx(0.92));
	double Previous = NetHeightAt(0.0);
	for (int Step = 1; Step <= 10; ++Step)
	{
		const double Height = NetHeightAt(Step * 0.5);
		CHECK(Height > Previous);
		Previous = Height;
	}
	// Usable at compile time.
	static_assert(NetHeightAt(0.0) == NetHeightCentre);
}

TEST_CASE("Court.Spec: service line is 6.95 from the net and 3.05 from the back wall")
{
	CHECK(ServiceLineX == 6.95);
	CHECK(HalfLength - ServiceLineX == Approx(3.05));
}

TEST_CASE("Court.Spec: side glass is a 3 m panel then a 2 m step")
{
	CHECK(SideGlassHeightAt(9.5) == 3.0);
	CHECK(SideGlassHeightAt(-9.5) == 3.0);
	CHECK(SideGlassHeightAt(7.0) == 2.0);
	CHECK(SideGlassHeightAt(-7.0) == 2.0);
	CHECK(SideGlassHeightAt(5.0) == 0.0);
	CHECK(SideGlassHeightAt(1.0) == 0.0);
}

TEST_CASE("Court.Spec: 2 m exit at the net end of the side glass")
{
	CHECK(ExitWidth == 2.0);
	CHECK(ExitFarX == 6.0);
	CHECK(ExitNearX == 4.0);
	CHECK(IsExitAt(5.0));
	CHECK(IsExitAt(-5.0));
	CHECK_FALSE(IsExitAt(7.0));
	CHECK_FALSE(IsExitAt(2.0));
}

TEST_CASE("Court.Spec: wall surfaces by position")
{
	CHECK(BackWallSurfaceAt(1.0) == ESurface::Glass);
	CHECK(BackWallSurfaceAt(3.5) == ESurface::Mesh);
	CHECK(BackWallSurfaceAt(4.5) == ESurface::None);
	CHECK(SideWallSurfaceAt(9.0, 2.9) == ESurface::Glass);
	CHECK(SideWallSurfaceAt(9.0, 3.5) == ESurface::Mesh);
	CHECK(SideWallSurfaceAt(7.0, 2.5) == ESurface::Mesh);
	CHECK(SideWallSurfaceAt(7.0, 3.5) == ESurface::None);
	CHECK(SideWallSurfaceAt(5.0, 1.0) == ESurface::None);
	CHECK(SideWallSurfaceAt(-2.0, 1.0) == ESurface::Mesh);
}

TEST_CASE("Court.Spec: inside court includes the boundary; Team A is on +X")
{
	CHECK(IsInsideCourt(0.0, 0.0));
	CHECK(IsInsideCourt(10.0, 5.0));
	CHECK_FALSE(IsInsideCourt(10.01, 0.0));
	CHECK_FALSE(IsInsideCourt(0.0, -5.01));
	CHECK(HalfOf(3.0) == EHalf::A);
	CHECK(HalfOf(-3.0) == EHalf::B);
}

TEST_CASE("Court.Spec: surface names match the GDD 3.4 event values")
{
	CHECK(std::string(SurfaceName(ESurface::Ground)) == "ground");
	CHECK(std::string(SurfaceName(ESurface::Glass)) == "glass");
	CHECK(std::string(SurfaceName(ESurface::Mesh)) == "mesh");
	CHECK(std::string(SurfaceName(ESurface::Net)) == "net");
	CHECK(std::string(SurfaceName(ESurface::Ceiling)) == "ceiling");
}
