#pragma once

#include "CoreMinimal.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelMainMenuScreen.generated.h"

class UPadelMenuTile;

/**
 * Spec 7.2, boards "MainMenu" and "MainMenuLocked": the Quick Match hero tile, Practice
 * and Settings beside it, and the bottom row of Career and Team Builder (locked) and
 * Controls. Locked tiles are focusable and say why; A on one shows a toast (spec 5.3).
 * B opens "Quit game?" (DefaultBack). The grid and its focus map come from
 * Padel::Ui::MainMenuTiles() and MainMenuFocusMap().
 */
UCLASS()
class PADEL_API UPadelMainMenuScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

public:
	UPadelMenuTile* FindTile(FName Id) const;

protected:
	virtual void BuildScreen() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;
	/** The staggered tiles carry a fresh entry's move, so the stage only fades (no double slide). */
	virtual bool UsesStageSlide() const override { return !bFreshEntry; }

private:
	void ApplyEntry();

	/** Seconds since the screen opened, for the tiles' staggered entry (spec 7.2, 9). */
	float EntrySeconds = 0.0f;
	bool bStaggerEntry = false;
	/** Set once at build: this arrival staggers its tiles (stays set after they land). */
	bool bFreshEntry = false;
	Padel::Ui::EPadFamily ShownFamily = Padel::Ui::EPadFamily::Xbox;

	/** In MainMenuTiles() order. */
	UPROPERTY()
	TArray<TObjectPtr<UPadelMenuTile>> Tiles;
};
