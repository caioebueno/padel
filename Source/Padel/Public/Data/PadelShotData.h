#pragma once

#include "CoreMinimal.h"
#include "Shots/BallMachineFeed.h"
#include "Shots/MovementTuning.h"
#include "Shots/PlayerRatings.h"
#include "Shots/PracticeBallParams.h"
#include "Shots/ShotLibrary.h"

#include <vector>

/**
 * Everything the shot system needs from Data/: the JSON files are the source of
 * truth (CLAUDE.md), read at startup into transient DataTables and converted to the
 * PadelCore structs. A missing or broken file is logged and leaves bOk false.
 */
struct PADEL_API FPadelShotData
{
	Padel::Shots::FShotLibrary Library;
	Padel::Shots::FMovementTuning Movement;
	Padel::Shots::FPracticeBallParams PracticeBall;
	std::vector<Padel::Shots::FBallMachineFeed> Feeds;
	bool bOk = false;
	TArray<FString> Errors;

	/** Loads every file from <ProjectDir>/Data. */
	static FPadelShotData Load();

	/** Loads Data/Players.json and returns the named row. */
	static bool LoadPlayer(FName RowName, Padel::Shots::FPlayerRatings& OutRatings, FString* OutDisplayName = nullptr);

	static FString DataDirectory();
};
