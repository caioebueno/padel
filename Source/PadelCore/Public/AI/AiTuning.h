// AI tuning values. The numbers live in Data/Difficulty.json and Data/AiTactics.json
// (CLAUDE.md: no hard-coded tuning numbers); AiTuningJson.h reads them. The fields
// start at zero here on purpose, so a value missing from the data shows up in tests.
// Distances are metres, times seconds. "Depth" is metres from the net toward a team's
// own back glass (0..10).

#pragma once

#include <string>

namespace Padel::AI
{
	// One row of Data/Difficulty.json (GDD 8 difficulty table).
	struct FAiDifficulty
	{
		std::string Name;
		// Between the opponent's contact and the AI moving for the ball.
		double ReadDelayS = 0.0;
		// Relative weights of the timing grade the AI releases with (GDD 5.1).
		double TimingWeightPerfect = 0.0;
		double TimingWeightGood = 0.0;
		double TimingWeightEarlyLate = 0.0;
		double TimingWeightMiss = 0.0;
		// 0..1: how strongly the AI prefers the safe option.
		double SafeShotBias = 0.0;
		// 0..1: how strongly the AI plays the situational shot (GDD 8 shot choice).
		double TacticalWeight = 0.0;
		// Multiplies the extra aim scatter (GDD 8: x1.4 / x1.0 / x0.8).
		double AimScatterMult = 0.0;
		// Added to every rating of the AI's players (GDD 8: -10 / 0 / +5).
		int AttributeOffset = 0;
		// Multiplies the rebound prediction noise that Wall Play sets.
		double PredictionNoiseMult = 0.0;
	};

	// The single row of Data/AiTactics.json.
	struct FAiTactics
	{
		// --- Formation (GDD 8 pair tactics) ---
		double NetDepthM = 0.0;            // both at the net, GDD: 2-3 m from the net
		double BackDepthM = 0.0;           // both back, GDD: 1-2 m in front of the glass
		double PartnerSpacingM = 0.0;      // GDD: 4-5 m side to side
		double BallShiftFactor = 0.0;      // how far the pair's centre follows the ball
		double MaxLateralShiftM = 0.0;
		double TakeMiddleShiftM = 0.0;     // extra shift for a player told to take the middle
		double HumanNetDepthM = 0.0;       // a human partner closer than this pulls the AI up
		double HumanDepthHysteresisM = 0.0;
		double DriveToNetMaxDepthM = 0.0;  // an own drive from inside this lets the pair advance
		double LobAdvanceMinDepthM = 0.0;  // a lob landing deeper than this gets time to advance
		double AtNetDepthM = 0.0;          // an opponent closer than this is "at the net"
		double HomeErrorMaxM = 0.0;        // home position error at Positioning 1
		double CourtMarginM = 0.0;         // players keep this far from the walls
		double MinPlayerDepthM = 0.0;      // closest a player goes to the net

		// --- Movement and the GDD 6.1 physical ranges (used to plan) ---
		double ArriveRadiusM = 0.0;
		double SlowRadiusM = 0.0;
		double SprintSlackS = 0.0;
		double SprintDistanceM = 0.0;
		double SpeedAt1Mps = 0.0;
		double SpeedAt99Mps = 0.0;
		double AccelTimeAt1S = 0.0;
		double AccelTimeAt99S = 0.0;
		double PlanningSpeedFactor = 0.0;
		double ReachSideAt1M = 0.0;
		double ReachSideAt99M = 0.0;
		double OverheadReachAt1M = 0.0;
		double OverheadReachAt99M = 0.0;
		double JumpExtraAt1M = 0.0;
		double JumpExtraAt99M = 0.0;
		double AnticipationDelayScaleAt1 = 0.0;
		double AnticipationDelayScaleAt99 = 0.0;

		// --- Prediction (GDD 8) ---
		double PredictDtS = 0.0;
		double PredictHorizonS = 0.0;
		int SampleEverySteps = 0;
		double WallNoiseAt1M = 0.0;
		double WallNoiseAt99M = 0.0;
		double GlassRereadDelayScale = 0.0;

