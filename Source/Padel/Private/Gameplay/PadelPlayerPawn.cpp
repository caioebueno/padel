#include "Gameplay/PadelPlayerPawn.h"

#include "Animation/PadelPlayerAnimComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Gameplay/PadelInputConfig.h"
#include "Gameplay/PadelPlayerController.h"
#include "Gameplay/PadelShotUnits.h"
#include "PadelLog.h"
#include "UObject/ConstructorHelpers.h"

namespace PadelPlayerPawnDetail
{
	// Placeholder body sizes (cm), not tuning: they only draw the player.
	constexpr float CapsuleRadius = 30.0f;
	constexpr float CapsuleHalfHeight = 90.0f;
}

APadelPlayerPawn::APadelPlayerPawn()
{
	using namespace PadelPlayerPawnDetail;
	PrimaryActorTick.bCanEverTick = false;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	// The simulation moves the player; collision is not used for it.
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Capsule;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
		// Engine cylinder is 100 cm tall and 100 cm wide.
		Body->SetRelativeScale3D(FVector(CapsuleRadius / 50.0f, CapsuleRadius / 50.0f, CapsuleHalfHeight / 50.0f));
	}

	// Imported characters stand on their origin and face +Y (Tools/AssetPipeline/README.md).
	Character = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Character"));
	Character->SetupAttachment(Capsule);
	Character->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -CapsuleHalfHeight), FRotator(0.0, -90.0, 0.0));
	Character->SetVisibility(false);

	Animation = CreateDefaultSubobject<UPadelPlayerAnimComponent>(TEXT("Animation"));

	Racket = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Racket"));
	Racket->SetupAttachment(Capsule);
	Racket->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cube.Succeeded())
	{
		Racket->SetStaticMesh(Cube.Object);
		Racket->SetRelativeScale3D(FVector(0.02f, 0.26f, 0.45f));
	}
}

void APadelPlayerPawn::InitShots(int32 PlayerId, double TeamSign, const Padel::Shots::FPlayerRatings* Ratings,
	const Padel::Shots::FShotLibrary* Library, const Padel::Shots::FMovementTuning* Movement,
	const Padel::Court::FVec3& StartMetres, uint64 Seed)
{
	ShotController = MakeUnique<Padel::Shots::FPlayerShotController>(PlayerId, TeamSign, Ratings, Library, Movement, StartMetres, Seed);

	// Face the net, racket on the forehand side.
	SetActorRotation(FRotator(0.0, TeamSign > 0.0 ? 180.0 : 0.0, 0.0));
	const Padel::Court::FVec3 Right = Padel::Shots::RightAxis(TeamSign);
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Padel::ToWorld(Right)) * Ratings->ForehandSign();
	Racket->SetRelativeLocation(Local * 55.0 + FVector(0.0, 0.0, 10.0));

	InitAnimation(Library, TeamSign > 0.0 ? 0 : 1);
	SyncFromSimulation();
}

void APadelPlayerPawn::InitAnimation(const Padel::Shots::FShotLibrary* Library, int32 TeamIndex)
{
	Animation->Init(Library, TeamIndex, Character, Racket);
	Body->SetVisibility(!Animation->HasSkeletalBody());
}

void APadelPlayerPawn::StepAnimation(const Padel::Shots::FSwingEvents& Events, double Dt, bool bRallyLive)
{
	if (ShotController)
	{
		Animation->Step(*ShotController, Events, Dt, bRallyLive);
	}
}

void APadelPlayerPawn::StepAnimation(const Padel::Shots::FPlayerShotController& Controller, const Padel::Shots::FSwingEvents& Events, double Dt, bool bRallyLive)
{
	Animation->Step(Controller, Events, Dt, bRallyLive);
}

void APadelPlayerPawn::PresentAnimation()
{
	Animation->Present();
}

Padel::Shots::FPlayerInputFrame APadelPlayerPawn::SampleInput() const
{
	Padel::Shots::FPlayerInputFrame Frame;
	if (!EnhancedInput || !InputConfig)
	{
		return Frame;
	}
	// The broadcast camera is always behind the human's team (GDD 9), so the stick is
	// already team-relative: X right, Y up = toward the net.
	const FVector2D Move = EnhancedInput->GetBoundActionValue(InputConfig->GetAction(EPadelInputAction::Move)).Get<FVector2D>();
	Frame.Move = Padel::Shots::FVec2{ Move.X, Move.Y };
	Frame.bSprint = IsHeld(EPadelInputAction::Sprint);
	Frame.bDrive = IsHeld(EPadelInputAction::ShotDrive);
	Frame.bLob = IsHeld(EPadelInputAction::ShotLob);
	Frame.bOverhead = IsHeld(EPadelInputAction::ShotOverhead);
	Frame.bModAttack = IsHeld(EPadelInputAction::ModAttack);
	Frame.bModTouch = IsHeld(EPadelInputAction::ModTouch);
	return Frame;
}

bool APadelPlayerPawn::IsHeld(EPadelInputAction Action) const
{
	return EnhancedInput->GetBoundActionValue(InputConfig->GetAction(Action)).Get<bool>();
}

void APadelPlayerPawn::SyncFromSimulation()
{
	if (!ShotController)
	{
		return;
	}
	FVector Location = Padel::ToWorld(ShotController->GetMotion().GetPosition());
	Location.Z += PadelPlayerPawnDetail::CapsuleHalfHeight;
	SetActorLocation(Location);
	Animation->Present();
}

void APadelPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const APadelPlayerController* PC = Cast<APadelPlayerController>(GetController());
	InputConfig = PC ? PC->GetInputConfig() : nullptr;
	if (!EnhancedInput || !InputConfig)
	{
		UE_LOG(LogPadel, Error, TEXT("APadelPlayerPawn needs Enhanced Input and APadelPlayerController's input config."));
		return;
	}

	// The controller needs held state, not events: it finds presses and releases itself
	// on the fixed step, so the same input frames replay the same swings.
	for (const EPadelInputAction Action : { EPadelInputAction::Move, EPadelInputAction::Sprint, EPadelInputAction::ShotDrive,
		EPadelInputAction::ShotLob, EPadelInputAction::ShotOverhead, EPadelInputAction::ModAttack, EPadelInputAction::ModTouch })
	{
		EnhancedInput->BindActionValue(InputConfig->GetAction(Action));
	}
}

void APadelPlayerPawn::ShowController(const Padel::Shots::FPlayerShotController& Controller, const Padel::Shots::FPlayerRatings& Ratings)
{
	// The team sign flips at a change of ends (GDD 3.1): turn to face the net again.
	if (Controller.GetTeamSign() != FacingTeamSign)
	{
		FacingTeamSign = Controller.GetTeamSign();
		SetActorRotation(FRotator(0.0, FacingTeamSign > 0.0 ? 180.0 : 0.0, 0.0));
		const Padel::Court::FVec3 Right = Padel::Shots::RightAxis(FacingTeamSign);
		const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Padel::ToWorld(Right)) * Ratings.ForehandSign();
		Racket->SetRelativeLocation(Local * 55.0 + FVector(0.0, 0.0, 10.0));
	}
	FVector Location = Padel::ToWorld(Controller.GetMotion().GetPosition());
	Location.Z += PadelPlayerPawnDetail::CapsuleHalfHeight;
	SetActorLocation(Location);
}
