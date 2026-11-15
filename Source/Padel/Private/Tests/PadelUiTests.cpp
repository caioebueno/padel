// UI and presentation in the engine: widgets build in C++ and show the model's text, and
// the broadcast camera reads Data/Camera.json. The text and maths themselves are covered
// by the native tests (Tests/Native/UI).
#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Presentation/PadelBroadcastCamera.h"
#include "UI/PadelHudWidget.h"
#include "UI/PadelMenuWidget.h"
#include "UI/PadelScoreBugWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelUiTests
{
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}

		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelScoreBugTest, "Padel.UI.ScoreBug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelScoreBugTest::RunTest(const FString& Parameters)
{
	PadelUiTests::FTestWorld Test;
	UPadelScoreBugWidget* Bug = CreateWidget<UPadelScoreBugWidget>(Test.World, UPadelScoreBugWidget::StaticClass());
	if (!TestNotNull(TEXT("Score bug created"), Bug))
	{
		return false;
	}
	Bug->TakeWidget();

	Padel::Ui::FScoreView View;
	View.Sets = {{6, 4}};
	View.Games = {2, 3};
	View.Points = {"40", "40"};
	View.ServingTeam = 1;
	Bug->UpdateScore(View);

	TestEqual(TEXT("Team A points"), Bug->GetPointsText(0), FString(TEXT("40")));
	TestEqual(TEXT("Team A games per set"), Bug->GetGamesText(0), FString(TEXT("6 2")));
	TestEqual(TEXT("Team B games per set"), Bug->GetGamesText(1), FString(TEXT("4 3")));
	TestTrue(TEXT("Team B serves"), Bug->IsServing(1));
	TestFalse(TEXT("Team A doesn't"), Bug->IsServing(0));
	TestEqual(TEXT("Golden point tag at 40-40 (GDD 3.1)"), Bug->GetTagText(), FString(TEXT("GOLDEN POINT")));

	View.Points = {"15", "0"};
	View.bBreakPoint = false;
	Bug->UpdateScore(View);
	TestTrue(TEXT("No tag at 15-0"), Bug->GetTagText().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelHudMessagesTest, "Padel.UI.HudMessages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelHudMessagesTest::RunTest(const FString& Parameters)
{
	PadelUiTests::FTestWorld Test;
	UPadelHudWidget* Hud = CreateWidget<UPadelHudWidget>(Test.World, UPadelHudWidget::StaticClass());
	if (!TestNotNull(TEXT("HUD created"), Hud))
	{
		return false;
	}
	Hud->TakeWidget();
	TestNotNull(TEXT("HUD has a score bug"), Hud->GetScoreBug());

	Hud->ShowMessage(EPadelHudMessage::Callout, FText::FromString(TEXT("Let")), FLinearColor::White, 1.0f);
	TestEqual(TEXT("Callout shows"), Hud->GetMessageText(EPadelHudMessage::Callout), FString(TEXT("Let")));
	Hud->TickMessages(0.5f);
	TestEqual(TEXT("Still showing at 0.5 s"), Hud->GetMessageText(EPadelHudMessage::Callout), FString(TEXT("Let")));
	Hud->TickMessages(0.6f);
	TestTrue(TEXT("Hidden after 1 s"), Hud->GetMessageText(EPadelHudMessage::Callout).IsEmpty());

	int32 Chosen = INDEX_NONE;
	Hud->OnGoldenReceiverChosen.AddLambda([&Chosen](int32 PlayerId) { Chosen = PlayerId; });
	Hud->OpenGoldenPrompt(1, FText::GetEmpty());
	TestTrue(TEXT("Golden point prompt open"), Hud->IsGoldenPromptOpen());
	Hud->CloseGoldenPrompt();
	TestFalse(TEXT("Golden point prompt closed"), Hud->IsGoldenPromptOpen());
	TestEqual(TEXT("Closing doesn't choose"), Chosen, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMenuWidgetTest, "Padel.UI.Menu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMenuWidgetTest::RunTest(const FString& Parameters)
{
	PadelUiTests::FTestWorld Test;
	UPadelMenuWidget* Menu = CreateWidget<UPadelMenuWidget>(Test.World, UPadelMenuWidget::StaticClass());
	if (!TestNotNull(TEXT("Menu created"), Menu))
	{
		return false;
	}
	Menu->TakeWidget();
	Menu->SetTitle(FText::FromString(TEXT("QUICK MATCH")));
	Menu->SetOptions({FText::FromString(TEXT("Start match")), FText::FromString(TEXT("Back"))});
	TestEqual(TEXT("Title"), Menu->GetTitle(), FString(TEXT("QUICK MATCH")));
	TestEqual(TEXT("Two options"), Menu->GetOptionCount(), 2);
	Menu->SetOptionLabel(1, FText::FromString(TEXT("Main menu")));
	TestEqual(TEXT("Relabelled"), Menu->GetOptionLabel(1), FString(TEXT("Main menu")));

	int32 Chosen = INDEX_NONE;
	Menu->OnOptionChosen.AddLambda([&Chosen](int32 Index) { Chosen = Index; });
	Menu->ChooseOption(1);
	TestEqual(TEXT("Choosing reports the index"), Chosen, 1);

	// Rebuilding the options replaces the old buttons.
	Menu->SetOptions({FText::FromString(TEXT("Resume"))});
	TestEqual(TEXT("One option"), Menu->GetOptionCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelCameraSettingsTest, "Padel.Presentation.CameraSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelCameraSettingsTest::RunTest(const FString& Parameters)
{
	// Data/Camera.json is the source of truth (CLAUDE.md: tuning data is text), and must
	// stay inside GDD 9's ranges.
	const Padel::Presentation::FCameraSettings Settings = APadelBroadcastCamera::LoadSettings();
	TestTrue(TEXT("Height 7-9 m"), Settings.HeightM >= 7.0 && Settings.HeightM <= 9.0);
	TestTrue(TEXT("6-8 m behind the glass"), Settings.BehindGlassM >= 6.0 && Settings.BehindGlassM <= 8.0);
	TestTrue(TEXT("Tracking at most 1.5 m"), Settings.MaxTrackingM > 0.0 && Settings.MaxTrackingM <= 1.5);
	TestTrue(TEXT("Change-over move takes time"), Settings.EndTransitionSeconds > 0.0);
	return true;
}

#endif
