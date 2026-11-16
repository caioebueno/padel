#include "Presentation/PadelBall.h"

#include "BallState.h"
#include "Components/StaticMeshComponent.h"
#include "Court/PadelCourtUnits.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/PadelShotUnits.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"

namespace PadelBallDetail
{
	const TCHAR* const BallPath = TEXT("/Game/Padel/Equipment/SM_Ball.SM_Ball");
	const TCHAR* const TrailPath = TEXT("/Game/Padel/Equipment/SM_BallTrail.SM_BallTrail");
	const TCHAR* const FallbackPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	// The engine sphere is 100 cm across.
	constexpr double FallbackDiameterCm = 100.0;

	UStaticMesh* LoadQuietly(const FSoftObjectPath& Path)
	{
		return Path.IsNull() ? nullptr
			: LoadObject<UStaticMesh>(nullptr, *Path.ToString(), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FQuat ToQuat(const Padel::Presentation::FBallQuat& Q)
	{
		// Same axes and the same rotation sense as the simulation (PadelCourtUnits.h).
		return FQuat(Q.X, Q.Y, Q.Z, Q.W);
	}
}

APadelBall::APadelBall()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	BallMesh->SetupAttachment(RootComponent);
	BallMesh->SetMobility(EComponentMobility::Movable);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetCastShadow(true);
	BallMesh->SetGenerateOverlapEvents(false);
	BallMesh->bUseAsOccluder = false;

	// A sibling of the ball, not a child: it must not turn with the spin.
	TrailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trail"));
	TrailMesh->SetupAttachment(RootComponent);
	TrailMesh->SetMobility(EComponentMobility::Movable);
	TrailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrailMesh->SetCastShadow(false);
	TrailMesh->SetGenerateOverlapEvents(false);
	TrailMesh->bUseAsOccluder = false;
	TrailMesh->SetVisibility(false);

	BallAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(PadelBallDetail::BallPath));
	TrailAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(PadelBallDetail::TrailPath));
}

void APadelBall::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Visual = Padel::Presentation::FBallVisual(LoadSettings());
	LoadMeshes();
	Hide();
}

void APadelBall::LoadMeshes()
{
	using namespace PadelBallDetail;
	if (UStaticMesh* Ball = LoadQuietly(BallAsset.ToSoftObjectPath()))
	{
		// Built to size by the pipeline (65 mm, GDD 2).
		BallMesh->SetStaticMesh(Ball);
		BallMesh->SetRelativeScale3D(FVector::OneVector);
		bImportedBall = true;
	}
	else if (UStaticMesh* Sphere = LoadQuietly(FSoftObjectPath(FallbackPath)))
	{
		BallMesh->SetStaticMesh(Sphere);
		const double DiameterCm = Padel::MetresToWorld(FVector(Padel::BallRadius * 2.0)).X;
		BallMesh->SetRelativeScale3D(FVector(DiameterCm / FallbackDiameterCm));
		bImportedBall = false;
		static bool bLogged = false;
		if (!bLogged)
		{
			bLogged = true;
			UE_LOG(LogPadel, Warning, TEXT("%s isn't imported: showing the engine sphere. Run Tools/AssetPipeline/pipeline.py all."),
				*BallAsset.ToSoftObjectPath().ToString());
		}
	}

	UStaticMesh* Trail = LoadQuietly(TrailAsset.ToSoftObjectPath());
	TrailMesh->SetStaticMesh(Trail);
	bHasTrail = Trail != nullptr;
}

Padel::Presentation::FBallVisualSettings APadelBall::LoadSettings()
{
	Padel::Presentation::FBallVisualSettings Settings;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("Ball.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogPadel, Warning, TEXT("No %s: using the default ball visuals."), *Path);
		return Settings;
	}
	std::string Error;
	if (!Padel::Presentation::ParseBallVisualSettings(TCHAR_TO_UTF8(*Text), Settings, Error))
	{
		UE_LOG(LogPadel, Warning, TEXT("%s: %s. Using the default ball visuals."), *Path, UTF8_TO_TCHAR(Error.c_str()));
	}
	return Settings;
}

void APadelBall::Present(const Padel::FVec3& PositionM, const Padel::FVec3& VelocityMps, const Padel::FVec3& SpinRadPerS, bool bVisible)
{
	if (!bVisible)
	{
		Hide();
		return;
	}
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (!bShown)
	{
		Visual.Reset(PositionM);
		LastPresentTime = Now;
		bShown = true;
		BallMesh->SetVisibility(true);
	}
	const double Dt = FMath::Max(0.0, Now - LastPresentTime);
	LastPresentTime = Now;
	Visual.Update(PositionM, VelocityMps, SpinRadPerS, Dt);

	SetActorLocation(Padel::ToWorld(PositionM));
	BallMesh->SetRelativeRotation(PadelBallDetail::ToQuat(Visual.GetOrientation()));
	ApplyTrail();
}

void APadelBall::ApplyTrail()
{
	const double Opacity = Visual.GetTrailOpacity();
	const double LengthM = Visual.GetTrailLength();
	if (!bHasTrail || Opacity <= 0.001 || LengthM < 0.01)
	{
		TrailMesh->SetVisibility(false);
		return;
	}
	// SM_BallTrail is 1 m long from its origin back along -X: aim +X from the tail to the
	// ball and stretch X to the trail's length.
	const Padel::FVec3 Back = Visual.GetTrailTail() - Visual.GetPosition();
	const FVector Forward = -FVector(Back.X, Back.Y, Back.Z).GetSafeNormal();
	TrailMesh->SetWorldRotation(Forward.Rotation());
	TrailMesh->SetRelativeScale3D(FVector(LengthM, 1.0, 1.0));
	TrailMesh->SetCustomPrimitiveDataFloat(0, static_cast<float>(Opacity));
	TrailMesh->SetVisibility(true);
}

void APadelBall::Hide()
{
	bShown = false;
	BallMesh->SetVisibility(false);
	TrailMesh->SetVisibility(false);
}
