// Small value types for the AI, in simulation metres on Unreal's axes (CourtSpec.h).
// Plain C++ so the AI builds in PadelCore and in the native tests.

#pragma once

// Determinism pragmas (no FMA contraction) for the inline maths below; see PadelCoreApi.h.
#include "PadelCoreApi.h"

#include <cmath>

namespace Padel::AI
{
	struct FAiVec
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;

		constexpr FAiVec operator+(const FAiVec& O) const { return { X + O.X, Y + O.Y, Z + O.Z }; }
		constexpr FAiVec operator-(const FAiVec& O) const { return { X - O.X, Y - O.Y, Z - O.Z }; }
		constexpr FAiVec operator*(double S) const { return { X * S, Y * S, Z * S }; }
		FAiVec& operator+=(const FAiVec& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }

		double Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }
		// Distance on the ground plane.
		double Length2D() const { return std::sqrt(X * X + Y * Y); }
	};

	// A 2D vector. Its meaning depends on the frame (see AiTeamFrame.h).
	struct FAiVec2
	{
		double X = 0.0;
		double Y = 0.0;

		constexpr FAiVec2 operator+(const FAiVec2& O) const { return { X + O.X, Y + O.Y }; }
		constexpr FAiVec2 operator-(const FAiVec2& O) const { return { X - O.X, Y - O.Y }; }
		constexpr FAiVec2 operator*(double S) const { return { X * S, Y * S }; }

		double Length() const { return std::sqrt(X * X + Y * Y); }
	};

	inline constexpr double ClampD(double V, double Lo, double Hi)
	{
		return V < Lo ? Lo : (V > Hi ? Hi : V);
	}

	inline constexpr double LerpD(double A, double B, double T)
	{
		return A + (B - A) * T;
	}

	inline constexpr double SignOf(double V)
	{
		return V < 0.0 ? -1.0 : 1.0;
	}

	// GDD 6.1: ratings run 1..99. Maps a rating onto [At1, At99].
	inline constexpr double ByRating(int Rating, double At1, double At99)
	{
		return LerpD(At1, At99, ClampD((Rating - 1) / 98.0, 0.0, 1.0));
	}
}
