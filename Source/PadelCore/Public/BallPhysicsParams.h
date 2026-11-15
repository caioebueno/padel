// Tuning for the ball integrator. The values live in Data/Physics.json (one DataTable
// row named "Default"); nothing here has a gameplay default on purpose, so a missing
// value fails loudly instead of hiding (CLAUDE.md: no hard-coded tuning numbers).

#pragma once

#include "CourtSpec.h"

#include <string>

namespace Padel
{
	// Bounce response of one surface (GDD 2, "Surfaces").
	struct FSurfaceParams
	{
		// Share of the normal speed kept after the bounce.
		double Restitution = 0.0;
		// Coulomb friction at the contact. Friction is what turns spin into a kick
		// (topspin) or a skid (slice), and what the glass mostly lacks.
		double Friction = 0.0;
		// Random deflection of the outgoing direction, radians either way (mesh only).
		double DeflectionMax = 0.0;
		// Random change of the outgoing speed, as a fraction either way (0.2 = ±20%).
		double SpeedVariation = 0.0;
	};

	struct FBallPhysicsParams
	{
		// m/s².
		double Gravity = 0.0;
		// Quadratic drag: a = -DragCoefficient × |v| × v (1/m), i.e. 0.5 ρ Cd A / m.
		double DragCoefficient = 0.0;
		// Magnus effect: a = MagnusCoefficient × (spin × v).
		double MagnusCoefficient = 0.0;
		// Spin lost to the air, fraction per second.
		double SpinDecayPerSecond = 0.0;
		// Moment of inertia as a fraction of m r² (2/3 for a thin hollow sphere).
		double InertiaFactor = 0.0;
		// Integration sub-steps per 120 Hz simulation tick.
		int SubStepsPerTick = 0;
		// A ground bounce slower than this (m/s, vertical) turns into rolling.
		double RollingThreshold = 0.0;
		// Deceleration of a rolling ball, m/s².
		double RollingDeceleration = 0.0;
		// Share of the speed kept when the ball clips the net tape and carries on.
		double NetTapeSpeedKeep = 0.0;
		// Referee watchdog: a live ball slower than StuckSpeed (m/s) for StuckSeconds is stuck.
		double StuckSpeed = 0.0;
		double StuckSeconds = 0.0;
		// Referee watchdog: the longest a point may stay live without a hit, seconds.
		double MaxFlightSeconds = 0.0;

		FSurfaceParams Ground;
		FSurfaceParams Glass;
		FSurfaceParams Mesh;
		FSurfaceParams Net;
		FSurfaceParams Ceiling;

		const FSurfaceParams& Surface(Court::ESurface Which) const;

		// True when every value is usable; otherwise names the first bad field.
		bool Validate(std::string& Error) const;
	};

	// Reads Data/Physics.json text: either one object, or a DataTable-style array of rows
	// where the row whose "Name" is "Default" (or the first row) is used. Every field is
	// required. Surface fields are prefixed by the surface: GroundRestitution, MeshFriction...
	bool ParseBallPhysicsJson(const std::string& Text, FBallPhysicsParams& Out, std::string& Error);
}
