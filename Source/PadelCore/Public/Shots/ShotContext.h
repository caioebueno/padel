// Everything the selector and resolver need about one contact. Built by
// FPlayerShotController (human or AI), or by hand in tests.

#pragma once

#include "Shots/PlayerRatings.h"
#include "Shots/ShotMath.h"

#include <cstdint>

namespace Padel::Shots
{
	enum class EShotButton : std::uint8_t { Drive, Lob, Overhead };

	struct FShotContext
	{
		const FPlayerRatings* Player = nullptr;
		int PlayerId = 0;
		// Hitter's half: +1 Team A (+X), -1 Team B.
		double TeamSign = 1.0;

		EShotButton Button = EShotButton::Drive;
		bool bModAttack = false;
		bool bModTouch = false;
		// 0-1 from hold time (GDD 7).
		double Charge = 0.0;
		// Team-relative stick at release: X = right, Y = deep (GDD 7).
		FVec2 Aim;
		// AI: used instead of the stick aim.
		bool bHasTargetOverride = false;
		FVec3 TargetOverride;

		// Release minus ideal moment, seconds.
		double TimingOffsetS = 0.0;
		FVec3 ContactPosition;
		// +1 when the ball is on the player's right at contact, -1 on the left.
		double ContactSide = 1.0;
		// Distance from the ideal contact point (GDD 5.2).
		double PositionDistanceM = 0.0;
		// The ball has bounced on the hitter's ground: not a volley.
		bool bBallBounced = true;
		bool bOffGlass = false;

		FVec3 PlayerVelocity;
		double TopSpeedMps = 0.0;
		// 0-1
		double Stamina = 1.0;
		// GDD 5.2: a split-step before the opponent's contact removes the running penalty.
		bool bSplitStepped = false;
		// GDD 4: feet still and behind the contact point.
		bool bSet = false;

		bool bServe = false;
		bool bReturn = false;
		// Break, set or golden point (GDD 5.3).
		bool bPressurePoint = false;
		bool bWrongSide = false;

		bool IsForehand() const { return Sign(ContactSide) == Player->ForehandSign(); }
	};
}
