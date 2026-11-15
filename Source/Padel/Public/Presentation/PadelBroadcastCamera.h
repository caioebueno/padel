#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Presentation/Model/PadelCameraRig.h"
#include "PadelBroadcastCamera.generated.h"

/**
 * Broadcast end camera (GDD 9): behind and above the human team's back glass, looking
 * down the court. It tracks the ball sideways (never more than ±1.5 m, never rotating
 * during a point) and orbits to the other end on a change of ends, so the human's team
 * is always at the bottom of the screen. The maths is Padel::Presentation::FCameraRig
 * (natively tested); the values come from Data/Camera.json.
 */
UCLASS()
class PADEL_API APadelBroadcastCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	APadelBroadcastCamera();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The human team's half: +1 = +X, -1 = -X. A change starts the change-over move. */
	void SetHumanEnd(int32 EndSign);

	/** Latest known ball position, in metres (simulation space). */
	void SetBallPositionMetres(const FVector& PositionM);

	const Padel::Presentation::FCameraRig& GetRig() const { return Rig; }

	/** Default transform for the camera behind Team A's (+X) back glass, in world units. */
	static FTransform GetDefaultTransform();

	/** Reads Data/Camera.json ("Broadcast"). Missing file or keys keep the defaults. */
	static Padel::Presentation::FCameraSettings LoadSettings();

	// GDD 9: 7-9 m high, 6-8 m behind the glass. Metres. Defaults when Data/Camera.json is missing.
	static constexpr double DefaultHeight = 8.0;
	static constexpr double DefaultBehindGlass = 7.0;
	static constexpr double DefaultLookAtX = -2.0;
	static constexpr float DefaultFieldOfView = 50.0f;

private:
	void ApplyPose(const Padel::Presentation::FCameraPose& Pose);

	Padel::Presentation::FCameraRig Rig;
};
