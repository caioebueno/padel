#include "UI/PadelHUD.h"

#include "Blueprint/UserWidget.h"
#include "Court/PadelCourtUnits.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "Gameplay/PadelPlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PadelLog.h"
#include "Presentation/PadelBroadcastCamera.h"
#include "Presentation/PadelMatchAudio.h"
#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelHudWidget.h"
#include "UI/PadelMenuWidget.h"
#include "UI/PadelUiStyle.h"

#define LOCTEXT_NAMESPACE "PadelHUD"

namespace
{
	constexpr float CalloutSeconds = 1.8f;
	constexpr float CueSeconds = 0.9f;
	constexpr float CommandSeconds = 1.6f;
	constexpr float CardSeconds = 3.0f;

	// Menu option indices, per screen.
	enum EMainOption : int32 { MainQuickMatch, MainQuit };
	enum ESetupOption : int32 { SetupDifficulty, SetupLength, SetupGolden, SetupPlayer, SetupStart, SetupBack };
	enum EPauseOption : int32 { PauseResume, PauseRestart, PauseQuit };
	enum EStatsOption : int32 { StatsPlayAgain, StatsMainMenu };

	FText Txt(const std::string& Text)
	{
		return PadelUiStyle::ToText(Text);
	}

	FLinearColor TimingColour(const std::string& Grade)
	{
		if (Grade == "PERFECT")
		{
			return PadelUiStyle::Good;
		}
		if (Grade == "EARLY" || Grade == "LATE")
		{
			return PadelUiStyle::Warning;
		}
		if (Grade == "MISS")
		{
			return PadelUiStyle::Bad;
		}
		return PadelUiStyle::Text;
	}
}

APadelHUD::APadelHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	MatchAudio = CreateDefaultSubobject<UPadelMatchAudio>(TEXT("MatchAudio"));
}

void APadelHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	HudWidget = CreateWidget<UPadelHudWidget>(PC, UPadelHudWidget::StaticClass());
	MenuWidget = CreateWidget<UPadelMenuWidget>(PC, UPadelMenuWidget::StaticClass());
	if (!HudWidget || !MenuWidget)
	{
		UE_LOG(LogPadel, Error, TEXT("Could not create the HUD widgets."));
		return;
	}
	HudWidget->AddToViewport(10);
	MenuWidget->AddToViewport(20);
	HudWidget->OnGoldenReceiverChosen.AddUObject(this, &APadelHUD::HandleGoldenReceiver);
	MenuWidget->OnOptionChosen.AddUObject(this, &APadelHUD::HandleMenuOption);
	MenuWidget->OnOptionCycled.AddUObject(this, &APadelHUD::HandleMenuCycle);
	MenuWidget->OnBack.AddUObject(this, &APadelHUD::HandleMenuBack);

	ShowScreen(EPadelScreen::MainMenu);
}

void APadelHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TryBindInput();

	if (Screen != EPadelScreen::Match)
	{
		return;
	}
	// Hold the match while the human picks the golden point receiver (GDD 3.1).
	const bool bWaitingForChoice = HudWidget && HudWidget->IsGoldenPromptOpen();
	if (DemoMatch && !bWaitingForChoice)
	{
		DemoMatch->Step(DeltaSeconds);
	}
	if (MatchEndCountdown > 0.0f)
	{
		MatchEndCountdown -= DeltaSeconds;
		if (MatchEndCountdown <= 0.0f)
		{
			EndMatch();
		}
	}
}

// --- Screens ---------------------------------------------------------------

