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
		Out.IdleAsset = Row->IdleAsset;
		Out.WalkAssets = Row->WalkAssets;
		Out.JogAssets = Row->JogAssets;
		Out.ReadyAsset = Row->ReadyAsset;
		Out.WalkUprightAssets = Row->WalkUprightAssets;
		Out.WalkClipSpeedMps = Row->WalkClipSpeedMps;
		Out.ArmReadyAlpha = Row->ArmReadyAlpha;
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
			W.MaxReachShare = Row->WarpMaxReachShare;
			W.OverheadBlendUnits = Row->WarpOverheadBlendM * Cm;
			W.TorsoClearanceUnits = Row->WarpTorsoClearanceM * Cm;
			W.MaxToeRiseUnits = Row->WarpMaxToeRiseM * Cm;
			W.MaxOverheadLeanDeg = Row->WarpMaxOverheadLeanDeg;
			for (const double Share : { Row->WarpPelvisDownShare, Row->WarpPelvisForwardShare, Row->WarpSpineSideShare, Row->WarpSpineForwardShare,
				Row->WarpSpineDownShare, Row->WarpTrunkTurnShare })
			{
				if (Share < 0.0 || Share > 1.0)
				{
					Out.Errors.Add(TEXT("Animation.json: the Warp*Share fields must be 0 to 1"));
					break;
				}
			}
			if (Row->WarpMaxPelvisDropM < 0.0 || Row->WarpMaxPelvisForwardM < 0.0 || Row->WarpKneeBendM < 0.0 || Row->WarpElbowBendM < 0.0 || Row->WarpOverheadBlendM <= 0.0 || Row->WarpTorsoClearanceM < 0.0 || Row->WarpMaxToeRiseM < 0.0 || Row->WarpMaxOverheadLeanDeg < 0.0 || Row->WarpMaxOverheadLeanDeg > 90.0
				|| Row->WarpMaxSpineDeg < 0.0 || Row->WarpMaxSpineDeg > 90.0 || Row->WarpMaxTrunkTurnDeg < 0.0 || Row->WarpMaxTrunkTurnDeg > 90.0)
			{
				Out.Errors.Add(TEXT("Animation.json: Warp limits must be 0 or more (angles up to 90 degrees)"));
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
		if (Row->ArmReadyAlpha < 0.0 || Row->ArmReadyAlpha > 1.0)
		{
			Out.Errors.Add(TEXT("Animation.json: ArmReadyAlpha must be 0 to 1"));
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
