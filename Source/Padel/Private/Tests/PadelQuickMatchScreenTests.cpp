// The Quick Match setup and Versus screens in the engine (docs/ui/MENU_SPEC.md 7.3, 7.4,
// 11.5): the cards read Data/Players.json, X swaps from anywhere, every focus path in
// spec 7.3's table works, Start reaches Versus, and Versus skips and auto-advances once.
// The numbers and rules themselves are covered without the engine in Tests/Native/UI.
#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UI/Model/PadelUiQuickMatch.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Screens/PadelSetupScreen.h"
#include "UI/Screens/PadelVersusScreen.h"
#include "UI/Widgets/PadelPlayerCard.h"
#include "UI/Widgets/PadelSegmented.h"
#include "UI/Widgets/PadelSelector.h"
#include "UI/Widgets/PadelToggle.h"

#if WITH_DEV_AUTOMATION_TESTS

using Padel::Ui::EUiInput;

namespace PadelQuickMatchTests
{
	struct FQuickMatchWorld
	{
		UWorld* World = nullptr;

		FQuickMatchWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}

		~FQuickMatchWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	UPadelMenuRoot* MakeRoot(UWorld* World)
	{
		UPadelMenuRoot* Root = CreateWidget<UPadelMenuRoot>(World, UPadelMenuRoot::StaticClass());
		if (Root)
		{
			Root->TakeWidget();
		}
		return Root;
	}

	void Unlock(UPadelMenuRoot* Root)
	{
		if (UPadelScreenWidget* Widget = Root->GetScreenWidget())
		{
			Widget->SkipEntryAnimation();
		}
	}

	void Show(UPadelMenuRoot* Root, EPadelScreen Screen)
	{
		Root->ShowScreen(Screen);
		Unlock(Root);
	}

	void Press(UPadelMenuRoot* Root, EUiInput Input)
	{
		Root->HandleInput(Input, false);
		Unlock(Root);
	}

	FString FocusOf(UPadelMenuRoot* Root)
	{
		return Root->GetScreenWidget() ? Root->GetScreenWidget()->GetFocusedId().ToString() : FString();
	}

