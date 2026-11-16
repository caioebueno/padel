#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Shots/PlayerInputFrame.h"
#include "Gameplay/PadelInputConfig.h"
#include "Shots/PlayerShotController.h"
#include "PadelPlayerPawn.generated.h"

class UCapsuleComponent;
class UEnhancedInputComponent;
class UPadelInputConfig;
class UPadelPlayerAnimComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * The human's player (GDD 6.3, 7). Enhanced Input fills an FPlayerInputFrame; the
 * movement and swings run in PadelCore's FPlayerShotController, stepped at the fixed
 * simulation rate by whoever owns the ball (APadelPracticeDirector in practice, the
 * match later). This actor only samples input and shows the result.
 *
 * The body is placeholder engine shapes until the character art and animation land.
 * UPadelPlayerAnimComponent (docs/decisions/0005-player-animation.md) swaps in the
 * skeletal body once it and ABP_PadelPlayer are imported, and swings the placeholder
 * racket in time with the ball until then.
 */
UCLASS()
class PADEL_API APadelPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	APadelPlayerPawn();

	/**
	 * In a match the shot controllers belong to the match runner (FAiMatchRunner), so the
	 * pawn only shows one: its position, facing the net from the controller's half.
	 */
	void ShowController(const Padel::Shots::FPlayerShotController& Controller, const Padel::Shots::FPlayerRatings& Ratings);

	/** Creates the shot controller. The pointers must outlive the pawn. */
	void InitShots(int32 PlayerId, double TeamSign, const Padel::Shots::FPlayerRatings* Ratings,
		const Padel::Shots::FShotLibrary* Library, const Padel::Shots::FMovementTuning* Movement,
		const Padel::Court::FVec3& StartMetres, uint64 Seed);

	/** This tick's input, in the team-relative frame the controller expects. */
	Padel::Shots::FPlayerInputFrame SampleInput() const;

	Padel::Shots::FPlayerShotController* GetShotController() const { return ShotController.Get(); }

	/** Moves the body to the simulated position and presents the latest animation frame. */
	void SyncFromSimulation();

	/** Steps the animation for the pawn's own controller (practice); once per fixed step, right after its Tick. */
	void StepAnimation(const Padel::Shots::FSwingEvents& Events, double Dt, bool bRallyLive);

	/**
	 * For pawns whose controller lives elsewhere (the match: FAiMatchRunner owns all four).
	 * InitAnimation once per pawn; then each fixed step, right after the controller's Tick,
	 * StepAnimation with that controller and its events. Don't cache the controller: the
	 * runner rebuilds them at a change of ends.
	 */
	void InitAnimation(const Padel::Shots::FShotLibrary* Library, int32 TeamIndex);
	void StepAnimation(const Padel::Shots::FPlayerShotController& Controller, const Padel::Shots::FSwingEvents& Events, double Dt, bool bRallyLive);

	/** Applies the latest animation frame; SyncFromSimulation does this for the pawn's own controller. */
	void PresentAnimation();

	UPadelPlayerAnimComponent* GetAnimation() const { return Animation; }

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	bool IsHeld(EPadelInputAction Action) const;

	/** The half the body faces from in ShowController; 0 until it first faces the net. */
	double FacingTeamSign = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Skeletal body, hidden until its mesh and AnimBP are imported. */
	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<USkeletalMeshComponent> Character;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UPadelPlayerAnimComponent> Animation;

	/** Placeholder racket, on the forehand side. */
	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UStaticMeshComponent> Racket;

	UPROPERTY()
	TObjectPtr<UPadelInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UEnhancedInputComponent> EnhancedInput;

	TUniquePtr<Padel::Shots::FPlayerShotController> ShotController;
};
