#pragma once

#include "CoreMinimal.h"
#include "Data/PadelShotData.h"
#include "GameFramework/Actor.h"
#include "Shots/BallMachine.h"
#include "Shots/PlayerShotController.h"
#include "Shots/PracticeBall.h"
#include "PadelPracticeDirector.generated.h"

class APadelPlayerPawn;
class UStaticMeshComponent;

/** What the debug overlay shows (GDD 7: the target only in the debug overlay). */
struct FPadelShotDebugInfo
{
	bool bHasShot = false;
	Padel::Shots::FShotIntent LastShot;
	Padel::Shots::ESwingOutcome LastOutcome = Padel::Shots::ESwingOutcome::None;
	FString Drill;
	bool bMachinePaused = false;
	int32 Hits = 0;
	int32 Perfects = 0;
	int32 SplitSteps = 0;
};

/**
 * Ball machine practice (GDD 11, brought forward so every slice shot can be played on
 * purpose, GDD 12 item 3). Owns the stand-in ball, the machine and the human's pawn,
 * and steps them together at the fixed simulation rate, in a set order, so a session
 * is deterministic for a given seed and input.
 *
 * Practice controls, on the partner-command buttons since practice has no partner:
 * D-pad left/right (3/4) change the drill, D-pad up (1) pauses the machine, D-pad
 * down (2) feeds a ball now. In the "serve" drill, A/J tosses and A/J again serves.
 */
UCLASS()
class PADEL_API APadelPracticeDirector : public AActor
{
	GENERATED_BODY()

public:
	APadelPracticeDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	const FPadelShotDebugInfo& GetDebugInfo() const { return DebugInfo; }
	APadelPlayerPawn* GetPlayerPawn() const { return PlayerPawn; }
	const Padel::Shots::FPracticeBall* GetBall() const { return Ball.Get(); }

	/** Row of Data/Players.json the human plays as. */
	UPROPERTY(EditAnywhere, Category = "Practice")
	FName PlayerRow = FName(TEXT("practice_default"));

	/** Seed for the machine and the shot scatter: same seed, same session. */
	UPROPERTY(EditAnywhere, Category = "Practice")
	int64 Seed = 1;

private:
	void StepSimulation(double Dt);
	void SpawnAndPossessPlayer();
	void BindPracticeInput();
	void DrawDebug() const;

	void PreviousDrill();
	void NextDrill();
	void TogglePause();
	void FeedNow();

	UPROPERTY(VisibleAnywhere, Category = "Practice")
	TObjectPtr<UStaticMeshComponent> BallMesh;

	UPROPERTY()
	TObjectPtr<APadelPlayerPawn> PlayerPawn;

	FPadelShotData Data;
	Padel::Shots::FPlayerRatings Ratings;
	TUniquePtr<Padel::Shots::FPracticeBall> Ball;
	TUniquePtr<Padel::Shots::FBallMachine> Machine;
	double Accumulator = 0.0;
	bool bReady = false;
	FPadelShotDebugInfo DebugInfo;
};
