#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/Model/PlayerAnimModel.h"
#include "PadelPlayerAnimComponent.generated.h"

class UAnimSequenceBase;
class USceneComponent;
class USkeletalMeshComponent;
struct FPadelAnimData;

/**
 * Animation for one player (docs/decisions/0005-player-animation.md). Owns the
 * engine-free FPlayerAnimModel, steps it on the simulation's fixed tick right after
 * the shot controller, and presents the latest frame each rendered frame:
 *
 * - With a skeletal body and ABP_PadelPlayer loaded, it pushes the frame into
 *   UPadelPlayerAnimInstance and the racket rides the hand bone.
 * - Until those assets exist, it moves the placeholder racket along the procedural
 *   swing arc, which reaches the ball on the contact tick.
 */
UCLASS(ClassGroup = (Padel), meta = (BlueprintSpawnableComponent))
class PADEL_API UPadelPlayerAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPadelPlayerAnimComponent();
	virtual ~UPadelPlayerAnimComponent() override;

	/** Loads the animation data and the body assets. Shots must outlive the component. */
	void Init(const Padel::Shots::FShotLibrary* Shots, int32 TeamIndex, USkeletalMeshComponent* InBody, USceneComponent* InRacket);

	/** One fixed simulation step, after FPlayerShotController::Tick returned Events. */
	void Step(const Padel::Shots::FPlayerShotController& Controller, const Padel::Shots::FSwingEvents& Events, double Dt, bool bRallyLive);

	void NotifyPointEnded(bool bWon, uint32 PointIndex);

	/** Applies the latest frame to the body or the placeholder racket. */
	void Present();

	/** True once a skeletal body and the AnimBP are in use. */
	bool HasSkeletalBody() const { return bSkeletalBody; }

	const Padel::Animation::FPlayerAnimFrame* GetFrame() const;

private:
	UAnimSequenceBase* Resolve(const std::string& AssetPath);

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<USceneComponent> Racket;

	/** Asset path -> sequence, or null when it isn't imported yet (looked up once). */
	UPROPERTY()
	TMap<FString, TObjectPtr<UAnimSequenceBase>> Sequences;

	TUniquePtr<FPadelAnimData> Data;
	TUniquePtr<Padel::Animation::FPlayerAnimModel> Model;
	bool bSkeletalBody = false;
	bool bStepped = false;
};
