#include "Gameplay/PadelMatchDirector.h"

#include "AI/PadelAiData.h"
#include "AI/PadelPartnerCommands.h"
#include "BallState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Gameplay/PadelPlayerController.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Gameplay/PadelShotUnits.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "Presentation/PadelMatchEvents.h"
#include "UI/Model/PadelUiCoreAdapter.h"
#include "UI/PadelHUD.h"
#include "UObject/ConstructorHelpers.h"

#include <optional>
#include <string>

namespace PadelMatchDirectorDetail
{
	// Engine sphere is 100 cm across; the ball is 6.5 cm (GDD 2).
	constexpr double BallMeshScale = Padel::BallRadius * 2.0;
	// A long hitch runs at most this many fixed ticks in one frame, then drops the
	// backlog rather than spiral (same rule as the practice director).
	constexpr int32 MaxTicksPerFrame = 12;

	bool LoadPhysics(Padel::FBallPhysicsParams& Out)
	{
		FString Text;
		const FString Path = FPaths::Combine(FPadelShotData::DataDirectory(), TEXT("Physics.json"));
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogPadel, Error, TEXT("Can't read %s"), *Path);
			return false;
		}
		std::string Error;
		if (!Padel::ParseBallPhysicsJson(TCHAR_TO_UTF8(*Text), Out, Error))
		{
			UE_LOG(LogPadel, Error, TEXT("Physics.json: %s"), UTF8_TO_TCHAR(Error.c_str()));
			return false;
		}
		return true;
	}

	// The setup screen's keys are lower case ("medium"), the rows' names are not.
	const Padel::AI::FAiDifficulty* FindDifficulty(const std::vector<Padel::AI::FAiDifficulty>& Rows, const std::string& Key)
	{
		const FString Wanted = UTF8_TO_TCHAR(Key.c_str());
		for (const Padel::AI::FAiDifficulty& Row : Rows)
		{
			if (Wanted.Equals(UTF8_TO_TCHAR(Row.Name.c_str()), ESearchCase::IgnoreCase))
			{
				return &Row;
			}
		}
		return nullptr;
	}
}

APadelMatchDirector::APadelMatchDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// GDD 6.4: the four slice players, ids as in the match (0, 1 Team A; 2, 3 Team B).
	PlayerRows = { FName(TEXT("selva_arunde")), FName(TEXT("mikko_ventara")), FName(TEXT("ilan_corvisa")),
		FName(TEXT("ruben_halvaro")) };

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	BallMesh->SetupAttachment(RootComponent);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetCastShadow(true);
	if (Sphere.Succeeded())
	{
		BallMesh->SetStaticMesh(Sphere.Object);
		BallMesh->SetWorldScale3D(FVector(PadelMatchDirectorDetail::BallMeshScale));
	}
	BallMesh->SetVisibility(false);
}

void APadelMatchDirector::BeginPlay()
{
	Super::BeginPlay();
	LoadData();
}

void APadelMatchDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseMatch();
	if (BoundHud)
	{
		BoundHud->OnMatchRequested.RemoveAll(this);
		BoundHud->OnMatchClosed.RemoveAll(this);
		BoundHud->OnGoldenReceiverPicked.RemoveAll(this);
		BoundHud = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool APadelMatchDirector::LoadData()
{
	if (bDataLoaded)
	{
		return bDataOk;
	}
	bDataLoaded = true;

	ShotData = FPadelShotData::Load();
	bool bOk = ShotData.bOk;
	bOk &= PadelMatchDirectorDetail::LoadPhysics(Physics);
	bOk &= Padel::AIData::LoadTactics(Tactics);
	bOk &= Padel::AIData::LoadDifficulties(Difficulties);
	if (PlayerRows.Num() != Padel::NumPlayers)
	{
		UE_LOG(LogPadel, Error, TEXT("APadelMatchDirector needs %d player rows, has %d."), Padel::NumPlayers, PlayerRows.Num());
		bOk = false;
	}
	else
	{
		for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
		{
			bOk &= FPadelShotData::LoadPlayer(PlayerRows[Id], Players[Id]);
		}
	}
	bDataOk = bOk;
	if (!bDataOk)
	{
		UE_LOG(LogPadel, Error, TEXT("Matches disabled: the match data did not load (see the errors above)."));
	}
	return bDataOk;
}

void APadelMatchDirector::BindHud()
{
	// The HUD is created by the player controller after the game mode spawns this actor.
	if (BoundHud)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APadelHUD* Hud = PC ? Cast<APadelHUD>(PC->GetHUD()) : nullptr;
	if (!Hud)
	{
		return;
	}
	BoundHud = Hud;
	Hud->OnMatchRequested.AddUObject(this, &APadelMatchDirector::HandleMatchRequested);
	Hud->OnMatchClosed.AddUObject(this, &APadelMatchDirector::HandleMatchClosed);
	Hud->OnGoldenReceiverPicked.AddUObject(this, &APadelMatchDirector::HandleGoldenReceiverPicked);
}

void APadelMatchDirector::BindPartnerCommands()
{
	if (bPartnerCommandsBound)
	{
		return;
	}
	APadelPlayerController* PC = Cast<APadelPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	const UPadelInputConfig* Config = PC ? PC->GetInputConfig() : nullptr;
	if (!Config)
	{
		return;
	}
	EnableInput(PC);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		return;
	}
	// GDD 7: the D-pad gives the human's AI partner a command. The HUD binds the same
	// actions to show the prompt, so neither consumes them.
	using A = EPadelInputAction;
	Input->BindAction(Config->GetAction(A::CmdNet), ETriggerEvent::Started, this, &APadelMatchDirector::CmdNet);
	Input->BindAction(Config->GetAction(A::CmdBack), ETriggerEvent::Started, this, &APadelMatchDirector::CmdBack);
	Input->BindAction(Config->GetAction(A::CmdMiddle), ETriggerEvent::Started, this, &APadelMatchDirector::CmdMiddle);
	Input->BindAction(Config->GetAction(A::CmdYours), ETriggerEvent::Started, this, &APadelMatchDirector::CmdYours);
	bPartnerCommandsBound = true;
}

bool APadelMatchDirector::StartMatch(const Padel::Ui::FMatchSetup& Setup)
{
	using namespace Padel;
	CloseMatch();
	if (!LoadData())
	{
		return false;
	}
	const AI::FAiDifficulty* Difficulty = PadelMatchDirectorDetail::FindDifficulty(Difficulties, Setup.DifficultyKey());
	if (!Difficulty)
	{
		UE_LOG(LogPadel, Error, TEXT("Data/Difficulty.json has no row for %s."), UTF8_TO_TCHAR(Setup.DifficultyKey().c_str()));
		return false;
	}

	HumanPlayerId = IsValidPlayer(Setup.HumanPlayerId) ? Setup.HumanPlayerId : -1;
	const uint64 MatchSeed = static_cast<uint64>(Seed) + static_cast<uint64>(MatchesStarted++);

	AI::FAiMatchSetup RunnerSetup;
	RunnerSetup.Players = Players;
	// GDD 8: the difficulty is the opponents'. The human's partner plays at the same
	// level, so a harder match is harder for both sides of the net.
	RunnerSetup.Difficulties = { Difficulty, Difficulty };
	RunnerSetup.HumanPlayerId = HumanPlayerId;
	RunnerSetup.Seed = MatchSeed;
	RunnerSetup.bAutoBeginPoints = true;

	Match = MakeUnique<FMatchSimulation>(Ui::ToMatchConfig(Setup), Physics, MatchSeed);
	Runner = MakeUnique<AI::FAiMatchRunner>(*Match, ShotData.Library, ShotData.Movement, Tactics, RunnerSetup);
	if (UPadelMatchEvents* Events = GetWorld() ? GetWorld()->GetSubsystem<UPadelMatchEvents>() : nullptr)
	{
		Events->Attach(*Match);
	}
	else
	{
		UE_LOG(LogPadel, Warning, TEXT("No UPadelMatchEvents subsystem: the match plays without UI or audio."));
	}

	SpawnPawns();
	Accumulator = 0.0;
	// Start broadcasts the first score and serve, so it runs after the events attach.
	Runner->Start();
	SyncView();
	UE_LOG(LogPadel, Log, TEXT("Match started: seed %llu, human player %d, %s."), static_cast<unsigned long long>(MatchSeed),
		HumanPlayerId, UTF8_TO_TCHAR(Setup.DifficultyKey().c_str()));
	return true;
}

