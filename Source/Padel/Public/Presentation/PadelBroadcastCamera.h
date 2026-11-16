#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Presentation/Model/PadelCameraRig.h"
#include "PadelBroadcastCamera.generated.h"

class APadelBall;

/**
 * Broadcast end camera (GDD 9): a long lens far behind and above the human team's back
 * glass, aimed at the foot of the net, so the whole court and both pairs are always in
 * shot. It drifts gently sideways with the ball (never turning during a point), starts
 * each serve centred, and cuts through a net-side transition shot on a change of ends, so
 * the human's team is always at the bottom of the screen. The maths is
 * Padel::Presentation::FCameraRig (natively tested); the values come from Data/Camera.json.
 */
UCLASS()
class PADEL_API APadelBroadcastCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	APadelBroadcastCamera();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The human team's half: +1 = +X, -1 = -X. A change cuts to the other end via the transition shot. */
	void SetHumanEnd(int32 EndSign);

	/** Ball position (and velocity, for the lead), in metres (simulation space). Tick reads the ball actor itself; this is for tests and tools. */
	void SetBallPositionMetres(const FVector& PositionM, const FVector& VelocityMps = FVector::ZeroVector);

	/** Match moments from UPadelMatchEvents (via the HUD): a serve is called (or re-called after a fault or let), a ball is struck, a point ends. */
	void OnServeCalled();
	void OnBallStruck();
	void OnPointEnded();

	const Padel::Presentation::FCameraRig& GetRig() const { return Rig; }

	/** Default transform for the camera behind Team A's (+X) back glass, in world units. */
	static FTransform GetDefaultTransform();

	/** Reads Data/Camera.json ("Broadcast", "EndCutaway"). Missing file or keys keep the defaults. */
	static Padel::Presentation::FCameraSettings LoadSettings();

private:
	void FeedBall();
	void ApplyPose(const Padel::Presentation::FCameraPose& Pose);
	double GetViewAspect() const;

	Padel::Presentation::FCameraRig Rig;

	UPROPERTY(Transient)
	TWeakObjectPtr<APadelBall> Ball;
};
