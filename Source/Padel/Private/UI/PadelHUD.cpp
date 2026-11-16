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
#include "Presentation/PadelMatchEvents.h"
#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelHudWidget.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/PadelUiStyle.h"

#define LOCTEXT_NAMESPACE "PadelHUD"

namespace
{
	constexpr float CalloutSeconds = 1.8f;
	constexpr float CueSeconds = 0.9f;
	constexpr float CommandSeconds = 1.6f;
	constexpr float CardSeconds = 3.0f;

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
	MenuRoot = CreateWidget<UPadelMenuRoot>(PC, UPadelMenuRoot::StaticClass());
	if (!HudWidget || !MenuRoot)
	{
		UE_LOG(LogPadel, Error, TEXT("Could not create the HUD widgets."));
		return;
	}
	HudWidget->AddToViewport(10);
	MenuRoot->AddToViewport(20);
	HudWidget->OnGoldenReceiverChosen.AddUObject(this, &APadelHUD::HandleGoldenReceiver);
	MenuRoot->SetHost(this);
	MenuRoot->OnScreenChanged.AddUObject(this, &APadelHUD::HandleScreenChanged);

	if (UWorld* World = GetWorld())
	{
		if (UPadelMatchEvents* Events = World->GetSubsystem<UPadelMatchEvents>())
		{
			MatchEvents.Bind(*Events, *this, Setup.bGoldenPoint);
		}
	}

	// Spec 7.1: the game opens on the title, which learns the player's device.
	ShowScreen(EPadelScreen::Title);
}

void APadelHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MatchEvents.Unbind();
	DemoMatch.Reset();
	Super::EndPlay(EndPlayReason);
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
	if (MenuRoot)
	{
		// The root broadcasts the change back to HandleScreenChanged.
		MenuRoot->ShowScreen(NewScreen);
	}
	else
	{
		Screen = NewScreen;
	}
}

void APadelHUD::HandleScreenChanged(EPadelScreen NewScreen)
{
	Screen = NewScreen;
	if (!HudWidget)
	{
		return;
	}
	const bool bMenu = Padel::Ui::IsMenuScreen(ToUiScreen(NewScreen));
	const bool bPaused = NewScreen == EPadelScreen::Paused || NewScreen == EPadelScreen::PracticePaused;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->SetPause(bPaused);
	}
	// The score bug stays under the pause scrim, so the paused match still reads (spec 7.5).
	HudWidget->SetVisibility(NewScreen == EPadelScreen::Match || bPaused
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	SetMenuInput(bMenu);
}

void APadelHUD::SetMenuInput(bool bMenu)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !MenuRoot)
	{
		return;
	}
	PC->SetShowMouseCursor(bMenu);
	if (bMenu)
	{
		// The root keeps keyboard focus and routes every key itself (spec 5).
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(MenuRoot->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void APadelHUD::MenuStartMatch()
{
	StartMatch();
}

void APadelHUD::MenuResumeMatch()
{
	ShowScreen(Screen == EPadelScreen::PracticePaused ? EPadelScreen::Practice : EPadelScreen::Match);
}

void APadelHUD::MenuRestartMatch()
{
	StartMatch();
}

void APadelHUD::MenuQuitMatch()
{
	// Spec 7.10: quitting drops the match; no stats screen.
	CloseMatch();
	ShowScreen(EPadelScreen::MainMenu);
}

void APadelHUD::MenuQuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
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

	// A restart closes the old match first, so the owner never holds two.
	CloseMatch();
	MatchEvents.SetGoldenPointRule(Setup.bGoldenPoint);
	ShowScreen(EPadelScreen::Match);
	HudWidget->UpdateScore(LastScore);

	OnMatchRequested.Broadcast(Setup);
	if (MatchEvents.HasLiveMatch())
	{
		return;
	}
	// No match owner attached a simulation: a scripted match on the same events instead.
	UE_LOG(LogPadel, Log, TEXT("No match simulation attached; playing the demo match."));
	Padel::Ui::FDemoMatchOptions Options;
	Options.SetsToWin = Setup.SetsToWin;
	Options.bGoldenPoint = Setup.bGoldenPoint;
	Options.Seed = 1000u + (++MatchCount);
	DemoMatch = MakeUnique<Padel::Ui::FDemoMatch>(Options, *this);
	DemoMatch->Start();
}

void APadelHUD::EndMatch()
{
	CloseMatch();
	ShowScreen(EPadelScreen::Stats);
}

void APadelHUD::CloseMatch()
{
	MatchEndCountdown = -1.0f;
	DemoMatch.Reset();
	OnMatchClosed.Broadcast();
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
	// GDD 3.1: the match owner passes this to FMatchSimulation::ChooseGoldenPointReceiver.
	if (DemoMatch)
	{
		DemoMatch->ChooseGoldenPointReceiver(PlayerId);
	}
	else
	{
		OnGoldenReceiverPicked.Broadcast(PlayerId);
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
	// Spec 7.5: Start pauses only during play, not over the golden point prompt or the
	// end-of-match card. In menus the menu root hears Start itself.
	if (!MenuRoot)
	{
		return;
	}
	const bool bGoldenPrompt = HudWidget && HudWidget->IsGoldenPromptOpen();
	const bool bEndCard = MatchEndCountdown > 0.0f;
	if (MenuRoot->GetFlow().CanOpenPause(bGoldenPrompt, bEndCard))
	{
		ShowScreen(Screen == EPadelScreen::Practice ? EPadelScreen::PracticePaused : EPadelScreen::Paused);
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