void APadelMatchDirector::CloseMatch()
{
	if (!Match)
	{
		return;
	}
	// The subsystem and the runner both hold the match: let go of it before it goes.
	if (UPadelMatchEvents* Events = GetWorld() ? GetWorld()->GetSubsystem<UPadelMatchEvents>() : nullptr)
	{
		Events->Detach();
	}
	Runner.Reset();
	Match.Reset();
	DestroyPawns();
	BallMesh->SetVisibility(false);
	HumanPlayerId = -1;
}

APadelPlayerPawn* APadelMatchDirector::GetPawn(int32 PlayerId) const
{
	return Pawns.IsValidIndex(PlayerId) ? Pawns[PlayerId].Get() : nullptr;
}

void APadelMatchDirector::SpawnPawns()
{
	DestroyPawns();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Pawns.SetNum(Padel::NumPlayers);
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		Pawns[Id] = World->SpawnActor<APadelPlayerPawn>(APadelPlayerPawn::StaticClass(), FTransform::Identity, Params);
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && Pawns.IsValidIndex(HumanPlayerId) && Pawns[HumanPlayerId])
	{
		// Keep the broadcast camera (GDD 9) instead of switching the view to the pawn.
		AActor* Camera = PC->GetViewTarget();
		PC->bAutoManageActiveCameraTarget = false;
		PC->Possess(Pawns[HumanPlayerId]);
		PC->SetViewTarget(Camera);
	}
}

void APadelMatchDirector::DestroyPawns()
{
	for (TObjectPtr<APadelPlayerPawn>& Pawn : Pawns)
	{
		if (Pawn)
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->UnPossess();
			}
			Pawn->Destroy();
		}
	}
	Pawns.Reset();
}

void APadelMatchDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BindHud();
	BindPartnerCommands();
	if (!Runner)
	{
		return;
	}

	// Fixed step (CLAUDE.md): frame time only decides how many ticks run.
	Accumulator += DeltaSeconds;
	int32 Ticks = 0;
	while (Accumulator >= Padel::SimTickSeconds && Ticks < PadelMatchDirectorDetail::MaxTicksPerFrame)
	{
		Accumulator -= Padel::SimTickSeconds;
		StepOnce();
		++Ticks;
		if (!Runner)
		{
			return; // A listener closed the match mid-step.
		}
	}
	if (Ticks == PadelMatchDirectorDetail::MaxTicksPerFrame)
	{
		Accumulator = 0.0;
	}
	SyncView();
}

void APadelMatchDirector::StepTicks(int32 Count)
{
	for (int32 Index = 0; Index < Count && Runner; ++Index)
	{
		StepOnce();
	}
	if (Runner)
	{
		SyncView();
	}
}

void APadelMatchDirector::StepOnce()
{
	if (Match->Phase() == Padel::EMatchPhase::MatchOver)
	{
		return; // The HUD shows the result and closes the match.
	}
	if (const APadelPlayerPawn* Human = GetPawn(HumanPlayerId))
	{
		Runner->SetHumanInput(Human->SampleInput());
	}
	Runner->Tick();
}

void APadelMatchDirector::SyncView()
{
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		if (APadelPlayerPawn* Pawn = GetPawn(Id))
		{
			// The runner rebuilds its controllers at a change of ends: read them fresh each time.
			Pawn->ShowController(Runner->Player(Id), Players[Id]);
		}
	}
	// The ball the players see: the server's toss while the match holds the ball, the
	// match ball during the rally.
	const Padel::Shots::FBallView Ball = Runner->Ball().Current();
	BallMesh->SetVisibility(Ball.bLive);
	if (Ball.bLive)
	{
		BallMesh->SetWorldLocation(Padel::ToWorld(Ball.Position));
	}
}

void APadelMatchDirector::HandleMatchRequested(const Padel::Ui::FMatchSetup& Setup)
{
	StartMatch(Setup);
}

void APadelMatchDirector::HandleMatchClosed()
{
	CloseMatch();
}

void APadelMatchDirector::HandleGoldenReceiverPicked(int32 PlayerId)
{
	if (Runner && Runner->IsWaitingForGoldenReceiver())
	{
		Runner->ChooseGoldenPointReceiver(PlayerId);
	}
}

void APadelMatchDirector::GiveCommand(EPadelInputAction Action)
{
	const std::optional<Padel::AI::EPartnerCommand> Command = Padel::AIInput::CommandForAction(Action);
	if (Runner && Command && Runner->GiveCommand(*Command))
	{
		UE_LOG(LogPadel, Verbose, TEXT("Partner took command %d"), static_cast<int32>(*Command));
	}
}