	bool HasHint(const TArray<FPadelHint>& Hints, EUiInput Input, const FString& Label)
	{
		return Hints.ContainsByPredicate([&](const FPadelHint& Hint)
		{
			return Hint.Input == Input && Hint.Label.ToString() == Label;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSetupCardsTest, "Padel.UI.Setup.Cards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSetupCardsTest::RunTest(const FString& Parameters)
{
	using namespace PadelQuickMatchTests;
	FQuickMatchWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MatchSetup);
	UPadelSetupScreen* Setup = Cast<UPadelSetupScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Match setup is the real screen"), Setup))
	{
		return false;
	}
	TestEqual(TEXT("Default focus is Start match (spec 7.3)"), FocusOf(Root), FString(Padel::Ui::SetupStartId));

	UPadelPlayerCard* You = Setup->GetPairCard(0);
	UPadelPlayerCard* Partner = Setup->GetPairCard(1);
	if (!TestNotNull(TEXT("Your card"), You) || !TestNotNull(TEXT("Partner card"), Partner))
	{
		return false;
	}
	TestEqual(TEXT("Ventara is the default human, on the left"), You->GetPlayerId(), 1);
	TestTrue(TEXT("Your card is Selected"), You->IsSelectedCard());
	TestEqual(TEXT("The partner card says Partner"), static_cast<int32>(Partner->GetRole()), static_cast<int32>(EPadelCardRole::Partner));
	// Spec 6.6: the equal-weight overall from Data/Players.json, and the averaged stats.
	TestEqual(TEXT("Ventara's overall"), You->GetOverallText(), FString(TEXT("77")));
	TestEqual(TEXT("Arunde's overall"), Partner->GetOverallText(), FString(TEXT("78")));
	TestEqual(TEXT("Ventara's Attack"), You->GetStatText(0), FString(TEXT("83")));
	TestTrue(TEXT("Profile line in words"), You->GetProfileText().Contains(TEXT("Left side")));
	TestTrue(TEXT("Profile line has the play style"), You->GetProfileText().Contains(TEXT("Power play")));

	UPadelPlayerCard* Opponent = Setup->GetOpponentCard(0);
	if (TestNotNull(TEXT("Opponent card"), Opponent))
	{
		TestFalse(TEXT("Opponent cards are small"), Opponent->IsLarge());
		TestEqual(TEXT("Corvisa's overall"), Opponent->GetOverallText(), FString(TEXT("78")));
		TestEqual(TEXT("Opponents have no badge"), static_cast<int32>(Opponent->GetRole()), static_cast<int32>(EPadelCardRole::None));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSetupSwapAnywhereTest, "Padel.UI.Setup.SwapAnywhere",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSetupSwapAnywhereTest::RunTest(const FString& Parameters)
{
	using namespace PadelQuickMatchTests;
	FQuickMatchWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MatchSetup);
	UPadelSetupScreen* Setup = Cast<UPadelSetupScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Match setup is the real screen"), Setup))
	{
		return false;
	}
	// Spec 11.5: X swaps the player from every focusable element on Setup.
	for (const std::string& Id : Setup->GetFocusMap().Ids())
	{
		const FString Name = UTF8_TO_TCHAR(Id.c_str());
		TestTrue(*FString::Printf(TEXT("Focus %s"), *Name), Setup->FocusElement(FName(*Name)));
		const int32 Before = Setup->Setup().HumanPlayerId;
		TestTrue(*FString::Printf(TEXT("%s shows X Swap player"), *Name),
			HasHint(Setup->GetHints(), EUiInput::Secondary, TEXT("Swap player")));
		Press(Root, EUiInput::Secondary);
		TestEqual(*FString::Printf(TEXT("X on %s swaps the human"), *Name), Setup->Setup().HumanPlayerId, Before ^ 1);
		TestEqual(*FString::Printf(TEXT("X on %s keeps focus"), *Name), FocusOf(Root), Name);
		// The cards don't move; the YOU badge and Selected frame cross over.
		TestEqual(TEXT("Left card stays Ventara"), Setup->GetPairCard(0)->GetPlayerId(), 1);
		TestEqual(TEXT("YOU follows the human"),
			Setup->GetPairCard(0)->IsSelectedCard(), Setup->Setup().HumanPlayerId == 1);
		TestEqual(TEXT("Exactly one card is Selected"),
			Setup->GetPairCard(0)->IsSelectedCard() != Setup->GetPairCard(1)->IsSelectedCard(), true);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSetupFocusPathsTest, "Padel.UI.Setup.FocusPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSetupFocusPathsTest::RunTest(const FString& Parameters)
{
	using namespace PadelQuickMatchTests;
	FQuickMatchWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MatchSetup);
	UPadelSetupScreen* Setup = Cast<UPadelSetupScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Match setup is the real screen"), Setup))
	{
		return false;
	}
	TestTrue(TEXT("A Start match on Start"), HasHint(Setup->GetHints(), EUiInput::Confirm, TEXT("Start match")));

	// Spec 7.3's table: one wrapping list.
	Press(Root, EUiInput::Down);
	TestEqual(TEXT("Start, Down wraps to Difficulty"), FocusOf(Root), FString(Padel::Ui::SetupDifficultyId));
	TestTrue(TEXT("A Change on a rules row"), HasHint(Setup->GetHints(), EUiInput::Confirm, TEXT("Change")));
	Press(Root, EUiInput::Up);
	TestEqual(TEXT("Difficulty, Up wraps to Start"), FocusOf(Root), FString(Padel::Ui::SetupStartId));
	Press(Root, EUiInput::Up);
	TestEqual(TEXT("Start, Up -> Golden point"), FocusOf(Root), FString(Padel::Ui::SetupGoldenPointId));
	Press(Root, EUiInput::Up);
	TestEqual(TEXT("Golden point, Up -> Format"), FocusOf(Root), FString(Padel::Ui::SetupFormatId));
	Press(Root, EUiInput::Up);
	TestEqual(TEXT("Format, Up -> Difficulty"), FocusOf(Root), FString(Padel::Ui::SetupDifficultyId));

	// Left and right belong to the focused control's value, and land in the setup.
	Press(Root, EUiInput::Right);
	TestEqual(TEXT("Right steps Difficulty to Hard"), static_cast<int32>(Setup->Setup().Difficulty),
		static_cast<int32>(Padel::Ui::EDifficulty::Hard));
	TestEqual(TEXT("Right doesn't move focus"), FocusOf(Root), FString(Padel::Ui::SetupDifficultyId));
	Press(Root, EUiInput::Down);
	Press(Root, EUiInput::Left);
	TestEqual(TEXT("Left picks 1 set"), Setup->Setup().SetsToWin, 1);
	Press(Root, EUiInput::Down);
	TestEqual(TEXT("Format, Down -> Golden point"), FocusOf(Root), FString(Padel::Ui::SetupGoldenPointId));
	Press(Root, EUiInput::Left);
	TestFalse(TEXT("Left turns golden point off"), Setup->Setup().bGoldenPoint);
	Press(Root, EUiInput::Down);
	TestEqual(TEXT("Golden point, Down -> Start"), FocusOf(Root), FString(Padel::Ui::SetupStartId));

	// Start match -> Versus (spec 7.3), whose rules line reads the choices just made.
	Press(Root, EUiInput::Confirm);
	TestEqual(TEXT("Start match goes to Versus"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Versus));

	// B on Setup goes back to the main menu, with the choices kept for the session.
	Show(Root, EPadelScreen::MatchSetup);
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("B on Setup -> Main menu"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::MainMenu));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelVersusCardTest, "Padel.UI.Versus.Card",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelVersusCardTest::RunTest(const FString& Parameters)
{
	using namespace PadelQuickMatchTests;
	FQuickMatchWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::Versus);
	UPadelVersusScreen* Versus = Cast<UPadelVersusScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Versus is the real screen"), Versus))
	{
		return false;
	}
	TestEqual(TEXT("Rules line"), Versus->GetRulesLine().ToString(), FString(TEXT("Medium · Best of 3 · Golden point")));
	TestEqual(TEXT("The human's name leads Lagoon"), Versus->GetPairText(0), FString(TEXT("Ventara · Arunde")));
	TestEqual(TEXT("Ember in id order"), Versus->GetPairText(1), FString(TEXT("Corvisa · Halvaro")));
	const TArray<FPadelHint> Hints = Versus->GetHints();
	TestTrue(TEXT("A Skip once ready"), HasHint(Hints, EUiInput::Confirm, TEXT("Skip")));
	TestFalse(TEXT("No B on Versus (spec 3)"), Hints.ContainsByPredicate([](const FPadelHint& Hint)
	{
		return Hint.Input == EUiInput::Back;
	}));
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("B does nothing"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Versus));

	// Auto-advance after Input.VersusSeconds, once.
	const float CardSeconds = PadelUi::Theme().Input.VersusSeconds;
	Versus->AdvanceCard(CardSeconds * 0.5f);
	TestFalse(TEXT("Not started halfway"), Versus->HasStartedMatch());
	Versus->AdvanceCard(CardSeconds);
	TestTrue(TEXT("Started when the card's time is up"), Versus->HasStartedMatch());
	TestFalse(TEXT("The loading indicator stays hidden for a ready match"), Versus->IsLoadingShown());
	TestEqual(TEXT("No Skip after the start"), Versus->GetHints().Num(), 0);

	// A skips at once. (Back to the main menu first, so the card is a fresh one.)
	Show(Root, EPadelScreen::MainMenu);
	Show(Root, EPadelScreen::Versus);
	Versus = Cast<UPadelVersusScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Versus again"), Versus))
	{
		return false;
	}
	TestFalse(TEXT("A fresh card hasn't started"), Versus->HasStartedMatch());
	Press(Root, EUiInput::Confirm);
	TestTrue(TEXT("A skips to the match"), Versus->HasStartedMatch());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelQuickMatchThreePressesTest, "Padel.UI.Flow.QuickMatchThreePresses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelQuickMatchThreePressesTest::RunTest(const FString& Parameters)
{
	using namespace PadelQuickMatchTests;
	FQuickMatchWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	// Spec 11.5: any button, A, A from the Title reaches the Versus screen.
	Show(Root, EPadelScreen::Title);
	Press(Root, EUiInput::Confirm);
	Press(Root, EUiInput::Confirm);
	Press(Root, EUiInput::Confirm);
	TestEqual(TEXT("Three presses reach Versus"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Versus));
	return true;
}

#endif
