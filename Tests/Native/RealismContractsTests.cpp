// The movement and strokes realism plan (K1) adds its data fields before any code reads
// them. These tests pin the shipped values to the neutral defaults that keep today's
// behaviour, so a lane that changes one does it on purpose and updates the test with it.
// Numbers and sources: docs/animation/BIOMECHANICS_MOVEMENT.md 5, BIOMECHANICS_SHOTS.md 5.
#include "doctest.h"
#include "AnimTestData.h"
#include "ShotTestData.h"
#include "Support/AiTestData.h"

using doctest::Approx;

TEST_CASE("Contracts.Realism: Movement.json's new fields are neutral (GDD 6.1, 6.3)")
{
	const Padel::Shots::FMovementTuning M = ShotTestData::LoadMovement();
	CHECK(M.Validate().empty());
	// M1 turned the falloff on (PlayerMotionTests and MotionCurveTests pin its shape).
	CHECK(M.AccelFalloff == Approx(0.3));
	// M2 turned these on (PlayerMotionTests pins their behaviour).
	CHECK(M.ShuffleSpeedFrac == Approx(0.65));
	CHECK(M.BackpedalSpeedFrac == Approx(0.5));
	CHECK(M.TurnToRunAt1S == Approx(0.27));
	CHECK(M.TurnToRunAt99S == Approx(0.09));
	CHECK(M.CrossoverMinDistM == Approx(2.5));
	CHECK(M.TurnBackMinDistM == Approx(1.5));
	CHECK(M.FatigueHardAccelMps2 == Approx(3.0));
	CHECK(M.FatigueDrainPerHardAccel == Approx(0.0));
	CHECK(M.FatigueRallyStartS == Approx(15.0));
	CHECK(M.FatigueDrainPerRallyS == Approx(0.0));
	CHECK(M.FatigueTopSpeedLossAtZero == Approx(0.0));
}

TEST_CASE("Contracts.Realism: a speed fraction left out of Movement.json is caught")
{
	// A missing JSON field reads as 0 in Unreal's import, which would freeze sideways
	// and backward movement; Validate() has to say so.
	Padel::Shots::FMovementTuning M = ShotTestData::LoadMovement();
	M.ShuffleSpeedFrac = 0.0;
	CHECK_FALSE(M.Validate().empty());
}

TEST_CASE("Contracts.Realism: AiTactics.json's new fields are neutral, or set by their lane (GDD 8)")
{
	const Padel::AI::FAiTactics& T = Padel::AI::TestData::Tactics();
	// A2 (movement doc 5.4): the stagger and the spacing by ball width.
	CHECK(T.NetStaggerM == Approx(0.75));
	CHECK(T.PartnerSpacingMinM == Approx(3.5));
	CHECK(T.PartnerSpacingMaxM == Approx(5.0));
	// A4 (movement doc 2.12): the side switch on lobs landing past 7 m.
	CHECK(T.SideSwitchMinLobDepthM == Approx(7.0));
	CHECK(T.FirstStepExtraS == Approx(0.0));
}

TEST_CASE("Contracts.Realism: AnimClips.json has no contact points yet (0005)")
{
	const Padel::Animation::FAnimClipLibrary Clips = AnimTestData::LoadClips();
	CHECK_FALSE(Clips.All().empty());
	for (const Padel::Animation::FAnimClip& Clip : Clips.All())
	{
		CAPTURE(Clip.Name);
		CHECK_FALSE(Clip.HasContactPoint());
	}
}
