#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/Model/PlayerAnimModel.h"
#include "Animation/Model/RacketGrip.h"
// Complete type: the generated vtable-helper constructor destroys the TUniquePtr below.
#include "Animation/PadelAnimData.h"
#include "Animation/PadelPlayerPoseNode.h"
#include "PadelPlayerAnimComponent.generated.h"

class UAnimSequenceBase;
class USceneComponent;
class USkeletalMeshComponent;

/**
 * Animation for one player (docs/decisions/0005-player-animation.md). Owns the
 * engine-free FPlayerAnimModel, steps it on the simulation's fixed tick right after
 * the shot controller, and presents the latest frame each rendered frame:
 *
 * - With a skeletal body and ABP_PadelPlayer loaded, it pushes the frame into
 *   UPadelPlayerAnimInstance and the racket rides the hand bone.
 * - Until those assets exist, it moves the placeholder racket along the procedural
 *   swing arc, which reaches the ball on the contact tick.
 * - When the player also has a MetaHuman in Data/PlayerBodies.json and it is on disk,
 *   the placeholder skeleton keeps running ABP_PadelPlayer hidden and the MetaHuman is
 *   shown in its place, retargeted from it every frame (docs/decisions/0006-metahuman-players.md).
 *
 * The racket is SM_Racket from Data/Racket.json once the asset pipeline has imported it
 * (docs/decisions/0008-racket.md). On a skeletal body it sits in the palm, placed from
 * the hand's joints in the reference pose (Animation/Model/RacketGrip.h).
 */
UCLASS(ClassGroup = (Padel), meta = (BlueprintSpawnableComponent))
class PADEL_API UPadelPlayerAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPadelPlayerAnimComponent();
	virtual ~UPadelPlayerAnimComponent() override;

	/**
	 * Loads the animation data and the body assets. Shots must outlive the component.
	 * PlayerKey is the player's row in Players.json; it picks a MetaHuman from PlayerBodies.json.
	 */
	void Init(const Padel::Shots::FShotLibrary* Shots, int32 TeamIndex, FName PlayerKey, USkeletalMeshComponent* InBody, USceneComponent* InRacket);

	/**
	 * One fixed simulation step, after FPlayerShotController::Tick returned Events. Context is
	 * what the owner knows about the point: rally live, the glass an AI player reads (N1c),
	 * the serve role and the ball (P1 stances and head look).
	 */
	void Step(const Padel::Shots::FPlayerShotController& Controller, const Padel::Shots::FSwingEvents& Events, double Dt,
		const Padel::Animation::FPlayerAnimContext& Context);

	void NotifyPointEnded(bool bWon, uint32 PointIndex);

	/** Applies the latest frame to the body or the placeholder racket. */
	void Present();

	/** True once a skeletal body and the AnimBP are in use. */
	bool HasSkeletalBody() const { return bSkeletalBody; }

	/** The placeholder skeleton: shown, or hidden and driving a MetaHuman. */
	USkeletalMeshComponent* GetDriverBody() const { return Body; }

	/** Racket in the left hand (set from the player's Handedness on the first step). */
	bool IsLeftHanded() const { return bLeftHanded; }

	/** The MetaHuman shown for this player, or null when the placeholder (or the shapes) are shown. */
	AActor* GetMetaHuman() const { return MetaHuman; }

	const Padel::Animation::FPlayerAnimFrame* GetFrame() const;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	UAnimSequenceBase* Resolve(const std::string& AssetPath);

	/** The C++ pose's clips from Animation.json (null where not imported). */
	FPadelLocomotionSet LoadLocomotionSet() const;

	/** Spawns PlayerKey's MetaHuman over the hidden placeholder skeleton; false (and logged) when it can't. */
	bool ShowMetaHuman(FName PlayerKey);
	void RemoveMetaHuman();

	/** Puts SM_Racket on the racket component when it's imported; keeps the placeholder otherwise. */
	void LoadRacket();

	/** Attaches the racket to To's racket bone, in the palm (RacketGrip.h). */
	void AttachRacket(USkeletalMeshComponent* To);

	/** Moves the racket, its grip and the contact warp to the left (or back to the right) arm. */
	void SetLeftHanded(bool bLeft);

	/** A right-side bone name ("hand_r") on the racket side: "hand_l" for a left-hander. */
	FName OnRacketSide(FName RightSideBone) const;

	bool bLeftHanded = false;

	/** The body's relative yaw square to the net, read on the first Present (N1 facing turns from it). */
	double BodyBaseYaw = 0.0;
	bool bBodyBaseYawSet = false;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<USceneComponent> Racket;

	/** The assembled MetaHuman actor, attached to Body; it belongs to this component. */
	UPROPERTY()
	TObjectPtr<AActor> MetaHuman;

	/** Asset path -> sequence, or null when it isn't imported yet (looked up once). */
	UPROPERTY()
	TMap<FString, TObjectPtr<UAnimSequenceBase>> Sequences;

	TUniquePtr<FPadelAnimData> Data;
	TUniquePtr<Padel::Animation::FPlayerAnimModel> Model;
	bool bSkeletalBody = false;
	Padel::Animation::FRacketSettings RacketSettings;
	bool bImportedRacket = false;
	bool bStepped = false;
};
