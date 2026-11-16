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
class UStaticMeshComponent;

/**
 * The human's player (GDD 6.3, 7). Enhanced Input fills an FPlayerInputFrame; the
 * movement and swings run in PadelCore's FPlayerShotController, stepped at the fixed
 * simulation rate by whoever owns the ball (APadelPracticeDirector in practice, the
 * match later). This actor only samples input and shows the result.
 *
 * The body is placeholder engine shapes until the character art and animation land;
 * the animation agent reads GetShotController()->GetPhase()/GetCharge() (docs/decisions/0004-shot-system.md).
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

	/** Moves the placeholder body to the simulated position. */
	void SyncFromSimulation();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	bool IsHeld(EPadelInputAction Action) const;

	/** The half the body faces from in ShowController; 0 until it first faces the net. */
	double FacingTeamSign = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Placeholder racket, on the forehand side. */
	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UStaticMeshComponent> Racket;

	UPROPERTY()
	TObjectPtr<UPadelInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UEnhancedInputComponent> EnhancedInput;

	TUniquePtr<Padel::Shots::FPlayerShotController> ShotController;
};
