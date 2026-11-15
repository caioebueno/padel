#include "Presentation/PadelBroadcastCamera.h"

#include "Camera/CameraComponent.h"
#include "Court/PadelCourtUnits.h"
#include "Dom/JsonObject.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

APadelBroadcastCamera::APadelBroadcastCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	// Keep tracking and the change-over move running while the pause menu is up.
	PrimaryActorTick.bTickEvenWhenPaused = true;
	GetCameraComponent()->SetFieldOfView(DefaultFieldOfView);
	GetCameraComponent()->SetConstraintAspectRatio(false);
	Tags.Add(FName(TEXT("BroadcastCamera")));
}

void APadelBroadcastCamera::BeginPlay()
{
	Super::BeginPlay();
	const Padel::Presentation::FCameraSettings Settings = LoadSettings();
	Rig = Padel::Presentation::FCameraRig(Settings, 1);
	GetCameraComponent()->SetFieldOfView(static_cast<float>(Settings.FieldOfViewDeg));
	ApplyPose(Rig.CurrentPose());
}

void APadelBroadcastCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyPose(Rig.Step(DeltaSeconds));
}

void APadelBroadcastCamera::SetHumanEnd(int32 EndSign)
{
	Rig.SetEnd(EndSign);
}

void APadelBroadcastCamera::SetBallPositionMetres(const FVector& PositionM)
{
	Rig.SetBallY(PositionM.Y);
}

void APadelBroadcastCamera::ApplyPose(const Padel::Presentation::FCameraPose& Pose)
{
	const FVector Location = Padel::MetresToWorld(FVector(Pose.Location.X, Pose.Location.Y, Pose.Location.Z));
	const FVector Target = Padel::MetresToWorld(FVector(Pose.LookAt.X, Pose.LookAt.Y, Pose.LookAt.Z));
	SetActorLocationAndRotation(Location, UKismetMathLibrary::FindLookAtRotation(Location, Target));
}

FTransform APadelBroadcastCamera::GetDefaultTransform()
{
	const FVector Location = Padel::MetresToWorld(FVector(Padel::Court::HalfLength + DefaultBehindGlass, 0.0, DefaultHeight));
	const FVector Target = Padel::MetresToWorld(FVector(DefaultLookAtX, 0.0, 0.0));
	return FTransform(UKismetMathLibrary::FindLookAtRotation(Location, Target), Location);
}

Padel::Presentation::FCameraSettings APadelBroadcastCamera::LoadSettings()
{
	Padel::Presentation::FCameraSettings Settings;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("Camera.json"));
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogPadel, Warning, TEXT("No %s: using the default broadcast camera."), *Path);
		return Settings;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	const TSharedPtr<FJsonObject>* Broadcast = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->TryGetObjectField(TEXT("Broadcast"), Broadcast))
	{
		UE_LOG(LogPadel, Warning, TEXT("%s has no \"Broadcast\" object: using the default broadcast camera."), *Path);
		return Settings;
	}
	const FJsonObject& Values = **Broadcast;
	Values.TryGetNumberField(TEXT("HeightM"), Settings.HeightM);
	Values.TryGetNumberField(TEXT("BehindGlassM"), Settings.BehindGlassM);
	Values.TryGetNumberField(TEXT("LookAtPastNetM"), Settings.LookAtPastNetM);
	Values.TryGetNumberField(TEXT("FieldOfViewDeg"), Settings.FieldOfViewDeg);
	Values.TryGetNumberField(TEXT("TrackingStrength"), Settings.TrackingStrength);
	Values.TryGetNumberField(TEXT("MaxTrackingM"), Settings.MaxTrackingM);
	Values.TryGetNumberField(TEXT("TrackingResponse"), Settings.TrackingResponse);
	Values.TryGetNumberField(TEXT("EndTransitionSeconds"), Settings.EndTransitionSeconds);
	return Settings;
}