void APadelHUD::ShowScreen(EPadelScreen NewScreen)
{
	Screen = NewScreen;
	if (!MenuWidget || !HudWidget)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	const bool bMenu = NewScreen != EPadelScreen::Match;
	if (PC)
	{
		PC->SetPause(NewScreen == EPadelScreen::Paused);
	}
	HudWidget->SetVisibility(NewScreen == EPadelScreen::Match || NewScreen == EPadelScreen::Paused
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	MenuWidget->SetVisibility(bMenu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	switch (NewScreen)
	{
	case EPadelScreen::MainMenu:
		BuildMainMenu();
		break;
	case EPadelScreen::MatchSetup:
		BuildSetupMenu();
		break;
	case EPadelScreen::Paused:
		BuildPauseMenu();
		break;
	case EPadelScreen::Stats:
		BuildStatsScreen();
		break;
	case EPadelScreen::Match:
		break;
	}
	SetMenuInput(bMenu);
}

void APadelHUD::SetMenuInput(bool bMenu)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}
	PC->SetShowMouseCursor(bMenu);
	if (bMenu)
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
		PC->SetInputMode(Mode);
		MenuWidget->FocusOption(0);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void APadelHUD::BuildMainMenu()
{
	// "Padel Street" is the working title (GDD 13 decision 2).
	MenuWidget->SetTitle(LOCTEXT("Title", "PADEL STREET"));
	MenuWidget->SetBody(LOCTEXT("MainBody", "2v2 padel. You and an AI partner against two AI players."));
	MenuWidget->SetTable({});
	MenuWidget->SetOptions({LOCTEXT("QuickMatch", "Quick Match"), LOCTEXT("Quit", "Quit")});
}

void APadelHUD::BuildSetupMenu()
{
	MenuWidget->SetTitle(LOCTEXT("SetupTitle", "QUICK MATCH"));
	MenuWidget->SetBody(FText::Format(LOCTEXT("SetupBody", "{0} vs {1}. Left and right change an option."),
		Txt(Padel::Ui::TeamName(0)), Txt(Padel::Ui::TeamName(1))));
	MenuWidget->SetTable({});
	MenuWidget->SetOptions({FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(),
		LOCTEXT("Start", "Start match"), LOCTEXT("Back", "Back")});
	RefreshSetupLabels();
}

void APadelHUD::RefreshSetupLabels()
{
	MenuWidget->SetOptionLabel(SetupDifficulty, FText::Format(LOCTEXT("Difficulty", "Difficulty: {0}"), Txt(Setup.DifficultyLabel())));
	MenuWidget->SetOptionLabel(SetupLength, FText::Format(LOCTEXT("Length", "Match: {0}"), Txt(Setup.LengthLabel())));
	MenuWidget->SetOptionLabel(SetupGolden, FText::Format(LOCTEXT("Golden", "Golden point: {0}"), Txt(Setup.GoldenPointLabel())));
	MenuWidget->SetOptionLabel(SetupPlayer, FText::Format(LOCTEXT("Player", "You play: {0}"), Txt(Setup.PlayerLabel())));
}

void APadelHUD::BuildPauseMenu()
{
	MenuWidget->SetTitle(LOCTEXT("PausedTitle", "PAUSED"));
	MenuWidget->SetBody(Txt(Padel::Ui::SummaryLine(LastScore)));
	MenuWidget->SetTable({});
	MenuWidget->SetOptions({LOCTEXT("Resume", "Resume"), LOCTEXT("Restart", "Restart match"), LOCTEXT("QuitToMenu", "Quit to main menu")});
}

void APadelHUD::BuildStatsScreen()
{
	const int32 Winner = Stats.MatchWinner;
	MenuWidget->SetTitle(Winner >= 0
		? FText::Format(LOCTEXT("StatsWinner", "{0} WIN"), Txt(Padel::Ui::TeamName(Winner)))
		: LOCTEXT("StatsTitle", "MATCH STATS"));
	MenuWidget->SetBody(FText::Format(LOCTEXT("StatsBody", "{0}  -  average rally {1} shots  -  {2}% of points had a shot off the glass"),
		Txt(Padel::Ui::FinalLine(Stats.FinalSets)),
		FText::AsNumber(FMath::RoundToInt(Stats.AverageRally() * 10.0) / 10.0),
		FText::AsNumber(FMath::RoundToInt(Stats.GlassPointShare() * 100.0))));

	TArray<TArray<FText>> Rows;
	Rows.Add({FText::GetEmpty(), Txt(Padel::Ui::PairLabel(0)), Txt(Padel::Ui::PairLabel(1))});
	for (const Padel::Ui::FStatRow& Row : Stats.SummaryRows())
	{
		Rows.Add({Txt(Row.Label), FText::AsNumber(Row.TeamA), FText::AsNumber(Row.TeamB)});
	}
	// "How the points ended" (Agent 7 brief), labelled from the reason codes.
	for (const Padel::Ui::FStatRow& Row : Stats.EndingRows())
	{
		Rows.Add({FText::Format(LOCTEXT("WonBy", "Won by: {0}"), Txt(Row.Label)), FText::AsNumber(Row.TeamA), FText::AsNumber(Row.TeamB)});
	}
	MenuWidget->SetTable(Rows);
	MenuWidget->SetOptions({LOCTEXT("PlayAgain", "Play again"), LOCTEXT("MainMenu", "Main menu")});
}

void APadelHUD::HandleMenuOption(int32 Index)
{
	switch (Screen)
	{
	case EPadelScreen::MainMenu:
		if (Index == MainQuickMatch)
		{
			ShowScreen(EPadelScreen::MatchSetup);
		}
		else if (Index == MainQuit)
		{
			UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
		}
		break;
	case EPadelScreen::MatchSetup:
		if (Index <= SetupPlayer)
		{
			HandleMenuCycle(Index, 1);
		}
		else if (Index == SetupStart)
		{
			StartMatch();
		}
		else
		{
			ShowScreen(EPadelScreen::MainMenu);
		}
		break;
	case EPadelScreen::Paused:
		if (Index == PauseResume)
		{
			ShowScreen(EPadelScreen::Match);
		}
		else if (Index == PauseRestart)
		{
			StartMatch();
		}
		else
		{
			DemoMatch.Reset();
			ShowScreen(EPadelScreen::MainMenu);
		}
		break;
	case EPadelScreen::Stats:
		if (Index == StatsPlayAgain)
		{
			StartMatch();
		}
		else
		{
			ShowScreen(EPadelScreen::MainMenu);
		}
		break;
	case EPadelScreen::Match:
		break;
	}
}

void APadelHUD::HandleMenuCycle(int32 Index, int32 Step)
{
	if (Screen != EPadelScreen::MatchSetup)
	{
		return;
	}
	switch (Index)
	{
	case SetupDifficulty:
		Setup.CycleDifficulty(Step);
		break;
	case SetupLength:
		Setup.CycleLength();
		break;
	case SetupGolden:
		Setup.ToggleGoldenPoint();
		break;
	case SetupPlayer:
		Setup.CyclePlayer();
		break;
	default:
		return;
	}
	RefreshSetupLabels();
}

void APadelHUD::HandleMenuBack()
{
	switch (Screen)
	{
	case EPadelScreen::MatchSetup:
	case EPadelScreen::Stats:
		ShowScreen(EPadelScreen::MainMenu);
		break;
	case EPadelScreen::Paused:
		ShowScreen(EPadelScreen::Match);
		break;
	case EPadelScreen::MainMenu:
	case EPadelScreen::Match:
		break;
	}
}

// --- Match -------------------------------------------------------------------

void APadelHUD::StartMatch()
{
	Stats = Padel::Ui::FMatchStats();
	LastScore = Padel::Ui::FScoreView();
	LastScore.bGoldenPoint = Setup.bGoldenPoint;
	MatchEndCountdown = -1.0f;
	if (APadelBroadcastCamera* Camera = FindCamera())
	{
		Camera->SetHumanEnd(1);
	}

	// Stand-in until the simulation lands (Agent 2): a scripted match on the same events.
	Padel::Ui::FDemoMatchOptions Options;
	Options.SetsToWin = Setup.SetsToWin;
	Options.bGoldenPoint = Setup.bGoldenPoint;
	Options.Seed = 1000u + (++MatchCount);
	DemoMatch = MakeUnique<Padel::Ui::FDemoMatch>(Options, *this);

	ShowScreen(EPadelScreen::Match);
	HudWidget->UpdateScore(LastScore);
	DemoMatch->Start();
}

void APadelHUD::EndMatch()
{
	MatchEndCountdown = -1.0f;
	DemoMatch.Reset();
	ShowScreen(EPadelScreen::Stats);
}

void APadelHUD::OnServeStarted(int ServerId)
{
	Stats.OnServeStarted();
	HudWidget->ShowMessage(EPadelHudMessage::Callout,
		FText::Format(LOCTEXT("ToServe", "{0} to serve"), Txt(Padel::Ui::Surname(ServerId))), PadelUiStyle::Text, CalloutSeconds);
}

void APadelHUD::OnServeFault(std::string_view Fault)
{
	HudWidget->ShowMessage(EPadelHudMessage::Callout, Txt(Padel::Ui::ServeFaultLabel(Fault)), PadelUiStyle::Accent, CalloutSeconds);
	if (MatchAudio)
	{
		MatchAudio->Play(EPadelMatchSound::Whistle, FVector::ZeroVector, false);
	}
}

void APadelHUD::OnServeLet()
{
	HudWidget->ShowMessage(EPadelHudMessage::Callout, LOCTEXT("Let", "Let"), PadelUiStyle::Accent, CalloutSeconds);
}

void APadelHUD::OnBallHit(int PlayerId, std::string_view ShotType, std::string_view TimingGrade, double SpeedMps)
{
	Stats.OnBallHit(PlayerId, ShotType, TimingGrade);
	if (MatchAudio)
	{
		MatchAudio->Play(EPadelMatchSound::RacketHit, FVector::ZeroVector, false);
	}
	// GDD 5.1: the timing cue is for the human's own shots only.
	if (PlayerId == Setup.HumanPlayerId)
	{
		const std::string Grade = Padel::Ui::NormaliseCode(TimingGrade);
		HudWidget->ShowMessage(EPadelHudMessage::TimingCue, Txt(Padel::Ui::TimingLabel(Grade)), TimingColour(Grade), CueSeconds);
	}
}

void APadelHUD::OnBallBounced(std::string_view Surface, int Half, const Padel::Ui::FUiVec3& PositionM)
{
	Stats.OnBallBounced(Surface);
	const FVector Metres(PositionM.X, PositionM.Y, PositionM.Z);
	if (APadelBroadcastCamera* Camera = FindCamera())
	{
		Camera->SetBallPositionMetres(Metres);
	}
	if (MatchAudio)
	{
		MatchAudio->Play(UPadelMatchAudio::SoundForSurface(PadelUiStyle::ToFString(std::string(Surface))), Padel::MetresToWorld(Metres));
	}
}

void APadelHUD::OnBallLeftCourt(const Padel::Ui::FUiVec3& PositionM)
{
}

void APadelHUD::OnPointEnded(int WinningTeam, std::string_view Reason, int LastHitterId)
{
	Stats.OnPointEnded(WinningTeam, Reason);
	HudWidget->CloseGoldenPrompt();
	HudWidget->ShowMessage(EPadelHudMessage::Callout,
		FText::Format(LOCTEXT("PointEnded", "{0}  -  {1}"), Txt(Padel::Ui::ReasonLabel(Reason)), Txt(Padel::Ui::TeamName(WinningTeam))),
		PadelUiStyle::TeamColour(WinningTeam), CalloutSeconds);
	if (MatchAudio)
	{
		const bool bBig = LastScore.bBreakPoint || LastScore.bSetPoint || LastScore.bMatchPoint || Padel::Ui::IsGoldenPointNow(LastScore);
		MatchAudio->Play(bBig ? EPadelMatchSound::CrowdBigPoint : EPadelMatchSound::CrowdPoint, FVector::ZeroVector, false);
	}
}

void APadelHUD::OnScoreChanged(const Padel::Ui::FScoreView& Score)
{
	LastScore = Score;
	HudWidget->UpdateScore(Score);
	// The human's team (Team A) is always at the bottom of the screen (GDD 9).
	if (APadelBroadcastCamera* Camera = FindCamera())
	{
		Camera->SetHumanEnd(Score.TeamAEnd);
	}
	if (Score.bMatchOver)
	{
		Stats.OnMatchWon(Score.Winner, Score.Sets);
	}
	if (!PendingCardTitle.IsEmpty())
	{
		// game_won / set_won / change_of_ends arrive before the new score (Agent 2's event
		// order), so the card waits for it.
		HudWidget->ShowScoreCard(PendingCardTitle, Score, CardSeconds);
		PendingCardTitle = FText::GetEmpty();
	}
}

void APadelHUD::OnGameWon(int Team)
{
	PendingCardTitle = FText::Format(LOCTEXT("GameWon", "GAME  {0}"), Txt(Padel::Ui::TeamName(Team)));
}

void APadelHUD::OnSetWon(int Team)
{
	PendingCardTitle = FText::Format(LOCTEXT("SetWon", "SET  {0}"), Txt(Padel::Ui::TeamName(Team)));
}

void APadelHUD::OnMatchWon(int Team)
{
	PendingCardTitle = FText::Format(LOCTEXT("MatchWon", "MATCH  {0}"), Txt(Padel::Ui::TeamName(Team)));
	MatchEndCountdown = MatchEndDelaySeconds;
}

void APadelHUD::OnChangeOfEnds()
{
	// Keep a game/set title if there is one; the camera moves on the next score (GDD 9).
	if (PendingCardTitle.IsEmpty())
	{
		PendingCardTitle = LOCTEXT("ChangeOfEnds", "CHANGE OF ENDS");
	}
}

void APadelHUD::OnGoldenPointStarted(int ReceivingTeam)
{
	HudWidget->ShowMessage(EPadelHudMessage::Callout, Txt(std::string(Padel::Ui::GoldenPointTag)), PadelUiStyle::Accent, CalloutSeconds);
	if (ReceivingTeam != Padel::Ui::TeamA)
	{
		return;
	}
	// GDD 3.1: the human pair chooses who receives. Input mode first, so the prompt keeps focus.
	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->SetInputMode(FInputModeGameAndUI());
	}
	HudWidget->OpenGoldenPrompt(Setup.HumanPlayerId,
		FText::Format(LOCTEXT("GoldenHint", "Choose with the stick, confirm with {0}"), ButtonLabel(EPadelInputAction::ShotDrive)));
}

