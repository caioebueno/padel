// The placeholder court layout (what APadelCourt builds) matches CourtSpec (GDD section 2).
// Checked by ray-casting against the boxes, so this tests the colliders themselves.
#include "doctest.h"
#include "CourtLayout.h"

#include <set>
#include <string>
#include <vector>

using namespace Padel::Court;
using doctest::Approx;

namespace
{
	constexpr double Eps = 0.01;

	std::optional<FRayHit> Ray(FVec3 Start, FVec3 End)
	{
		return RaycastLayout(BuildCourtLayout(), Start, End);
	}

	ESurface SurfaceOf(const std::optional<FRayHit>& Hit)
	{
		return Hit ? Hit->Surface : ESurface::None;
	}

	// Casts from the centre line out through both side walls at (X, Z).
	std::vector<ESurface> Side(double X, double Z)
	{
		std::vector<ESurface> Out;
		for (const double S : { 1.0, -1.0 })
		{
			const auto Hit = Ray({ X, 0.0, Z }, { X, S * 20.0, Z });
			Out.push_back(SurfaceOf(Hit));
			if (Hit)
			{
				CHECK(Hit->Point.Y == Approx(S * 5.0).epsilon(Eps));
			}
		}
		return Out;
	}

	const std::vector<ESurface> BothGlass{ ESurface::Glass, ESurface::Glass };
	const std::vector<ESurface> BothMesh{ ESurface::Mesh, ESurface::Mesh };
	const std::vector<ESurface> BothOpen{ ESurface::None, ESurface::None };
}

TEST_CASE("Court.Layout: every collider has a surface and names are unique")
{
	const auto& Layout = BuildCourtLayout();
	std::set<std::string> Names;
	int Colliders = 0;
	for (const FCourtBox& Box : Layout)
	{
		CHECK_MESSAGE(Names.insert(Box.Name).second, "duplicate name ", Box.Name);
		CHECK(Box.Size.X > 0.0);
		CHECK(Box.Size.Y > 0.0);
		CHECK(Box.Size.Z > 0.0);
		if (Box.bCollides)
		{
			++Colliders;
			CHECK_MESSAGE(Box.Surface != ESurface::None, Box.Name);
		}
		else
		{
			CHECK_MESSAGE(Box.Surface == ESurface::None, Box.Name);
			CHECK(Box.bVisible);
		}
	}
	CHECK(Colliders > 20);
}

TEST_CASE("Court.Layout: ground is at Z = 0 across the court")
{
	for (const auto& [X, Y] : std::vector<std::pair<double, double>>{ { 3, 0 }, { 9.9, 4.9 }, { -9.9, -4.9 }, { -6, 2 } })
	{
		const auto Hit = Ray({ X, Y, 2.0 }, { X, Y, -1.0 });
		REQUIRE(Hit);
		CHECK(Hit->Surface == ESurface::Ground);
		CHECK(Hit->Box->Zone == EZone::Court);
		CHECK(Hit->Point.Z == Approx(0.0).epsilon(Eps));
	}
}

TEST_CASE("Court.Layout: run-off floor surrounds the court")
{
	for (const auto& [X, Y] : std::vector<std::pair<double, double>>{ { 0, 7 }, { 5, -7 }, { 11, 0 }, { -11.5, 8 } })
	{
		const auto Hit = Ray({ X, Y, 2.0 }, { X, Y, -1.0 });
		REQUIRE(Hit);
		CHECK(Hit->Box->Zone == EZone::Runoff);
		CHECK(Hit->Point.Z == Approx(0.0).epsilon(Eps));
	}
}

TEST_CASE("Court.Layout: back walls are 3 m glass then mesh to 4 m")
{
	for (const double H : { 1.0, -1.0 })
	{
		const auto Glass = Ray({ 0, 0, 1.5 }, { H * 20.0, 0, 1.5 });
		REQUIRE(Glass);
		CHECK(Glass->Surface == ESurface::Glass);
		CHECK(Glass->Point.X == Approx(H * 10.0).epsilon(Eps));
		CHECK(SurfaceOf(Ray({ 0, 3, 2.95 }, { H * 20.0, 3, 2.95 })) == ESurface::Glass);
		CHECK(SurfaceOf(Ray({ 0, -3, 3.05 }, { H * 20.0, -3, 3.05 })) == ESurface::Mesh);
		CHECK(SurfaceOf(Ray({ 0, 0, 3.95 }, { H * 20.0, 0, 3.95 })) == ESurface::Mesh);
		CHECK(SurfaceOf(Ray({ 0, 0, 4.1 }, { H * 20.0, 0, 4.1 })) == ESurface::None);
	}
}

