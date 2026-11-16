#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PadelPlayerBodyRow.generated.h"

/**
 * One row of Data/PlayerBodies.json, keyed by the player's row name in Players.json
 * (docs/decisions/0006-metahuman-players.md). Asset paths, not tuning: which MetaHuman
 * a player shows. A row whose assets are not on disk falls back to the team placeholder.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelPlayerBodyRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The assembled MetaHuman actor Blueprint class (".../BP_Name.BP_Name_C"). MetaHuman assets are not in git. */
	UPROPERTY(EditAnywhere, Category = "MetaHuman") FString MetaHumanClass;
	/** The skeletal mesh component in that actor that the others follow (MetaHuman's "Body"). */
	UPROPERTY(EditAnywhere, Category = "MetaHuman") FString BodyComponent;
};
