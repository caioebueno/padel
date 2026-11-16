#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/UniquePtr.h"

#include "AI/AiMatchRunner.h"
#include "AI/AiTuning.h"
#include "BallPhysicsParams.h"
#include "Data/PadelShotData.h"
#include "Gameplay/PadelInputConfig.h"
#include "MatchSimulation.h"
#include "UI/Model/PadelUiMatchSetup.h"

#include <array>
#include <vector>

#include "PadelMatchDirector.generated.h"

class APadelHUD;
class APadelPlayerPawn;
class UStaticMeshComponent;

/**
 * Owns the real match (GDD 11 mode 1, Quick Match vs AI): the FMatchSimulation, the
 * FAiMatchRunner that plays the AI players and the human through the shot system, and
 * the four pawns that show them.
 *
 * The HUD's Start match asks for a match (APadelHUD::OnMatchRequested). The director
 * builds it from the setup, attaches UPadelMatchEvents so the UI, audio and camera hear
 * it, and then steps the runner at the fixed 120 Hz with the human's sampled input.
 * Frame time only decides how many fixed ticks run (CLAUDE.md), so a match plays the
 * same for the same seed and input frames.
 *
 * APadelGameMode spawns one. The HUD falls back to its demo match only when this
 * director can't start a match (for example, a data file failed to load).
 */
UCLASS()
class PADEL_API APadelMatchDirector : public AActor
{
	GENERATED_BODY()

public:
	APadelMatchDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Builds and starts a match from the setup screen's choices. A HumanPlayerId of -1
	 * plays four AI players (tests, attract mode). Returns false, and starts nothing, when
	 * the data didn't load.
	 */
	bool StartMatch(const Padel::Ui::FMatchSetup& Setup);

	/** Detaches the events and drops the match and its pawns. Safe with no match. */
	void CloseMatch();

	/** Runs fixed ticks now, for tests and replays. The human's input is their pawn's. */
	void StepTicks(int32 Count);

	bool IsMatchRunning() const { return Match.IsValid(); }
	const Padel::FMatchSimulation* GetMatch() const { return Match.Get(); }
	const Padel::AI::FAiMatchRunner* GetRunner() const { return Runner.Get(); }
	APadelPlayerPawn* GetPawn(int32 PlayerId) const;

	/** Rows of Data/Players.json, by player id (0, 1 Team A; 2, 3 Team B; GDD 6.4). */
	UPROPERTY(EditAnywhere, Category = "Match")
	TArray<FName> PlayerRows;

	/** First match's seed; each new match in the session adds one. */
	UPROPERTY(EditAnywhere, Category = "Match")
	int64 Seed = 1;

private:
	bool LoadData();
	void BindHud();
	void BindPartnerCommands();
	void SpawnPawns();
	void DestroyPawns();
	void StepOnce();
	void SyncView();

	void HandleMatchRequested(const Padel::Ui::FMatchSetup& Setup);
	void HandleMatchClosed();
	void HandleGoldenReceiverPicked(int32 PlayerId);
	void GiveCommand(EPadelInputAction Action);
	void CmdNet() { GiveCommand(EPadelInputAction::CmdNet); }
	void CmdBack() { GiveCommand(EPadelInputAction::CmdBack); }
	void CmdMiddle() { GiveCommand(EPadelInputAction::CmdMiddle); }
	void CmdYours() { GiveCommand(EPadelInputAction::CmdYours); }

	UPROPERTY(VisibleAnywhere, Category = "Match")
	TObjectPtr<UStaticMeshComponent> BallMesh;

	/** By player id; null slots when no match is running. */
	UPROPERTY()
	TArray<TObjectPtr<APadelPlayerPawn>> Pawns;

	UPROPERTY()
	TObjectPtr<APadelHUD> BoundHud;

	// Loaded once from Data/ (CLAUDE.md: the text files are the source of truth).
	bool bDataLoaded = false;
	bool bDataOk = false;
	FPadelShotData ShotData;
	Padel::FBallPhysicsParams Physics;
	Padel::AI::FAiTactics Tactics;
	std::vector<Padel::AI::FAiDifficulty> Difficulties;
	std::array<Padel::Shots::FPlayerRatings, Padel::NumPlayers> Players;

	// The runner listens to the match, so it is created after it and destroyed first.
	TUniquePtr<Padel::FMatchSimulation> Match;
	TUniquePtr<Padel::AI::FAiMatchRunner> Runner;
	int32 HumanPlayerId = -1;
	int32 MatchesStarted = 0;
	double Accumulator = 0.0;
	bool bPartnerCommandsBound = false;
};
