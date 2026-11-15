// Small vector helpers for the shot system, on Court::FVec3 (metres, Unreal axes).
// Named functions rather than operators so they can't clash with whatever vector
// maths the ball engine adds to FVec3.

#pragma once

#include "CourtLayout.h"

#include "PadelCoreApi.h"

#include <cmath>

namespace Padel::Shots
{
	using Court::FVec3;

	// Team-relative stick or AI move direction: X = the player's right, Y = toward the net.
	struct FVec2
	{
		double X = 0.0;
		double Y = 0.0;
	};

	inline constexpr double Tau = 6.283185307179586;

	inline FVec3 Vec(double X, double Y, double Z) { return FVec3{ X, Y, Z }; }
	inline FVec3 Add(const FVec3& A, const FVec3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
	inline FVec3 Sub(const FVec3& A, const FVec3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
	inline FVec3 Scale(const FVec3& A, double S) { return { A.X * S, A.Y * S, A.Z * S }; }
	inline double DotProduct(const FVec3& A, const FVec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	inline double Length(const FVec3& A) { return std::sqrt(DotProduct(A, A)); }
	// Length on the ground plane (ignores Z).
	inline double LengthXY(const FVec3& A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }
	inline double DistanceXY(const FVec3& A, const FVec3& B) { return LengthXY(Sub(A, B)); }

	inline double Length2(const FVec2& A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }
	inline FVec2 Limit2(const FVec2& A, double MaxLength)
	{
		const double L = Length2(A);
		return L > MaxLength && L > 0.0 ? FVec2{ A.X * MaxLength / L, A.Y * MaxLength / L } : A;
	}

	inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	inline double InverseLerp(double A, double B, double V) { return B == A ? 0.0 : (V - A) / (B - A); }
	inline double Sign(double V) { return V > 0.0 ? 1.0 : (V < 0.0 ? -1.0 : 0.0); }

	// Unreal is left-handed with Z up: a player facing -X has -Y on their right.
	// TeamSign is the hitter's half: +1 for Team A (+X), -1 for Team B.
	inline FVec3 ForwardAxis(double TeamSign) { return { -TeamSign, 0.0, 0.0 }; }
	inline FVec3 RightAxis(double TeamSign) { return { 0.0, -TeamSign, 0.0 }; }

	// Team-relative ground vector -> world.
	inline FVec3 TeamToWorld(const FVec2& V, double TeamSign)
	{
		return Add(Scale(RightAxis(TeamSign), V.X), Scale(ForwardAxis(TeamSign), V.Y));
	}
}
