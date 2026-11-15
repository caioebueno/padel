// Small vector maths for the simulation, on CourtLayout's FVec3 (metres). Kept free of
// Unreal's FVector so PadelCore builds standalone; the Padel module converts at its
// boundary (and from metres to centimetres) in one place.

#pragma once

#include "CourtLayout.h"

#include <cmath>

namespace Padel::Court
{
	inline constexpr FVec3 operator+(const FVec3& A, const FVec3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
	inline constexpr FVec3 operator-(const FVec3& A, const FVec3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
	inline constexpr FVec3 operator-(const FVec3& A) { return { -A.X, -A.Y, -A.Z }; }
	inline constexpr FVec3 operator*(const FVec3& A, double S) { return { A.X * S, A.Y * S, A.Z * S }; }
	inline constexpr FVec3 operator*(double S, const FVec3& A) { return A * S; }
	inline constexpr FVec3 operator/(const FVec3& A, double S) { return { A.X / S, A.Y / S, A.Z / S }; }
	inline FVec3& operator+=(FVec3& A, const FVec3& B) { A = A + B; return A; }
	inline FVec3& operator-=(FVec3& A, const FVec3& B) { A = A - B; return A; }
	inline FVec3& operator*=(FVec3& A, double S) { A = A * S; return A; }
	inline constexpr bool operator==(const FVec3& A, const FVec3& B) { return A.X == B.X && A.Y == B.Y && A.Z == B.Z; }

	inline constexpr double Dot(const FVec3& A, const FVec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }

	inline constexpr FVec3 Cross(const FVec3& A, const FVec3& B)
	{
		return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X };
	}

	inline double Size(const FVec3& A) { return std::sqrt(Dot(A, A)); }
	inline double SizeXY(const FVec3& A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }

	inline FVec3 SafeNormal(const FVec3& A, const FVec3& Fallback = { 1.0, 0.0, 0.0 })
	{
		const double L = Size(A);
		return L > 1e-12 ? A / L : Fallback;
	}

	// Rotates V around a unit Axis by Angle radians (Rodrigues).
	inline FVec3 RotateAround(const FVec3& V, const FVec3& Axis, double Angle)
	{
		const double C = std::cos(Angle);
		const double S = std::sin(Angle);
		return V * C + Cross(Axis, V) * S + Axis * (Dot(Axis, V) * (1.0 - C));
	}

	inline constexpr FVec3 UpVector{ 0.0, 0.0, 1.0 };
}

namespace Padel
{
	using FVec3 = Court::FVec3;
}
