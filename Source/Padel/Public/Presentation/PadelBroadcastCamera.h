#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "PadelBroadcastCamera.generated.h"

/**
 * Placeholder broadcast end camera (GDD 9): behind and above the human team's back
 * glass, looking down the court. Fixed for now; ball tracking, the change-of-ends
 * move and loading these values from Data/Camera.json belong to the presentation task.
 */
UCLASS()
class PADEL_API APadelBroadcastCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	APadelBroadcastCamera();

	/** Default transform for the camera behind Team A's (+X) back glass, in world units. */
	static FTransform GetDefaultTransform();

	// GDD 9: 7-9 m high, 6-8 m behind the glass. Metres.
	static constexpr double DefaultHeight = 8.0;
	static constexpr double DefaultBehindGlass = 7.0;
	static constexpr double DefaultLookAtX = -2.0;
	static constexpr float DefaultFieldOfView = 50.0f;
};
