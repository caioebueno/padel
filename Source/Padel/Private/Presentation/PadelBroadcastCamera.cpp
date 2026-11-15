#include "Presentation/PadelBroadcastCamera.h"

#include "Camera/CameraComponent.h"
#include "Court/PadelCourtUnits.h"
#include "Kismet/KismetMathLibrary.h"

APadelBroadcastCamera::APadelBroadcastCamera()
{
	GetCameraComponent()->SetFieldOfView(DefaultFieldOfView);
	GetCameraComponent()->SetConstraintAspectRatio(false);
	Tags.Add(FName(TEXT("BroadcastCamera")));
}

FTransform APadelBroadcastCamera::GetDefaultTransform()
{
	const FVector Location = Padel::MetresToWorld(FVector(Padel::Court::HalfLength + DefaultBehindGlass, 0.0, DefaultHeight));
	const FVector Target = Padel::MetresToWorld(FVector(DefaultLookAtX, 0.0, 0.0));
	return FTransform(UKismetMathLibrary::FindLookAtRotation(Location, Target), Location);
}
