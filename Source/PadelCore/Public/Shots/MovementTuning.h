// Player movement constants (GDD 6.1, 6.3): the single row of Data/Movement.json.

#pragma once

#include "Shots/ShotMath.h"

#include <string>
#include <vector>

namespace Padel::Shots
{
	struct PADELCORE_API FMovementTuning
	{
		// GDD 6.1 Speed and Acceleration
		double TopSpeedAt1Mps = 0.0;
		double TopSpeedAt99Mps = 0.0;
		double TimeToTopAt1S = 0.0;
		double TimeToTopAt99S = 0.0;
		// Braking is quicker than accelerating.
		double BrakeMult = 0.0;

		// GDD 6.3 Sprint and stamina (stamina is 0-1)
		double SprintSpeedBonus = 0.0;
		double SprintDrainAt1 = 0.0;
		double SprintDrainAt99 = 0.0;
		double RegenAt1 = 0.0;
		double RegenAt99 = 0.0;
		double SprintMinStamina = 0.0;

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
