// Tuning for the stand-in practice ball: the single row of Data/PracticeBall.json.

#pragma once

// Determinism pragmas (no FMA contraction) for the inline maths below; see PadelCoreApi.h.
#include "PadelCoreApi.h"

#include <string>
#include <vector>

namespace Padel::Shots
{
	struct FPracticeBallParams
	{
		double GravityMps2 = 0.0;
		double RadiusM = 0.0;
		// GDD 2 surface table: restitution on the normal speed, and the fraction of the
		// tangential speed kept on a bounce.
		double GroundRestitution = 0.0;
		double GroundTangentKeep = 0.0;
		double GlassRestitution = 0.0;
		double GlassTangentKeep = 0.0;
		double MeshRestitution = 0.0;
		double MeshTangentKeep = 0.0;
		double NetRestitution = 0.0;
		// How long a dead ball stays visible before the machine feeds the next one.
		double DeadLingerS = 0.0;
		// Fixed sub-step (CLAUDE.md: 120 Hz).
		double StepS = 0.0;

		std::vector<std::string> Validate() const
		{
			std::vector<std::string> E;
			if (!(GravityMps2 > 0.0)) E.push_back("GravityMps2 must be > 0");
			if (!(RadiusM > 0.0)) E.push_back("RadiusM must be > 0");
			if (!(GroundRestitution > 0.0)) E.push_back("GroundRestitution must be > 0");
			if (!(GlassRestitution > 0.0)) E.push_back("GlassRestitution must be > 0");
			if (!(StepS > 0.0)) E.push_back("StepS must be > 0");
			return E;
		}
	};
}
