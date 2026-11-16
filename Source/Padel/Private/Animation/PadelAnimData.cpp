#include "Animation/PadelAnimData.h"

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
