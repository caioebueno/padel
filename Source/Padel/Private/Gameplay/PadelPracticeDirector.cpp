#include "Gameplay/PadelPracticeDirector.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Gameplay/PadelInputConfig.h"
#include "Gameplay/PadelPlayerController.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Gameplay/PadelShotUnits.h"
#include "Kismet/GameplayStatics.h"
#include "PadelLog.h"
#include "UObject/ConstructorHelpers.h"

namespace PadelPracticeDirectorDetail
{
	// The human is player 0, Team A, on +X (GDD 2), starting behind the baseline centre.
	constexpr int32 HumanPlayerId = 0;
	constexpr double HumanTeamSign = 1.0;
	// Start spot in metres: a placement, not tuning.
	const Padel::Court::FVec3 HumanStart{ 8.5, -1.5, 0.0 };
	// Engine sphere is 100 cm across; the ball is 6.5 cm (GDD 2).
	constexpr double BallMeshScale = 0.065;
	// The machine stands in for the opponents' hit, so it seeds that stream differently.
	constexpr uint64 MachineSeedSalt = 0x9E3779B97F4A7C15ull;
}

APadelPracticeDirector::APadelPracticeDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	BallMesh->SetupAttachment(RootComponent);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetCastShadow(true);
	if (Sphere.Succeeded())
	{
		BallMesh->SetStaticMesh(Sphere.Object);
		BallMesh->SetWorldScale3D(FVector(PadelPracticeDirectorDetail::BallMeshScale));
	}
	BallMesh->SetVisibility(false);
}

void APadelPracticeDirector::BeginPlay()
{
	Super::BeginPlay();

	Data = FPadelShotData::Load();
	if (!Data.bOk || !FPadelShotData::LoadPlayer(PlayerRow, Ratings))
	{
		UE_LOG(LogPadel, Error, TEXT("Practice disabled: the shot data did not load (see the errors above)."));
		SetActorTickEnabled(false);
		return;
	}
	Ball = MakeUnique<Padel::Shots::FPracticeBall>(&Data.PracticeBall);
	Machine = MakeUnique<Padel::Shots::FBallMachine>(Data.Feeds, PadelPracticeDirectorDetail::HumanTeamSign,
		static_cast<uint64>(Seed) ^ PadelPracticeDirectorDetail::MachineSeedSalt);
}

void APadelPracticeDirector::SpawnAndPossessPlayer()
{
	using namespace PadelPracticeDirectorDetail;
	APadelPlayerController* PC = Cast<APadelPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!PC || !PC->GetInputConfig())
	{
		return; // The controller builds its input config in BeginPlay; try again next tick.
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PlayerPawn = GetWorld()->SpawnActor<APadelPlayerPawn>(APadelPlayerPawn::StaticClass(), FTransform::Identity, Params);
	PlayerPawn->InitShots(HumanPlayerId, HumanTeamSign, &Ratings, &Data.Library, &Data.Movement, HumanStart, static_cast<uint64>(Seed));

	// Keep the broadcast camera (GDD 9) instead of switching the view to the pawn.
	AActor* Camera = PC->GetViewTarget();
	PC->bAutoManageActiveCameraTarget = false;
	PC->Possess(PlayerPawn);
	PC->SetViewTarget(Camera);

	BindPracticeInput();
	bReady = true;
}

void APadelPracticeDirector::BindPracticeInput()
{
	APadelPlayerController* PC = Cast<APadelPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	EnableInput(PC);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	const UPadelInputConfig* Config = PC ? PC->GetInputConfig() : nullptr;
	if (!Input || !Config)
	{
		return;
	}
	using A = EPadelInputAction;
	Input->BindAction(Config->GetAction(A::CmdMiddle), ETriggerEvent::Started, this, &APadelPracticeDirector::PreviousDrill);
	Input->BindAction(Config->GetAction(A::CmdYours), ETriggerEvent::Started, this, &APadelPracticeDirector::NextDrill);
	Input->BindAction(Config->GetAction(A::CmdNet), ETriggerEvent::Started, this, &APadelPracticeDirector::TogglePause);
	Input->BindAction(Config->GetAction(A::CmdBack), ETriggerEvent::Started, this, &APadelPracticeDirector::FeedNow);
}

void APadelPracticeDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Ball)
	{
		return;
	}
	if (!bReady)
	{
		SpawnAndPossessPlayer();
		if (!bReady)
		{
			return;
		}
	}

	// Fixed step (CLAUDE.md): frame time only decides how many steps run.
	const double Step = Data.PracticeBall.StepS;
	Accumulator += DeltaSeconds;
	int32 Steps = 0;
	while (Accumulator >= Step && Steps < 12)
	{
		Accumulator -= Step;
		StepSimulation(Step);
		++Steps;
	}
	if (Steps == 12)
	{
		Accumulator = 0.0; // A long hitch: drop the backlog rather than spiral.
	}

	PlayerPawn->SyncFromSimulation();
	BallMesh->SetVisibility(Ball->IsLive() || !Ball->IsDone());
	BallMesh->SetWorldLocation(Padel::ToWorld(Ball->GetPosition()));
	DebugInfo.Drill = UTF8_TO_TCHAR(Machine->GetDrill().c_str());
	DebugInfo.bMachinePaused = Machine->IsPaused();
	DrawDebug();
}

void APadelPracticeDirector::StepSimulation(double Dt)
{
	using namespace Padel::Shots;
	FPlayerShotController* Player = PlayerPawn->GetShotController();

	if (Machine->IsServeDrill())
	{
		if (!Ball->IsLive() && Ball->IsDone() && !Player->IsAwaitingServe() && Player->GetPhase() == ESwingPhase::Idle)
		{
			Player->SetAwaitingServe(true);
		}
	}
	else if (Player->IsAwaitingServe())
	{
		Player->SetAwaitingServe(false);
	}

	if (Machine->Tick(Dt, *Ball))
	{
		// The feed is the "opponent's contact" for the split-step (GDD 6.3).
		if (Player->NotifyOpponentContact())
		{
			++DebugInfo.SplitSteps;
		}
	}

	const FSwingEvents Events = Player->Tick(PlayerPawn->SampleInput(), Dt, *Ball);
	PlayerPawn->StepAnimation(Events, Dt, Ball->IsLive());
	if (Events.Outcome != ESwingOutcome::None)
	{
		DebugInfo.LastOutcome = Events.Outcome;
	}
	if (Events.Outcome == ESwingOutcome::Hit)
	{
		DebugInfo.bHasShot = true;
		DebugInfo.LastShot = Events.Intent;
		++DebugInfo.Hits;
		if (Events.Intent.TimingGrade == Padel::ETimingGrade::Perfect)
		{
			++DebugInfo.Perfects;
		}
		UE_LOG(LogPadel, Verbose, TEXT("Shot: %s %s %+.0f ms %.0f km/h"), UTF8_TO_TCHAR(Padel::ShotTypeName(Events.Intent.ShotType)),
			UTF8_TO_TCHAR(Padel::TimingGradeName(Events.Intent.TimingGrade)), Events.Intent.TimingOffsetS * 1000.0, Events.Intent.SpeedKmh());
	}
	Ball->Advance(Dt);
}

void APadelPracticeDirector::DrawDebug() const
{
#if !UE_BUILD_SHIPPING
	// GDD 7: the target is shown only in the debug overlay.
	const UWorld* World = GetWorld();
	const Padel::Shots::FPlayerShotController* Player = PlayerPawn ? PlayerPawn->GetShotController() : nullptr;
	if (Player && Player->HasIdealMoment())
	{
		DrawDebugSphere(World, Padel::ToWorld(Player->GetIdealBallPosition()), 12.0f, 8, FColor::Cyan);
	}
	if (DebugInfo.bHasShot)
	{
		const FVector Aimed = Padel::ToWorld(DebugInfo.LastShot.AimedTarget);
		DrawDebugCircle(World, Aimed + FVector(0, 0, 2), DebugInfo.LastShot.ScatterRadiusM * 100.0f, 32, FColor::Yellow, false, -1.0f, 0, 2.0f,
			FVector(0, 1, 0), FVector(1, 0, 0), false);
		DrawDebugSphere(World, Padel::ToWorld(DebugInfo.LastShot.Target), 10.0f, 8, FColor::Red);
	}
#endif
}

void APadelPracticeDirector::PreviousDrill()
{
	if (Machine)
	{
		Machine->NextDrill(-1);
	}
}

void APadelPracticeDirector::NextDrill()
{
	if (Machine)
	{
		Machine->NextDrill(1);
	}
}

void APadelPracticeDirector::TogglePause()
{
	if (Machine)
	{
		Machine->SetPaused(!Machine->IsPaused());
	}
}

void APadelPracticeDirector::FeedNow()
{
	if (Machine && Ball && !Ball->IsLive() && !Machine->IsServeDrill())
	{
		Machine->FeedNow(*Ball);
		if (PlayerPawn && PlayerPawn->GetShotController())
		{
			PlayerPawn->GetShotController()->NotifyOpponentContact();
		}
	}
}
