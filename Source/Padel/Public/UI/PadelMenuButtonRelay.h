#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PadelMenuButtonRelay.generated.h"

class UPadelMenuWidget;

/** Tells the menu which of its buttons was pressed (UButton's OnClicked has no parameters). */
UCLASS()
class PADEL_API UPadelMenuButtonRelay : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UPadelMenuWidget> Menu;

	int32 Index = 0;

	UFUNCTION()
	void HandleClicked();
};
