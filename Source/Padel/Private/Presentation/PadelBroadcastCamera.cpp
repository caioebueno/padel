#include "Presentation/PadelBroadcastCamera.h"

#include "Camera/CameraComponent.h"
#include "Court/PadelCourtUnits.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "Presentation/PadelBall.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace PadelBroadcastCameraDetail
{
	constexpr double DefaultAspect = 16.0 / 9.0;

	void ReadVec3(const FJsonObject& Object, const TCHAR* Field, Padel::Presentation::FVec3& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object.TryGetArrayField(Field, Values) && Values->Num() == 3)
		{
			Out = {(*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber()};
		}
	}
}

APadelBroadcastCamera::APadelBroadcastCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	// Keep the field of view matched to the window while the pause menu is up.
	PrimaryActorTick.bTickEvenWhenPaused = true;
	UCameraComponent* CameraComponent = GetCameraComponent();
	const Padel::Presentation::FCameraSettings Defaults;
	CameraComponent->SetFieldOfView(static_cast<float>(Padel::Presentation::HorizontalFovDeg(
		Defaults, Defaults.VerticalFieldOfViewDeg, PadelBroadcastCameraDetail::DefaultAspect)));
	CameraComponent->SetConstraintAspectRatio(false);
	// Broadcast cameras shoot deep focus and don't smear when they move (camera spec 4).
	CameraComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
	CameraComponent->PostProcessSettings.MotionBlurAmount = 0.0f;
	Tags.Add(FName(TEXT("BroadcastCamera")));
}

void APadelBroadcastCamera::BeginPlay()
{
	Super::BeginPlay();
	Rig = Padel::Presentation::FCameraRig(LoadSettings(), 1);
	ApplyPose(Rig.CurrentPose());
}

void APadelBroadcastCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	// A paused point doesn't track or run the transition shot; the pose is re-applied so
	// a window resize still updates the field of view.
	const bool bPaused = World && World->IsPaused();
	if (!bPaused)
	{
		FeedBall();
	}
	ApplyPose(Rig.Step(bPaused ? 0.0 : DeltaSeconds));
}

void APadelBroadcastCamera::SetHumanEnd(int32 EndSign)
{
	Rig.SetEnd(EndSign);
}

void APadelBroadcastCamera::SetBallPositionMetres(const FVector& PositionM, const FVector& VelocityMps)
{
	Rig.SetBall(PositionM.Y, VelocityMps.Y);
}

void APadelBroadcastCamera::OnServeCalled()
{
	Rig.OnServeCalled();
}

void APadelBroadcastCamera::OnBallStruck()
{
	Rig.OnBallStruck();
}

void APadelBroadcastCamera::OnPointEnded()
{
	Rig.OnPointEnded();
}

void APadelBroadcastCamera::FeedBall()
{
	// GDD 9 tracking follows the ball every frame, not just its bounces.
	if (!Ball.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			TActorIterator<APadelBall> It(World);
			Ball = It ? *It : nullptr;
		}
	}
	if (const APadelBall* Shown = Ball.Get(); Shown && Shown->IsShown())
	{
		Rig.SetBall(Shown->GetPresentedPositionM().Y, Shown->GetPresentedVelocityMps().Y);
	}
}

double APadelBroadcastCamera::GetViewAspect() const
{
	int32 Width = 0;
	int32 Height = 0;
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->GetViewportSize(Width, Height);
		}
	}
	return Width > 0 && Height > 0 ? static_cast<double>(Width) / Height : PadelBroadcastCameraDetail::DefaultAspect;
}

void APadelBroadcastCamera::ApplyPose(const Padel::Presentation::FCameraPose& Pose)
{
	const FVector Location = Padel::MetresToWorld(FVector(Pose.Location.X, Pose.Location.Y, Pose.Location.Z));
	const FVector Target = Padel::MetresToWorld(FVector(Pose.LookAt.X, Pose.LookAt.Y, Pose.LookAt.Z));
	SetActorLocationAndRotation(Location, UKismetMathLibrary::FindLookAtRotation(Location, Target));
	// The engine holds the horizontal field of view; we hold the vertical one, so every
	// screen shape shows the same court top to bottom (camera spec 4).
	GetCameraComponent()->SetFieldOfView(static_cast<float>(
		Padel::Presentation::HorizontalFovDeg(Rig.GetSettings(), Pose.VerticalFovDeg, GetViewAspect())));
}

FTransform APadelBroadcastCamera::GetDefaultTransform()
{
	const Padel::Presentation::FCameraPose Pose = Padel::Presentation::EndPose(Padel::Presentation::FCameraSettings(), 1, 0.0);
	const FVector Location = Padel::MetresToWorld(FVector(Pose.Location.X, Pose.Location.Y, Pose.Location.Z));
	const FVector Target = Padel::MetresToWorld(FVector(Pose.LookAt.X, Pose.LookAt.Y, Pose.LookAt.Z));
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
	Values.TryGetNumberField(TEXT("VerticalFieldOfViewDeg"), Settings.VerticalFieldOfViewDeg);
	Values.TryGetNumberField(TEXT("MinHorizontalFieldOfViewDeg"), Settings.MinHorizontalFieldOfViewDeg);
	Values.TryGetNumberField(TEXT("TrackingDeadzoneM"), Settings.TrackingDeadzoneM);
	Values.TryGetNumberField(TEXT("TrackingStrength"), Settings.TrackingStrength);
	Values.TryGetNumberField(TEXT("MaxTrackingM"), Settings.MaxTrackingM);
	Values.TryGetNumberField(TEXT("TrackingLeadSeconds"), Settings.TrackingLeadSeconds);
	Values.TryGetNumberField(TEXT("TrackingHalfLifeSeconds"), Settings.TrackingHalfLifeSeconds);
	Values.TryGetNumberField(TEXT("MaxTrackingSpeedMps"), Settings.MaxTrackingSpeedMps);
	Values.TryGetNumberField(TEXT("RecentreDelaySeconds"), Settings.RecentreDelaySeconds);
	Values.TryGetNumberField(TEXT("RecentreSeconds"), Settings.RecentreSeconds);
	Values.TryGetNumberField(TEXT("EndCutawaySeconds"), Settings.EndCutawaySeconds);
	if (Values.HasField(TEXT("FieldOfViewDeg")) || Values.HasField(TEXT("TrackingResponse")) || Values.HasField(TEXT("EndTransitionSeconds")))
	{
		UE_LOG(LogPadel, Warning, TEXT("%s: FieldOfViewDeg, TrackingResponse and EndTransitionSeconds are no longer read; see VerticalFieldOfViewDeg, TrackingHalfLifeSeconds and EndCutawaySeconds."), *Path);
	}

	const TSharedPtr<FJsonObject>* Cutaway = nullptr;
	if (Root->TryGetObjectField(TEXT("EndCutaway"), Cutaway))
	{
		const FJsonObject& Shot = **Cutaway;
		PadelBroadcastCameraDetail::ReadVec3(Shot, TEXT("PositionM"), Settings.CutawayLocationM);
		PadelBroadcastCameraDetail::ReadVec3(Shot, TEXT("LookAtM"), Settings.CutawayLookAtM);
		Shot.TryGetNumberField(TEXT("VerticalFieldOfViewDeg"), Settings.CutawayVerticalFieldOfViewDeg);
		Shot.TryGetNumberField(TEXT("PanDeg"), Settings.CutawayPanDeg);
	}
	return Settings;
}