		// --- Interception ---
		double MinContactHeightM = 0.0;
		double IdealContactHeightM = 0.0;
		double OverheadMinHeightM = 0.0;
		double OverheadIdealHeightM = 0.0; // the planner's favourite overhead contact near the net (GDD 4)
		// GDD 5.2: where the ball should meet the player, beside and in front of them.
		// The same numbers as Data/ShotModel.json IdealContactSideM / ForwardM (a test
		// keeps them equal), so the AI stands where the shot model wants the ball.
		double ContactSideM = 0.0;
		double ContactForwardM = 0.0;
		double InterceptWallMarginM = 0.0;
		double ScoreHeightWeight = 0.0;
		double ScoreSlackWeight = 0.0;
		double SlackCapS = 0.0;
		double ScoreTimeWeight = 0.0;
		double VolleyAtNetBonus = 0.0;
		double OverheadBonus = 0.0;
		double SwingLeadS = 0.0;           // the shot request goes out this long before contact

		// --- Who takes the ball ---
		double MiddleBandM = 0.0;
		double BetterPlacedMarginS = 0.0;
		double ConfusionMax = 0.0;         // chance at Teamwork 1 a middle ball goes to the other partner

		// --- Shot choice (GDD 8) ---
		double SmashMaxDepthM = 0.0;
		double SmashMinHeightM = 0.0;
		double SetSlackS = 0.0;            // time to spare that counts as "set under the ball" (GDD 4)
		double PinnedDepthM = 0.0;
		double ChiquitaMaxDepthM = 0.0;
		double LowBallHeightM = 0.0;
		double WeightBandeja = 0.0;
		double WeightViboraLeft = 0.0;
		double WeightViboraRight = 0.0;
		double WeightSmash = 0.0;
		double WeightOverheadLobFromBack = 0.0;
		double WeightLobVsNet = 0.0;
		double WeightChiquitaVsNet = 0.0;
		double WeightDriveVsNet = 0.0;
		double WeightDriveBase = 0.0;
		double WeightLobBase = 0.0;
		double WeightChiquitaBase = 0.0;
		double WeightLobPinned = 0.0;
		double PinnedDriveScale = 0.0;
		double WeightSplitDrive = 0.0;
		double WeightVolley = 0.0;
		double WeightLobVolley = 0.0;
		double SafeLobBonus = 0.0;
		double KickServeChance = 0.0;

		// --- Targets, as the opponents' depth ---
		double LobDepthM = 0.0;
		double LobShortSpreadM = 0.0;     // a lob lands up to this much short of LobDepthM, times the difficulty's aim scatter
		double LobSideM = 0.0;
		double ChiquitaDepthM = 0.0;
		double DriveDeepDepthM = 0.0;
		double DriveSideM = 0.0;
		double VolleyDeepDepthM = 0.0;
		double FeetDepthOffsetM = 0.0;
		double BandejaDepthM = 0.0;
		double BandejaSideM = 0.0;
		double ViboraDepthM = 0.0;
		double ViboraSideM = 0.0;
		double SmashDepthM = 0.0;
		double ServeDepthM = 0.0;
		double ServeSideM = 0.0;
		double TargetSideLimitM = 0.0;
		double TargetDepthMinM = 0.0;
		double TargetDepthMaxM = 0.0;
		double AimScatterBaseM = 0.0;      // extra aim scatter before the difficulty multiplier

		// --- Charge per shot, 0..1 ---
		double ChargeDrive = 0.0;
		double ChargeVolley = 0.0;
		double ChargeLob = 0.0;
		double ChargeBandeja = 0.0;
		double ChargeVibora = 0.0;
		double ChargeSmash = 0.0;
		double ChargeChiquita = 0.0;
		double ChargeServe = 0.0;

		// --- Timing windows (GDD 5.1 base; Data/Shots.json wins once the adapter reads it) ---
		double WindowPerfectS = 0.0;
		double WindowGoodS = 0.0;
		double WindowEarlyLateS = 0.0;
		double WindowMissExtraS = 0.0;

		// --- Match flow, for AI players in a real match ---
		double ServeDelayS = 0.0;          // the AI server waits this long, set, before the toss
		double PointGapS = 0.0;            // between the end of a point and the next serve
		double WaistHeightFraction = 0.0;  // waist height / player height, for the serve's waist rule (GDD 3.2)
		double DeadBallMarginS = 0.0;      // a late swing still releases this long before the ball dies
		double LateRequestS = 0.0;         // while the controller sees another moment, the request may wait until this long before contact, with less backswing
		double ContactMatchS = 0.0;        // the swing controller's contact moment must be this close to the plan's
		double NetClearanceM = 0.0;        // the AI wants its flat shots this far over the tape
		double NetRetargetDepthStepM = 0.0;
		int NetRetargetSteps = 0;
		double NetRetargetChargeStep = 0.0;
	};
}
