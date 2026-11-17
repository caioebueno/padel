#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Animation/Model/PlayerAnimTuning.h"
#include "PadelAnimSettingsRow.generated.h"

/**
 * The single row ("default") of Data/Animation.json (docs/decisions/0005-player-animation.md).
 * Field meanings are documented on Padel::Animation::FAnimSettings.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelAnimSettingsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Locomotion") double WalkMinMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double SprintMinMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double RunClipSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double SprintClipSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double StrideRateMin = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double StrideRateMax = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double ReadyBallWindowS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double PlantedMaxSpeedMps = 0.0;
	/** N1: how fast the body turns to its facing (square to the net, or toward travel in Crossover, TurnAndRun and Run), degrees a second. */
	UPROPERTY(EditAnywhere, Category = "Locomotion") double FacingTurnRateDegPerS = 0.0;
	/** N1c: degrees the body turns side-on from square to the net while it waits on a ball off the back glass, and off a side glass. */
	UPROPERTY(EditAnywhere, Category = "Locomotion") double GlassBackTurnDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double GlassSideTurnDeg = 0.0;
	/** N1: ready-stance depth, the crouched clips' weight against the upright ones: at the net, and at the back glass. */
	UPROPERTY(EditAnywhere, Category = "Locomotion") double ReadyCrouchAlphaNet = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double ReadyCrouchAlphaBack = 0.0;
	/** F1 body dynamics: facing springs, lean, plant drop, split-step hop and ready bounce. Meanings on Padel::Animation::FAnimSettings. */
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingChestHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingHeadHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingMaxChestTwistDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingMaxHeadTwistDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double LeanDegPerMps2 = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double LeanMaxDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double LeanHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double PlantDropPerMps2 = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double PlantDropMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double PelvisHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double SplitHopHeightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double SplitLandDropM = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double SplitLandS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double ReadyBounceM = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double ReadyBounceHz = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double ReadyBounceHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double FullBodyBlendPerS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double SwingBlendInS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double ClipCrossfadeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Celebration") double CelebrationS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Celebration") double CelebrationBlendS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double ReadyRacketRightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double ReadyRacketForwardM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double ReadyRacketUpM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<double> TrajectoryTimesS;
	UPROPERTY(EditAnywhere, Category = "Celebration") TArray<FString> CelebrateAssets;
	UPROPERTY(EditAnywhere, Category = "Celebration") TArray<FString> DejectedAssets;

	/** Skeletal mesh per team (index 0 Team A, 1 Team B) and the thin AnimBP class (ABP_PadelPlayer). */
	UPROPERTY(EditAnywhere, Category = "Assets") TArray<FString> TeamMeshes;
	UPROPERTY(EditAnywhere, Category = "Assets") FString AnimClass;
	/** Bone the racket attaches to once a skeletal body is shown. */
	UPROPERTY(EditAnywhere, Category = "Assets") FString RacketBone;

	/**
	 * MetaHuman players (docs/decisions/0006-metahuman-players.md): the IK Retargeter from the
	 * placeholder skeleton to the MetaHuman body, the first LOD the MetaHumans may draw (their
	 * LOD 0 face is far more than a broadcast-distance player needs at 60 fps) and the last LOD
	 * that runs the retargeter's IK pass (-1 = every LOD).
	 */
	UPROPERTY(EditAnywhere, Category = "MetaHuman") FString MetaHumanRetargeter;
	UPROPERTY(EditAnywhere, Category = "MetaHuman") int32 MetaHumanMinLod = 0;
	UPROPERTY(EditAnywhere, Category = "MetaHuman") int32 MetaHumanIkLodThreshold = -1;

	/**
	 * The C++ pose used when ABP_PadelPlayer isn't there (0006): idle, walk and jog cycles
	 * (forward, back, left, right) on the placeholder skeleton, the walk cycle's ground speed
	 * (the jog's is RunClipSpeedMps), and how far the arm reaches for the racket target in the
	 * ready stance while no swing clip is imported.
	 */
	UPROPERTY(EditAnywhere, Category = "Locomotion") FString IdleAsset;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<FString> WalkAssets;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<FString> JogAssets;
	/** N1: the crouched ready idle and the upright walk cycles (forward, back, left, right) the ready stance mixes with by ReadyCrouchAlpha*. */
	UPROPERTY(EditAnywhere, Category = "Locomotion") FString ReadyAsset;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<FString> WalkUprightAssets;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double WalkClipSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double ArmReadyAlpha = 0.0;

	/**
	 * Contact warp (movement plan N3, shots doc 4.2, PadelContactWarp.h): how the reach for the
	 * ball splits across the body. Metres and degrees here; Unreal units once, at load.
	 */
	/** Of a target below the face, how much the pelvis drops (knee bend; the feet stay planted). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpPelvisDownShare = 0.8;
	/** The deepest knee bend. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxPelvisDropM = 0.5;
	/** Of the reach ahead, how much the pelvis shifts forward. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpPelvisForwardShare = 0.25;
	/** The longest forward pelvis shift. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxPelvisForwardM = 0.15;
	/** Of the sideways reach, how much the spine side-bends. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpSpineSideShare = 0.45;
	/** Of the reach ahead, how much the spine flexes forward. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpSpineForwardShare = 0.25;
	/** Of what's still below after the knees, how much the spine flexes. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpSpineDownShare = 0.75;
	/** The most the spine bends, both passes together. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxSpineDeg = 55.0;
	/** Of the angle from the racket shoulder to the ball, how much the trunk turns about the vertical. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpTrunkTurnShare = 0.7;
	/** The most the trunk turns. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxTrunkTurnDeg = 50.0;
	/** How far in front of the knee the leg IK aims it (knees bend forward). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpKneeBendM = 0.2;
	/** How far out the arm IK aims the elbow (it keeps the current pose's bend). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpElbowBendM = 0.2;
	/** Of the arm's length, the furthest the hand goes before the racket turns along the arm (never a locked elbow). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxReachShare = 0.98;
	/** How far above the shoulder a target has to be for the full overhead arm (long arm, elbow up and out); blended in from the shoulder. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpOverheadBlendM = 0.40;
	/** How far the forearm keeps from the spine (pelvis up to the top spine bone) through a swing: the torso half-depth plus a margin. 0 turns the guard off. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpTorsoClearanceM = 0.14;
	/** An overhead past the arm's reach: how high the player rises onto the toes, and how far the trunk leans towards the ball. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxToeRiseM = 0.12;
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxOverheadLeanDeg = 30.0;

	Padel::Animation::FAnimSettings ToCore() const;
};
