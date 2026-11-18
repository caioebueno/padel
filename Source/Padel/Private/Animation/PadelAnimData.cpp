#include "Animation/PadelAnimData.h"

#include "Court/PadelCourtUnits.h"
#include "Data/PadelAnimClipRow.h"
#include "Data/PadelAnimSettingsRow.h"
#include "Data/PadelShotData.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "UObject/Package.h"

namespace PadelAnimDataDetail
{
	UDataTable* LoadAnimTable(const TCHAR* File, UScriptStruct* RowStruct, TArray<FString>& Errors)
	{
		const FString Path = FPaths::Combine(FPadelShotData::DataDirectory(), File);
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			Errors.Add(FString::Printf(TEXT("Cannot read %s"), *Path));
			return nullptr;
		}
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = RowStruct;
		for (const FString& Problem : Table->CreateTableFromJSONString(Json))
		{
			Errors.Add(FString::Printf(TEXT("%s: %s"), File, *Problem));
		}
		return Table;
	}
}

FPadelAnimData FPadelAnimData::Load()
{
	using namespace PadelAnimDataDetail;
	FPadelAnimData Out;

	UDataTable* SettingsTable = LoadAnimTable(TEXT("Animation.json"), FPadelAnimSettingsRow::StaticStruct(), Out.Errors);
	if (const FPadelAnimSettingsRow* Row = SettingsTable ? SettingsTable->FindRow<FPadelAnimSettingsRow>(FName(TEXT("default")), TEXT("Animation.json"), false) : nullptr)
	{
		Out.Settings = Row->ToCore();
		Out.TeamMeshes = Row->TeamMeshes;
		Out.AnimClass = Row->AnimClass;
		Out.RacketBone = FName(*Row->RacketBone);
		Out.MetaHumanRetargeter = Row->MetaHumanRetargeter;
		Out.MetaHumanMinLod = Row->MetaHumanMinLod;
		Out.MetaHumanIkLodThreshold = Row->MetaHumanIkLodThreshold;
		Out.MetaHumanFallbackClass = Row->MetaHumanFallbackClass;
		Out.MetaHumanShoeMesh = Row->MetaHumanShoeMesh;
		Out.ShoeAnkleToBallM = Row->ShoeAnkleToBallM;
		if (!Row->MetaHumanShoeMesh.IsEmpty() && Row->ShoeAnkleToBallM <= 0.0)
		{
			Out.Errors.Add(TEXT("Animation.json: ShoeAnkleToBallM must be above 0 with a MetaHumanShoeMesh"));
		}
		Out.IdleAsset = Row->IdleAsset;
		Out.WalkAssets = Row->WalkAssets;
		Out.JogAssets = Row->JogAssets;
		Out.ReadyAsset = Row->ReadyAsset;
		Out.WalkUprightAssets = Row->WalkUprightAssets;
		Out.WalkClipSpeedMps = Row->WalkClipSpeedMps;
		Out.MotionMatchingDatabases = Row->MotionMatchingDatabases;
		Out.MotionMatchingStillDatabases = Row->MotionMatchingStillDatabases;
		Out.MotionMatchingStopDatabases = Row->MotionMatchingStopDatabases;
		Out.MotionMatchingWalkDatabases = Row->MotionMatchingWalkDatabases;
		Out.MotionMatchingSprintDatabases = Row->MotionMatchingSprintDatabases;
		Out.MotionMatchingTurnInPlaceDatabases = Row->MotionMatchingTurnInPlaceDatabases;
		for (const TArray<FString>* Subset : { &Row->MotionMatchingStillDatabases, &Row->MotionMatchingStopDatabases, &Row->MotionMatchingTurnInPlaceDatabases,
			&Row->MotionMatchingWalkDatabases, &Row->MotionMatchingSprintDatabases })
		{
			for (const FString& Path : *Subset)
			{
				if (!Row->MotionMatchingDatabases.Contains(Path))
				{
					Out.Errors.Add(FString::Printf(TEXT("Animation.json: %s isn't in MotionMatchingDatabases"), *Path));
				}
			}
		}
		{
			FPadelMotionMatchingTuning& M = Out.MotionMatching;
			M.SearchThrottleS = Row->MotionMatchingSearchThrottleS;
			M.BlendTimeS = Row->MotionMatchingBlendTimeS;
			M.PoseJumpThresholdS = Row->MotionMatchingPoseJumpThresholdS;
			M.PlayRateMin = Row->MotionMatchingPlayRateMin;
			M.PlayRateMax = Row->MotionMatchingPlayRateMax;
			M.MaxBlends = Row->MotionMatchingMaxBlends;
			M.RepickAfterBlend = Row->MotionMatchingRepickAfterBlend;
			M.HistoryPoses = Row->MotionMatchingHistoryPoses;
			M.HistorySamplingS = Row->MotionMatchingHistorySamplingS;
			M.StillBelowUnits = Row->MotionMatchingStillBelowMps * Padel::MetresToWorld(FVector(1.0, 0.0, 0.0)).X;
			M.TurnInPlaceDeg = Row->MotionMatchingTurnInPlaceDeg;
			M.StopAfterUnits = Row->MotionMatchingStopAfterM * Padel::MetresToWorld(FVector(1.0, 0.0, 0.0)).X;
			M.RunFromUnits = Row->MotionMatchingRunFromMps * Padel::MetresToWorld(FVector(1.0, 0.0, 0.0)).X;
			M.SprintFromUnits = Row->MotionMatchingSprintFromMps * Padel::MetresToWorld(FVector(1.0, 0.0, 0.0)).X;
			M.GaitOverlap = Row->MotionMatchingGaitOverlap;
			M.InertialHalfLifeS = Row->MotionMatchingInertialHalfLifeS;
			M.SwitchCostMargin = Row->MotionMatchingSwitchCostMargin;
			if (Row->MotionMatchingInertialHalfLifeS < 0.0 || Row->MotionMatchingSwitchCostMargin < 0.0)
			{
				Out.Errors.Add(TEXT("Animation.json: MotionMatchingInertialHalfLifeS and MotionMatchingSwitchCostMargin must be 0 or more"));
			}
			if (M.RunFromUnits <= 0.0 || M.SprintFromUnits <= M.RunFromUnits || M.GaitOverlap < 0.0 || M.GaitOverlap >= 1.0)
			{
				Out.Errors.Add(TEXT("Animation.json: MotionMatchingSprintFromMps must be above MotionMatchingRunFromMps (above 0), MotionMatchingGaitOverlap 0 to below 1"));
			}
			if (M.SearchThrottleS < 0.0 || M.BlendTimeS < 0.0 || M.PoseJumpThresholdS < 0.0 || M.HistorySamplingS < 0.0)
			{
				Out.Errors.Add(TEXT("Animation.json: MotionMatching times must be 0 or more"));
			}
			if (M.PlayRateMin <= 0.0 || M.PlayRateMin > 1.0 || M.PlayRateMax < 1.0)
			{
				Out.Errors.Add(TEXT("Animation.json: MotionMatchingPlayRateMin must be above 0 and at most 1, MotionMatchingPlayRateMax at least 1"));
			}
			if (M.MaxBlends < 1 || M.HistoryPoses < 2)
			{
				Out.Errors.Add(TEXT("Animation.json: MotionMatchingMaxBlends must be 1 or more and MotionMatchingHistoryPoses 2 or more"));
			}
		}
		Out.MocapLegFadeFromMps = Row->MocapLegFadeFromMps;
		Out.MocapLegFadeToMps = Row->MocapLegFadeToMps;
		Out.MocapLegKeep = Row->MocapLegKeep;
		Out.MocapLeanKeep = Row->MocapLeanKeep;
		Out.MocapChestTwistKeep = Row->MocapChestTwistKeep;
		Out.MocapHeadMaxTwistDeg = Row->MocapHeadMaxTwistDeg;
		if (Row->MocapChestTwistKeep < 0.0 || Row->MocapChestTwistKeep > 1.0 || Row->MocapHeadMaxTwistDeg < 0.0)
		{
			Out.Errors.Add(TEXT("Animation.json: MocapChestTwistKeep must be 0 to 1 and MocapHeadMaxTwistDeg 0 or more"));
		}
		{
			const double Unit = Padel::MetresToWorld(FVector(1.0, 0.0, 0.0)).X;
			FPadelLocomotionWarpTuning& W = Out.LocomotionWarp;
			W.OrientationMaxDeg = Row->OrientationWarpMaxDeg;
			W.OrientationHalfLifeS = Row->OrientationWarpHalfLifeS;
			W.OrientationMinSpeedUnits = Row->OrientationWarpMinMps * Unit;
			W.OrientationSpineCounter = Row->OrientationWarpSpineCounter;
			W.StrideMin = Row->StrideWarpMin;
			W.StrideMax = Row->StrideWarpMax;
			W.StrideHalfLifeS = Row->StrideWarpHalfLifeS;
			W.StrideMinSpeedUnits = Row->StrideWarpMinMps * Unit;
			W.FootLockHeightUnits = Row->FootLockHeightM * Unit;
			W.FootLockSpeedUnits = Row->FootLockSpeedMps * Unit;
			W.FootLockReleaseHeightUnits = Row->FootLockReleaseHeightM * Unit;
			W.FootLockReleaseSpeedUnits = Row->FootLockReleaseSpeedMps * Unit;
			W.FootLockMaxDriftUnits = Row->FootLockMaxDriftM * Unit;
			W.FootLockBlendOutS = Row->FootLockBlendOutS;
			if (W.OrientationMaxDeg < 0.0 || W.OrientationMaxDeg > 90.0 || W.StrideMin <= 0.0 || W.StrideMin > 1.0 || W.StrideMax < 1.0
				|| W.OrientationHalfLifeS < 0.0 || W.StrideHalfLifeS < 0.0 || W.FootLockHeightUnits < 0.0 || W.FootLockBlendOutS < 0.0
				|| W.FootLockReleaseHeightUnits < W.FootLockHeightUnits || W.FootLockReleaseSpeedUnits < W.FootLockSpeedUnits || W.FootLockMaxDriftUnits <= 0.0)
			{
				Out.Errors.Add(TEXT("Animation.json: OrientationWarpMaxDeg 0 to 90, StrideWarpMin above 0 to 1, StrideWarpMax 1 or more, FootLock release limits at least the lock ones, FootLockMaxDriftM above 0, times 0 or more"));
			}
		}
		if (Row->MocapLegFadeFromMps < 0.0 || Row->MocapLegFadeToMps <= Row->MocapLegFadeFromMps
			|| Row->MocapLegKeep < 0.0 || Row->MocapLegKeep > 1.0 || Row->MocapLeanKeep < 0.0 || Row->MocapLeanKeep > 1.0)
		{
			Out.Errors.Add(TEXT("Animation.json: MocapLegFadeToMps must be above MocapLegFadeFromMps (0 or more), MocapLegKeep and MocapLeanKeep 0 to 1"));
		}
		{
			// Metres to Unreal units once, here (CLAUDE.md, "Coordinates and units").
			const double Cm = Padel::MetresToWorld(FVector(1.0, 0.0, 0.0)).X;
			FPadelWarpTuning& W = Out.Warp;
			W.PelvisDownShare = Row->WarpPelvisDownShare;
			W.MaxPelvisDropUnits = Row->WarpMaxPelvisDropM * Cm;
			W.PelvisForwardShare = Row->WarpPelvisForwardShare;
			W.MaxPelvisForwardUnits = Row->WarpMaxPelvisForwardM * Cm;
			W.SpineSideShare = Row->WarpSpineSideShare;
			W.SpineForwardShare = Row->WarpSpineForwardShare;
			W.SpineDownShare = Row->WarpSpineDownShare;
			W.MaxSpineDeg = Row->WarpMaxSpineDeg;
			W.TrunkTurnShare = Row->WarpTrunkTurnShare;
			W.MaxTrunkTurnDeg = Row->WarpMaxTrunkTurnDeg;
			W.KneeBendUnits = Row->WarpKneeBendM * Cm;
			W.ElbowBendUnits = Row->WarpElbowBendM * Cm;
			W.ElbowPoleSteadyUnits = Row->WarpElbowPoleSteadyM * Cm;
			W.MaxReachShare = Row->WarpMaxReachShare;
			W.MinReachShare = Row->WarpMinReachShare;
			W.CarryFollowShare = Row->WarpCarryFollowShare;
			W.OverheadBlendUnits = Row->WarpOverheadBlendM * Cm;
			W.TorsoClearanceUnits = Row->WarpTorsoClearanceM * Cm;
			W.MaxToeRiseUnits = Row->WarpMaxToeRiseM * Cm;
			W.MaxOverheadLeanDeg = Row->WarpMaxOverheadLeanDeg;
			for (const double Share : { Row->WarpCarryFollowShare, Row->WarpPelvisDownShare, Row->WarpPelvisForwardShare, Row->WarpSpineSideShare, Row->WarpSpineForwardShare,
				Row->WarpSpineDownShare, Row->WarpTrunkTurnShare })
			{
				if (Share < 0.0 || Share > 1.0)
				{
					Out.Errors.Add(TEXT("Animation.json: the Warp*Share fields must be 0 to 1"));
					break;
				}
			}
			if (Row->WarpMaxPelvisDropM < 0.0 || Row->WarpMaxPelvisForwardM < 0.0 || Row->WarpKneeBendM < 0.0 || Row->WarpElbowBendM < 0.0 || Row->WarpElbowPoleSteadyM < 0.0 || Row->WarpOverheadBlendM <= 0.0 || Row->WarpTorsoClearanceM < 0.0 || Row->WarpMaxToeRiseM < 0.0 || Row->WarpMaxOverheadLeanDeg < 0.0 || Row->WarpMaxOverheadLeanDeg > 90.0
				|| Row->WarpMaxSpineDeg < 0.0 || Row->WarpMaxSpineDeg > 90.0 || Row->WarpMaxTrunkTurnDeg < 0.0 || Row->WarpMaxTrunkTurnDeg > 90.0)
			{
				Out.Errors.Add(TEXT("Animation.json: Warp limits must be 0 or more (angles up to 90 degrees)"));
			}
			if (Row->WarpMinReachShare < 0.0 || Row->WarpMinReachShare >= Row->WarpMaxReachShare)
			{
				Out.Errors.Add(TEXT("Animation.json: WarpMinReachShare must be 0 or more and below WarpMaxReachShare"));
			}
			if (Row->WarpMaxReachShare <= 0.0 || Row->WarpMaxReachShare > 1.0)
			{
				Out.Errors.Add(TEXT("Animation.json: WarpMaxReachShare must be above 0 and at most 1"));
			}
		}
		if (Row->WalkClipSpeedMps <= 0.0 || Row->WalkClipSpeedMps >= Row->RunClipSpeedMps)
		{
			Out.Errors.Add(TEXT("Animation.json: WalkClipSpeedMps must be above 0 and below RunClipSpeedMps"));
		}
		if (Row->WalkAssets.Num() != 4 || Row->JogAssets.Num() != 4 || (Row->WalkUprightAssets.Num() != 0 && Row->WalkUprightAssets.Num() != 4))
		{
			Out.Errors.Add(TEXT("Animation.json: WalkAssets, JogAssets and WalkUprightAssets list forward, back, left, right"));
		}
		{
			// P1: every stance exactly once.
			TSet<FString> Seen;
			for (const FPadelStanceRow& Stance : Row->Stances)
			{
				Padel::Animation::EStance Parsed;
				if (!Padel::Animation::ParseStance(TCHAR_TO_UTF8(*Stance.Name), Parsed))
				{
					Out.Errors.Add(FString::Printf(TEXT("Animation.json: unknown stance \"%s\""), *Stance.Name));
				}
				else if (Seen.Contains(Stance.Name))
				{
					Out.Errors.Add(FString::Printf(TEXT("Animation.json: stance \"%s\" is listed twice"), *Stance.Name));
				}
				Seen.Add(Stance.Name);
			}
			if (Seen.Num() != Padel::Animation::NumStances)
			{
				Out.Errors.Add(TEXT("Animation.json: Stances must list Relaxed, Serve, Receive, Net and Back"));
			}
		}
		if (Row->MetaHumanMinLod < 0)
		{
			Out.Errors.Add(TEXT("Animation.json: MetaHumanMinLod must be 0 or more"));
		}
		for (const std::string& Problem : Out.Settings.Validate())
		{
			Out.Errors.Add(UTF8_TO_TCHAR(Problem.c_str()));
		}
	}
	else
	{
		Out.Errors.Add(TEXT("Animation.json has no \"default\" row"));
	}

	if (UDataTable* Clips = LoadAnimTable(TEXT("AnimClips.json"), FPadelAnimClipRow::StaticStruct(), Out.Errors))
	{
		Clips->ForeachRow<FPadelAnimClipRow>(TEXT("AnimClips.json"), [&Out](const FName& Key, const FPadelAnimClipRow& Row)
		{
			Padel::Animation::FAnimClip Clip;
			FString Error;
			if (Row.ToCore(Key, Clip, Error))
			{
				Out.Clips.Add(Clip);
			}
			else
			{
				Out.Errors.Add(Error);
			}
		});
		for (const std::string& Problem : Out.Clips.Validate())
		{
			Out.Errors.Add(UTF8_TO_TCHAR(Problem.c_str()));
		}
	}

	if (UDataTable* Bodies = LoadAnimTable(TEXT("PlayerBodies.json"), FPadelPlayerBodyRow::StaticStruct(), Out.Errors))
	{
		Bodies->ForeachRow<FPadelPlayerBodyRow>(TEXT("PlayerBodies.json"), [&Out](const FName& Key, const FPadelPlayerBodyRow& Row)
		{
			if (Row.MetaHumanClass.IsEmpty() || Row.BodyComponent.IsEmpty())
			{
				Out.Errors.Add(FString::Printf(TEXT("PlayerBodies.json: %s needs MetaHumanClass and BodyComponent"), *Key.ToString()));
				return;
			}
			Out.PlayerBodies.Add(Key, Row);
		});
	}

	for (const FString& Error : Out.Errors)
	{
		UE_LOG(LogPadel, Error, TEXT("Animation data: %s"), *Error);
	}
	Out.bOk = Out.Errors.IsEmpty();
	return Out;
}
