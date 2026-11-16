#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Presentation/Model/PadelNetReaction.h"
#include "CourtSpec.h"
#include "PadelTypes.h"

#include "PadelNetReactionSubsystem.generated.h"

class UMaterialParameterCollection;
class UPadelMatchEvents;

/**
 * Makes the net give way when the ball goes into it or clips the tape (GDD 2 net,
 * GDD 3.4 ball_bounced on "net"). Cosmetic only: the ball's bounce is PadelCore's.
 *
 * Listens to UPadelMatchEvents, runs the engine-free FNetReaction model and writes its
 * parameters into MPC_NetReaction each frame while the net is moving. The net
 * materials (M_PadelNet, M_PadelNetSolid) read that collection and move SM_CourtNet's
 * vertices, so any net placed in any level reacts, with no per-vertex CPU work.
 *
 * Tuning comes from Data/Net.json ("Reaction"). Without MPC_NetReaction (the asset
 * pipeline hasn't been run yet) the model still runs; nothing on screen moves.
 */
UCLASS()
class PADEL_API UPadelNetReactionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Listens to Events (done on Initialize for this world's events). Rebinding moves the listener. */
	void BindTo(UPadelMatchEvents& Events);
	void Unbind();

	/** Steps the model and writes the collection. Tick calls it; tests call it directly. */
	void Advance(double Seconds);

	const Padel::Presentation::FNetReaction& GetModel() const { return Model; }

	/** Reads Data/Net.json ("Reaction"). A missing file or key keeps the defaults. */
	static Padel::Presentation::FNetReactionSettings LoadSettings();

	static const TCHAR* CollectionPath;

private:
	void HandleBallHit(int32 PlayerId, Padel::EShotType ShotType, Padel::ETimingGrade TimingGrade, double SpeedMps,
		const FVector& Spin);
	void HandleBallBounced(Padel::Court::ESurface Surface, Padel::Court::EHalf Half, const FVector& WorldPosition);
	void WriteCollection();

	Padel::Presentation::FNetReaction Model;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> Collection;

	TWeakObjectPtr<UPadelMatchEvents> BoundEvents;
	FDelegateHandle HitHandle;
	FDelegateHandle BouncedHandle;
	// True when the collection holds a moving net, so one last write puts it back at rest.
	bool bCollectionDirty = false;
};
