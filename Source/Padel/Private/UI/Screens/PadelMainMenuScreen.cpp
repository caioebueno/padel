#include "UI/Screens/PadelMainMenuScreen.h"

#include "UI/Model/PadelUiFrontEnd.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Widgets/PadelMenuTile.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiNavKind;

namespace
{
	struct FMainMenuCopy
	{
		FText Label;
		FText Description;
		EPadelIcon Icon = EPadelIcon::Court;
	};

	// Spec 8's copy for each tile. Locked tiles show the mode's line icon (spec 6.3);
	// the others have no rendered art yet (W12), so the court icon stands in (spec 7.12).
	FMainMenuCopy MainMenuCopyFor(const FString& Id)
	{
		if (Id == TEXT("Main.QuickMatch"))
		{
			return {LOCTEXT("Main.QuickMatch", "Quick Match"), LOCTEXT("Main.QuickMatch.Desc", "You and an AI partner against two AI players.")};
		}
		if (Id == TEXT("Main.Practice"))
		{
			return {LOCTEXT("Main.Practice", "Practice"), LOCTEXT("Main.Practice.Desc", "Ball machine drills for every shot.")};
		}
		if (Id == TEXT("Main.Settings"))
		{
			return {LOCTEXT("Main.Settings", "Settings"), LOCTEXT("Main.Settings.Desc", "Audio and accessibility.")};
		}
		if (Id == TEXT("Main.Controls"))
		{
			return {LOCTEXT("Main.Controls", "Controls"), LOCTEXT("Main.Controls.Desc", "See every button.")};
		}
		if (Id == TEXT("Main.Career"))
		{
			return {LOCTEXT("Main.Career", "Career"), LOCTEXT("Main.Locked.Desc", "Coming in a later update."), EPadelIcon::Trophy};
		}
		return {LOCTEXT("Main.TeamBuilder", "Team Builder"), LOCTEXT("Main.Locked.Desc", "Coming in a later update."),
			EPadelIcon::LinkedFigures};
	}
}

void UPadelMainMenuScreen::BuildScreen()
{
	// The board's header: the game's name. The profile chip beside it is post-slice (6.5).
	SetHeader(LOCTEXT("Title.Logo", "Padel Street"));

	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	ShownFamily = GetPadFamily();
	for (const Padel::Ui::FMainMenuTile& Spec : Padel::Ui::MainMenuTiles())
	{
		const FString Id = UTF8_TO_TCHAR(Spec.Id);
		const FMainMenuCopy Copy = MainMenuCopyFor(Id);
		UPadelMenuTile* Tile = MakeElement<UPadelMenuTile>(FName(*Id));
		Tile->SetHero(Spec.bHero);
		Tile->SetTileSize(Theme.SpanWidth(Spec.Span), Spec.Height);
		Tile->SetLabel(Copy.Label);
		Tile->SetDescription(Copy.Description);
		Tile->SetFallbackIcon(Copy.Icon);
		Tile->SetLocked(Spec.bLocked);
		Tile->SetGlyphFamily(ShownFamily);
		PlaceInColumns(Tile, Spec.Column, Spec.Span, Spec.Top, Spec.Height, Spec.bBottomRow);
		Tiles.Add(Tile);
	}
	FocusMap = Padel::Ui::MainMenuFocusMap();

	// Spec 7.2: on a fresh arrival (from Title) the tiles slide in from the left, staggered.
	// Coming back to the menu restores focus and enters as one, like any screen.
	bStaggerEntry = GetMenuRoot() && GetMenuRoot()->GetFlow().FocusOnArrival().empty() && !PadelUi::Options().bReduceMotion;
	bFreshEntry = bStaggerEntry;
	EntrySeconds = 0.0f;
	ApplyEntry();
}

UPadelMenuTile* UPadelMainMenuScreen::FindTile(FName Id) const
{
	return Cast<UPadelMenuTile>(FindElement(Id));
}

void UPadelMainMenuScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bStaggerEntry)
	{
		EntrySeconds += InDeltaTime;
		ApplyEntry();
	}
	// The hero's "A Play" chip follows the device in use (spec 5.5).
	if (ShownFamily != GetPadFamily())
	{
		ShownFamily = GetPadFamily();
		for (UPadelMenuTile* Tile : Tiles)
		{
			Tile->SetGlyphFamily(ShownFamily);
		}
	}
}

void UPadelMainMenuScreen::ApplyEntry()
{
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	bool bAllDone = true;
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		UPadelMenuTile* Tile = Tiles[Index];
		if (!Tile)
		{
			continue;
		}
		float T = 1.0f;
		if (bStaggerEntry && Motion.ScreenIn > 0.0f)
		{
			const float Delay = Padel::Ui::StaggerDelay(Index, Motion.Stagger, static_cast<int>(Motion.StaggerMax));
			T = FMath::Clamp((EntrySeconds - Delay) / Motion.ScreenIn, 0.0f, 1.0f);
		}
		bAllDone &= T >= 1.0f;
		const float Eased = Padel::Ui::EaseOutQuint(T);
		// Translation and opacity only: the tile's render scale is its own focus scale.
		Tile->SetRenderTranslation(FVector2D(-Motion.ScreenSlideIn * (1.0f - Eased), 0.0f));
		Tile->SetRenderOpacity(Eased);
	}
	if (bAllDone)
	{
		bStaggerEntry = false;
	}
}

void UPadelMainMenuScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	UPadelMenuRoot* Root = GetMenuRoot();
	if (!Element || !Root)
	{
		return;
	}
	const FString Id = Element->GetElementId().ToString();
	for (const Padel::Ui::FMainMenuTile& Spec : Padel::Ui::MainMenuTiles())
	{
		if (Id != UTF8_TO_TCHAR(Spec.Id))
		{
			continue;
		}
		if (Spec.bLocked)
		{
			// Spec 5.3, 6.13: a locked tile explains itself and goes nowhere.
			const UPadelMenuTile* Tile = FindTile(Element->GetElementId());
			Root->ShowToast(FText::Format(LOCTEXT("Main.Locked.Toast", "{0} is coming in a later update."),
				Tile ? Tile->GetLabel() : FText::GetEmpty()));
			return;
		}
		Root->ShowScreen(ToPadelScreen(Spec.Target), EUiNavKind::Forward);
		return;
	}
}

#undef LOCTEXT_NAMESPACE
