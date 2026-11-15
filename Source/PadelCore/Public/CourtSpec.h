// Real padel court measurements and the shared coordinate system (GDD section 2).
//
// Pure C++20 with no Unreal headers, so it builds inside the PadelCore module and
// in the standalone native test suite (Tests/Native) that CI runs on every PR.
//
// Coordinates (simulation): metres, right-handed numbers on Unreal's axes:
//   X  along the length, -10 .. +10. Team A starts on the +X half.
//   Y  across the width,  -5 ..  +5.
//   Z  up, ground at 0.
//   The net is the plane X = 0.
// The Unreal world uses the same axes in centimetres: multiply by UnrealUnitsPerMetre.
//
// These are regulation measurements, not tuning numbers, so they live in code.

#pragma once

namespace Padel::Court
{
	inline constexpr double UnrealUnitsPerMetre = 100.0;

	// --- Playing area (inside measurements) ---
	inline constexpr double Length = 20.0;
	inline constexpr double Width = 10.0;
	inline constexpr double HalfLength = Length / 2.0;
	inline constexpr double HalfWidth = Width / 2.0;

	// --- Net ---
	inline constexpr double NetHeightCentre = 0.88;
	inline constexpr double NetHeightPosts = 0.92;
	// Regulation maximum post height.
	inline constexpr double NetPostHeight = 1.05;

	// --- Lines (lines count as in) ---
	inline constexpr double LineWidth = 0.05;
	// Service line distance from the net.
	inline constexpr double ServiceLineFromNet = 6.95;
	// How far the centre service line runs past the service line toward the back wall.
	inline constexpr double CentreLineExtension = 0.20;

	// --- Walls ---
	// Back wall: glass, full width.
	inline constexpr double BackGlassHeight = 3.0;
	// Metal mesh above the back glass and the first side panel, up to this total height.
	inline constexpr double FenceTotalHeight = 4.0;
	// Side glass: first panel from each back wall.
	inline constexpr double SideGlassFullLength = 2.0;
	inline constexpr double SideGlassFullHeight = 3.0;
	// Side glass: the "step" panel after it.
	inline constexpr double SideGlassStepLength = 2.0;
	inline constexpr double SideGlassStepHeight = 2.0;
	// Mesh on the rest of the side walls, and above the step panel.
	inline constexpr double SideMeshHeight = 3.0;
	// Exit opening: one per side per half, at the net end of the side glass, open to full height.
	// FIP allows 0.72 to 2.20 depending on the court variant; Caio picked 2.0.
	inline constexpr double ExitWidth = 2.0;
	// Indoor slice court: hitting the ceiling is a fault.
	inline constexpr double CeilingHeight = 8.0;

	// --- Placeholder build sizes (not regulation; only for collision volumes) ---
	inline constexpr double WallThickness = 0.12;
	inline constexpr double GroundThickness = 0.2;
	// Run-off floor outside the side walls and behind the back walls, for out-of-court play.
	inline constexpr double RunoffSide = 4.0;
	inline constexpr double RunoffBack = 2.0;

	// --- Derived X positions (distance from the net, positive half) ---
	inline constexpr double ServiceLineX = ServiceLineFromNet;
	// Where the full-height side glass ends and the step begins.
	inline constexpr double SideGlassStepStartX = HalfLength - SideGlassFullLength;
	// Where the step glass ends (net end of the side glass).
	inline constexpr double SideGlassEndX = SideGlassStepStartX - SideGlassStepLength;
	// The exit spans ExitNearX .. ExitFarX on each half.
	inline constexpr double ExitFarX = SideGlassEndX;
	inline constexpr double ExitNearX = SideGlassEndX - ExitWidth;

	enum class EHalf : int
	{
		A = 1,  // +X
		B = -1, // -X
	};

	// Surface names match the ball_bounced event values in GDD 3.4.
	enum class ESurface : int
	{
		None,
		Ground,
		Glass,
		Mesh,
		Net,
		Ceiling,
	};

	inline constexpr const char* SurfaceName(ESurface Surface)
	{
		switch (Surface)
		{
		case ESurface::Ground: return "ground";
		case ESurface::Glass: return "glass";
		case ESurface::Mesh: return "mesh";
		case ESurface::Net: return "net";
		case ESurface::Ceiling: return "ceiling";
		default: return "none";
		}
	}

	inline constexpr double Abs(double Value)
	{
		return Value < 0.0 ? -Value : Value;
	}

	inline constexpr double Clamp01(double Value)
	{
		return Value < 0.0 ? 0.0 : (Value > 1.0 ? 1.0 : Value);
	}

	// Net top height at a given Y across the court. The tape sags in a parabola from
	// 0.92 at the posts to 0.88 in the centre.
	inline constexpr double NetHeightAt(double Y)
	{
		const double T = Clamp01(Abs(Y) / HalfWidth);
		return NetHeightCentre + (NetHeightPosts - NetHeightCentre) * T * T;
	}

	// True when a ground point is inside the playing area. Lines count as in, and the
	// outer edge is the inside face of the walls.
	inline constexpr bool IsInsideCourt(double X, double Y)
	{
		return Abs(X) <= HalfLength && Abs(Y) <= HalfWidth;
	}

	// Which half an X position is on. The net plane itself counts as half A.
	inline constexpr EHalf HalfOf(double X)
	{
		return X >= 0.0 ? EHalf::A : EHalf::B;
	}

	// Height of the glass part of the side wall at a given X; 0 where the side wall is
	// mesh or an exit.
	inline constexpr double SideGlassHeightAt(double X)
	{
		const double AX = Abs(X);
		if (AX > HalfLength)
		{
			return 0.0;
		}
		if (AX >= SideGlassStepStartX)
		{
			return SideGlassFullHeight;
		}
		if (AX >= SideGlassEndX)
		{
			return SideGlassStepHeight;
		}
		return 0.0;
	}

	// True when X falls inside an exit opening in the side walls.
	inline constexpr bool IsExitAt(double X)
	{
		const double AX = Abs(X);
		return AX > ExitNearX && AX < ExitFarX;
	}

	// Which surface of the side wall a point at (X, Z) on the wall plane would hit.
	inline constexpr ESurface SideWallSurfaceAt(double X, double Z)
	{
		const double AX = Abs(X);
		if (AX > HalfLength || Z < 0.0 || IsExitAt(X))
		{
			return ESurface::None;
		}
		if (Z < SideGlassHeightAt(X))
		{
			return ESurface::Glass;
		}
		const double MeshTop = AX >= SideGlassStepStartX ? FenceTotalHeight : SideMeshHeight;
		return Z < MeshTop ? ESurface::Mesh : ESurface::None;
	}

	// Which surface of a back wall a point at height Z would hit.
	inline constexpr ESurface BackWallSurfaceAt(double Z)
	{
		if (Z < 0.0)
		{
			return ESurface::None;
		}
		if (Z < BackGlassHeight)
		{
			return ESurface::Glass;
		}
		return Z < FenceTotalHeight ? ESurface::Mesh : ESurface::None;
	}
}
