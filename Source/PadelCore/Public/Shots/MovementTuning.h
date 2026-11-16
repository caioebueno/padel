// Player movement constants (GDD 6.1, 6.3): the single row of Data/Movement.json.

#pragma once

#include "Shots/ShotMath.h"

#include <string>
#include <vector>

namespace Padel::Shots
{
	struct PADELCORE_API FMovementTuning
	{
		// GDD 6.1 Speed and Acceleration. TimeToTop is top speed over the push from standing
		// (a0 = TopSpeed / TimeToTop): the time to top speed only when AccelFalloff is 0.
		double TopSpeedAt1Mps = 0.0;
		double TopSpeedAt99Mps = 0.0;
		double TimeToTopAt1S = 0.0;
		double TimeToTopAt99S = 0.0;
		// Braking is quicker than accelerating.
		double BrakeMult = 0.0;
		// Acceleration falls as speed rises: a = a0 * (1 - AccelFalloff * v / vTop).
		// 0 keeps it constant; 1 falls to nothing at top speed (BIOMECHANICS_MOVEMENT 5.1).
		double AccelFalloff = 0.0;

		// GDD 6.3 Direction relative to the net (BIOMECHANICS_MOVEMENT 5.2). Fractions of
		// top speed for a player square to the net; 1 = as fast as running forward.
		double ShuffleSpeedFrac = 0.0;
		double BackpedalSpeedFrac = 0.0;
		// Hip turn before a crossover or turn-and-run, at GDD 6.1 Agility 1 and 99.
		double TurnToRunAt1S = 0.0;
		double TurnToRunAt99S = 0.0;
		// Moves longer than these turn and run instead of shuffling or backpedalling.
		double CrossoverMinDistM = 0.0;
		double TurnBackMinDistM = 0.0;
		// The forward, sideways and backward gaits meet at 45 degrees to the net. Once in
		// one, the stick has to cross that line by this many degrees to change it, so a
		// direction wobbling around 45 keeps one gait.
		double GaitHysteresisDeg = 0.0;
		// Reaching the glass or net margin slower than this isn't a dead stop: the braking
		// curve planned the arrival, and only a rounding hair is clamped.
		double EdgeStopToleranceMps = 0.0;

		// GDD 6.3 Sprint and stamina (stamina is 0-1)
		double SprintSpeedBonus = 0.0;
		double SprintDrainAt1 = 0.0;
		double SprintDrainAt99 = 0.0;
		double RegenAt1 = 0.0;
		double RegenAt99 = 0.0;
		double SprintMinStamina = 0.0;

		// GDD 6.3 Fatigue from hard efforts and long rallies (BIOMECHANICS_MOVEMENT 2.13, 5.5).
		// An acceleration above FatigueHardAccelMps2, held as Shots/HardEffortCounter.h
		// counts it, is one hard effort and drains stamina by the amount at GDD 6.1
		// Stamina 1 and 99.
		double FatigueHardAccelMps2 = 0.0;
		double FatigueDrainPerHardAccelAt1 = 0.0;
		double FatigueDrainPerHardAccelAt99 = 0.0;
		// Past FatigueRallyStartS of rally, stamina drains this much per second, by Stamina.
		double FatigueRallyStartS = 0.0;
		double FatigueDrainPerRallySAt1 = 0.0;
		double FatigueDrainPerRallySAt99 = 0.0;
		// The rest a gap between points stands for: when a point ends the player recovers
		// as if they had rested this long at the regen rate. The match's own gap is a
		// couple of seconds; between real points it is 15 to 20 s.
		double FatigueBetweenPointRestS = 0.0;
		// Fraction of top speed and acceleration lost at 0 stamina, linear in stamina.
		double FatigueTopSpeedLossAtZero = 0.0;

		// GDD 6.3 Split-step
		double SplitMaxSpeedMps = 0.0;
		double SplitWindowS = 0.0;
		// Hop length, for animation.
		double SplitHopS = 0.0;
		// First-step acceleration multiplier after a split-step, by Agility.
		double SplitFirstStepAt1 = 0.0;
		double SplitFirstStepAt99 = 0.0;
		double SplitBoostS = 0.0;

		// GDD 6.3 Home drift, as a fraction of top speed, by Positioning
		double HomeDriftAt1 = 0.0;
		double HomeDriftAt99 = 0.0;
		double HomeArriveM = 0.0;

		// Court limits
		double WallMarginM = 0.0;
		double NetMarginM = 0.0;

		std::vector<std::string> Validate() const;
	};
}
