// The menu's screens and the per-screen hint item, shared by the HUD, the menu root and
// every screen (docs/ui/MENU_SPEC.md section 3). Shared contract: adding or reordering a
// screen changes Padel::Ui::EUiScreen too, in its own PR.

#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/Model/PadelUiScreenFlow.h"

#include "PadelScreenTypes.generated.h"

/** Mirrors Padel::Ui::EUiScreen, value for value. Confirm dialogs are layers, not screens. */
UENUM(BlueprintType)
enum class EPadelScreen : uint8
{
	Title,
	MainMenu,
	MatchSetup,
	Versus,
	Match,
	Paused,
	Controls,
	Settings,
	Stats,
	Practice,
	PracticePaused,
};

static_assert(static_cast<int32>(EPadelScreen::PracticePaused) + 1 == static_cast<int32>(Padel::Ui::EUiScreen::Count),
	"EPadelScreen must mirror Padel::Ui::EUiScreen");

inline Padel::Ui::EUiScreen ToUiScreen(EPadelScreen Screen)
{
	return static_cast<Padel::Ui::EUiScreen>(Screen);
}

inline EPadelScreen ToPadelScreen(Padel::Ui::EUiScreen Screen)
{
	return static_cast<EPadelScreen>(Screen);
}

/** One button hint (spec 6.1). Only live actions get one: a dead action is removed, not greyed. */
struct FPadelHint
{
	Padel::Ui::EUiInput Input = Padel::Ui::EUiInput::None;
	FText Label;
};

/** A confirm dialog (spec 6.12, 7.10). The safe option sits on the right and has focus; B picks it. */
struct FPadelConfirm
{
	FText Title;
	FText Body;
	FText ActionLabel;
	FText SafeLabel;
	bool bDanger = false;
	TFunction<void()> OnAction;
	TFunction<void()> OnSafe;
};