TEST_CASE("Court.Layout: side walls follow the regulation layout")
{
	for (const double H : { 1.0, -1.0 })
	{
		// First 2 m from the back wall: glass to 3 m, mesh to 4 m.
		CHECK(Side(H * 9.0, 2.9) == BothGlass);
		CHECK(Side(H * 9.0, 3.5) == BothMesh);
		CHECK(Side(H * 9.0, 4.1) == BothOpen);
		// The step: glass to 2 m, mesh to 3 m.
		CHECK(Side(H * 7.0, 1.9) == BothGlass);
		CHECK(Side(H * 7.0, 2.5) == BothMesh);
		CHECK(Side(H * 7.0, 3.1) == BothOpen);
		// The 2 m exit at the net end of the side glass, open to full height.
		CHECK(Side(H * 5.0, 1.0) == BothOpen);
		CHECK(Side(H * 5.0, 2.5) == BothOpen);
		CHECK(Side(H * 4.1, 1.0) == BothOpen);
		CHECK(Side(H * 5.9, 1.0) == BothOpen);
		// Mesh from the exit to the net, 3 m high.
		CHECK(Side(H * 2.0, 1.0) == BothMesh);
		CHECK(Side(H * 0.5, 2.9) == BothMesh);
		CHECK(Side(H * 2.0, 3.1) == BothOpen);
	}
}

TEST_CASE("Court.Layout: side walls agree with CourtSpec everywhere")
{
	// Sweep the side wall plane and compare the layout with the analytic spec.
	for (double X = -9.95; X < 10.0; X += 0.1)
	{
		for (double Z = 0.05; Z < 4.5; Z += 0.1)
		{
			const auto Hit = Ray({ X, 0, Z }, { X, 20.0, Z });
			CHECK_MESSAGE(SurfaceOf(Hit) == SideWallSurfaceAt(X, Z), "x=", X, " z=", Z);
		}
	}
}

TEST_CASE("Court.Layout: net is 0.88 in the middle and 0.92 at the posts")
{
	CHECK(SurfaceOf(Ray({ 3, 0, 0.86 }, { -3, 0, 0.86 })) == ESurface::Net);
	CHECK(SurfaceOf(Ray({ 3, 0.1, 0.90 }, { -3, 0.1, 0.90 })) == ESurface::None);
	CHECK(SurfaceOf(Ray({ 3, 4.6, 0.90 }, { -3, 4.6, 0.90 })) == ESurface::Net);
	CHECK(SurfaceOf(Ray({ 3, 4.6, 0.95 }, { -3, 4.6, 0.95 })) == ESurface::None);
	const auto Hit = Ray({ 3, -2, 0.3 }, { -3, -2, 0.3 });
	REQUIRE(Hit);
	CHECK(Hit->Surface == ESurface::Net);
	CHECK(Hit->Point.X == Approx(0.0).epsilon(0.05));
}

TEST_CASE("Court.Layout: net segment tops follow the sag")
{
	int Segments = 0;
	for (const FCourtBox& Box : BuildCourtLayout())
	{
		if (Box.Name.rfind("NetSegment", 0) == 0)
		{
			CHECK(Box.Centre.Z + Box.Size.Z / 2.0 == Approx(NetHeightAt(Box.Centre.Y)));
			++Segments;
		}
	}
	CHECK(Segments == 20);
}

TEST_CASE("Court.Layout: ceiling is at 8 m")
{
	const auto Hit = Ray({ 1, 1, 1 }, { 1, 1, 20 });
	REQUIRE(Hit);
	CHECK(Hit->Surface == ESurface::Ceiling);
	CHECK(Hit->Point.Z == Approx(8.0).epsilon(Eps));
}

TEST_CASE("Court.Layout: service lines are painted at 6.95")
{
	int Found = 0;
	for (const FCourtBox& Box : BuildCourtLayout())
	{
		if (Box.Name == "ServiceLineA")
		{
			CHECK(Box.Centre.X == Approx(6.95));
			++Found;
		}
		if (Box.Name == "ServiceLineB")
		{
			CHECK(Box.Centre.X == Approx(-6.95));
			++Found;
		}
	}
	CHECK(Found == 2);
}
