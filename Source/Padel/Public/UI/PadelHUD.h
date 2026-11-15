#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Gameplay/PadelInputConfig.h"
#include "UI/PadelMatchEventBinder.h"
#include "Templates/UniquePtr.h"
#include "UI/Model/PadelUiDemoMatch.h"
#include "UI/Model/PadelUiEventSink.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "UI/Model/PadelUiMatchStats.h"
#include "UI/Model/PadelUiScore.h"

#include "PadelHUD.generated.h"

class APadelBroadcastCamera;
class UPadelHudWidget;
class UPadelMatchAudio;
class UPadelMenuWidget;
struct FInputActionInstance;

/** Start match was chosen: the match owner builds an FMatchSimulation from Setup and attaches it. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelMatchRequested, const Padel::Ui::FMatchSetup& /*Setup*/);
/** The match is over or was quit: the match owner detaches and drops its simulation. */
DECLARE_MULTICAST_DELEGATE(FOnPadelMatchClosed);
/** GDD 3.1: the human picked the golden point receiver; the owner calls ChooseGoldenPointReceiver. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelGoldenReceiverPicked, int32 /*PlayerId*/);

UENUM()
enum class EPadelScreen : uint8
{
	MainMenu,
	MatchSetup,
	Match,
	Paused,
	Stats,
};

/**
 * The slice's screen flow, Quick Match from the main menu to the stats screen (GDD 11
 * mode 1, GDD 12 item 1), and the one place where match events reach the UI, camera and
 * audio (GDD 3.4).
 *
 * It implements IUiEventSink and hears the real match through UPadelMatchEvents
 * (FPadelMatchEventBinder). The HUD doesn't own the match: on Start match it broadcasts
 * OnMatchRequested, and whoever owns the simulation builds it and attaches it to the
 * subsystem. If nothing is attached by then, a scripted FDemoMatch plays instead, so the
 * flow stays playable until a match owner exists.
 */
UCLASS()
class PADEL_API APadelHUD : public AHUD, public FPadelUiEventSink
{
	GENERATED_BODY()

public:
	APadelHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void ShowScreen(EPadelScreen NewScreen);
	EPadelScreen GetScreen() const { return Screen; }
	const Padel::Ui::FMatchStats& GetStats() const { return Stats; }
	const Padel::Ui::FMatchSetup& GetSetup() const { return Setup; }
	UPadelHudWidget* GetHudWidget() const { return HudWidget; }
	UPadelMenuWidget* GetMenuWidget() const { return MenuWidget; }

	// IUiEventSink: GDD 3.4 events.
	virtual void OnServeStarted(int ServerId) override;
	virtual void OnServeFault(std::string_view Fault) override;
	virtual void OnServeLet() override;
	virtual void OnBallHit(int PlayerId, std::string_view ShotType, std::string_view TimingGrade, double SpeedMps) override;
	virtual void OnBallBounced(std::string_view Surface, int Half, const Padel::Ui::FUiVec3& PositionM) override;
	virtual void OnBallLeftCourt(const Padel::Ui::FUiVec3& PositionM) override;
	virtual void OnPointEnded(int WinningTeam, std::string_view Reason, int LastHitterId) override;
	virtual void OnScoreChanged(const Padel::Ui::FScoreView& Score) override;
	virtual void OnGameWon(int Team) override;
	virtual void OnSetWon(int Team) override;
	virtual void OnMatchWon(int Team) override;
	virtual void OnChangeOfEnds() override;
	virtual void OnGoldenPointStarted(int ReceivingTeam) override;

	/** Partner command pressed (GDD 8): shows the prompt. The partner AI binds the action itself. */
	void ShowPartnerCommand(EPadelInputAction Action);

	FOnPadelMatchRequested OnMatchRequested;
	FOnPadelMatchClosed OnMatchClosed;
	FOnPadelGoldenReceiverPicked OnGoldenReceiverPicked;

	/** True while the scripted stand-in plays because no real match was attached. */
	bool IsPlayingDemo() const { return DemoMatch.IsValid(); }

	/** Seconds the end-of-match card stays up before the stats screen. */
	static constexpr float MatchEndDelaySeconds = 4.0f;

protected:
	/** Optional sounds; a thin Blueprint subclass of APadelHUD may assign them. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Padel|Audio")
	TObjectPtr<UPadelMatchAudio> MatchAudio;

private:
	void StartMatch();
	void EndMatch();
	void CloseMatch();
	void SetMenuInput(bool bMenu);
	void BuildMainMenu();
	void BuildSetupMenu();
	void BuildPauseMenu();
	void BuildStatsScreen();
	void RefreshSetupLabels();
	void HandleMenuOption(int32 Index);
	void HandleMenuCycle(int32 Index, int32 Step);
	void HandleMenuBack();
	void HandleGoldenReceiver(int32 PlayerId);
	void TryBindInput();
	void HandlePausePressed(const FInputActionInstance& Instance);
	void HandleCmdNet(const FInputActionInstance& Instance);
	void HandleCmdBack(const FInputActionInstance& Instance);
	void HandleCmdMiddle(const FInputActionInstance& Instance);
	void HandleCmdYours(const FInputActionInstance& Instance);
	FText ButtonLabel(EPadelInputAction Action) const;
	APadelBroadcastCamera* FindCamera() const;

	UPROPERTY()
	TObjectPtr<UPadelHudWidget> HudWidget;

	UPROPERTY()
	TObjectPtr<UPadelMenuWidget> MenuWidget;

	EPadelScreen Screen = EPadelScreen::MainMenu;
	Padel::Ui::FMatchSetup Setup;
	Padel::Ui::FMatchStats Stats;
	Padel::Ui::FScoreView LastScore;
	TUniquePtr<Padel::Ui::FDemoMatch> DemoMatch;
	FPadelMatchEventBinder MatchEvents;
	uint32 MatchCount = 0;
	float MatchEndCountdown = -1.0f;
	/** Title for the score card, shown when the next score arrives. */
	FText PendingCardTitle;
	bool bInputBound = false;
};