void APadelHUD::HandleGoldenReceiver(int32 PlayerId)
{
	// Agent 2: FMatchSimulation::ChooseGoldenPointReceiver(PlayerId) once the sim is wired.
	if (DemoMatch)
	{
		DemoMatch->ChooseGoldenPointReceiver(PlayerId);
	}
	HudWidget->ShowMessage(EPadelHudMessage::Callout,
		FText::Format(LOCTEXT("Receives", "{0} receives"), Txt(Padel::Ui::Surname(PlayerId))), PadelUiStyle::Accent, CalloutSeconds);
	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

// --- Input -------------------------------------------------------------------

void APadelHUD::TryBindInput()
{
	if (bInputBound)
	{
		return;
	}
	// The controller builds its input config in its BeginPlay, which may run after ours.
	APadelPlayerController* PC = Cast<APadelPlayerController>(GetOwningPlayerController());
	UPadelInputConfig* Config = PC ? PC->GetInputConfig() : nullptr;
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	if (!Config || !Input)
	{
		return;
	}
	Input->BindAction(Config->GetAction(EPadelInputAction::Pause), ETriggerEvent::Started, this, &APadelHUD::HandlePausePressed);
	Input->BindAction(Config->GetAction(EPadelInputAction::CmdNet), ETriggerEvent::Started, this, &APadelHUD::HandleCmdNet);
	Input->BindAction(Config->GetAction(EPadelInputAction::CmdBack), ETriggerEvent::Started, this, &APadelHUD::HandleCmdBack);
	Input->BindAction(Config->GetAction(EPadelInputAction::CmdMiddle), ETriggerEvent::Started, this, &APadelHUD::HandleCmdMiddle);
	Input->BindAction(Config->GetAction(EPadelInputAction::CmdYours), ETriggerEvent::Started, this, &APadelHUD::HandleCmdYours);
	bInputBound = true;
}

void APadelHUD::HandlePausePressed(const FInputActionInstance& Instance)
{
	if (Screen == EPadelScreen::Match)
	{
		ShowScreen(EPadelScreen::Paused);
	}
}

void APadelHUD::HandleCmdNet(const FInputActionInstance& Instance)
{
	ShowPartnerCommand(EPadelInputAction::CmdNet);
}

void APadelHUD::HandleCmdBack(const FInputActionInstance& Instance)
{
	ShowPartnerCommand(EPadelInputAction::CmdBack);
}

void APadelHUD::HandleCmdMiddle(const FInputActionInstance& Instance)
{
	ShowPartnerCommand(EPadelInputAction::CmdMiddle);
}

void APadelHUD::HandleCmdYours(const FInputActionInstance& Instance)
{
	ShowPartnerCommand(EPadelInputAction::CmdYours);
}

void APadelHUD::ShowPartnerCommand(EPadelInputAction Action)
{
	if (Screen != EPadelScreen::Match || !HudWidget)
	{
		return;
	}
	FText Command;
	switch (Action)
	{
	case EPadelInputAction::CmdNet:
		Command = LOCTEXT("CmdNet", "Go to the net");
		break;
	case EPadelInputAction::CmdBack:
		Command = LOCTEXT("CmdBack", "Stay back");
		break;
	case EPadelInputAction::CmdMiddle:
		Command = LOCTEXT("CmdMiddle", "Take the middle");
		break;
	case EPadelInputAction::CmdYours:
		Command = LOCTEXT("CmdYours", "Yours");
		break;
	default:
		return;
	}
	HudWidget->ShowMessage(EPadelHudMessage::PartnerCommand,
		FText::Format(LOCTEXT("PartnerCommand", "{0}   Partner: {1}"), ButtonLabel(Action), Command), PadelUiStyle::Text, CommandSeconds);
}

FText APadelHUD::ButtonLabel(EPadelInputAction Action) const
{
	const APadelPlayerController* PC = Cast<APadelPlayerController>(GetOwningPlayerController());
	const UPadelInputConfig* Config = PC ? PC->GetInputConfig() : nullptr;
	if (!Config || !Config->GetMatchContext())
	{
		return FText::GetEmpty();
	}
	// GDD 7: name the button on the pad in use (Xbox, PlayStation or Switch), else the key.
	Padel::Ui::EPadFamily Family = Padel::Ui::EPadFamily::KeyboardMouse;
	if (const UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get())
	{
		const FHardwareDeviceIdentifier Device = Devices->GetMostRecentlyUsedHardwareDevice(PC->GetPlatformUserId());
		Family = Padel::Ui::FamilyFromDeviceName(PadelUiStyle::ToUtf8(Device.HardwareDeviceIdentifier.ToString()));
	}
	const bool bPad = Family != Padel::Ui::EPadFamily::KeyboardMouse;
	const UInputAction* Wanted = Config->GetAction(Action);
	for (const FEnhancedActionKeyMapping& Mapping : Config->GetMatchContext()->GetMappings())
	{
		if (Mapping.Action != Wanted || Mapping.Key.IsGamepadKey() != bPad)
		{
			continue;
		}
		if (bPad)
		{
			const std::string Label = Padel::Ui::GamepadKeyLabel(PadelUiStyle::ToUtf8(Mapping.Key.GetFName().ToString()), Family);
			if (!Label.empty())
			{
				return Txt(Label);
			}
		}
		return Mapping.Key.GetDisplayName();
	}
	return FText::GetEmpty();
}

APadelBroadcastCamera* APadelHUD::FindCamera() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<APadelBroadcastCamera> It(World);
	return It ? *It : nullptr;
}

#undef LOCTEXT_NAMESPACE
