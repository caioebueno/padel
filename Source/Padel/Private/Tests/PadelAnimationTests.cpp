// The animation tuning loads through Unreal DataTables from the same Data/*.json files
// the native tests read, and a practice drive shows its contact frame on the hit tick
// (docs/decisions/0005-player-animation.md).
#include "Misc/AutomationTest.h"

#include "Animation/Model/PlayerAnimModel.h"
#include "Animation/PadelAnimData.h"
#include "Data/PadelShotData.h"
#include "Shots/BallMachine.h"
#include "Shots/PlayerShotController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelAnimDataLoadTest, "Padel.Animation.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelAnimDataLoadTest::RunTest(const FString& Parameters)
{
	const FPadelAnimData Data = FPadelAnimData::Load();
	for (const FString& Error : Data.Errors)
	{
		AddError(Error);
	}
	TestTrue(TEXT("Animation data loads"), Data.bOk);
	TestTrue(TEXT("Clips for every slice shot"), Data.Clips.Validate().empty());
	TestEqual(TEXT("One mesh per team"), Data.TeamMeshes.Num(), 2);

	// docs/decisions/0006-metahuman-players.md: every slice player names a MetaHuman (which may
	// not be on disk: then the placeholder shows), and the retargeter is set.
	for (const TCHAR* Player : { TEXT("selva_arunde"), TEXT("mikko_ventara"), TEXT("ilan_corvisa"), TEXT("ruben_halvaro") })
	{
		TestTrue(FString::Printf(TEXT("%s has a MetaHuman row"), Player), Data.PlayerBodies.Contains(FName(Player)));
	}
	TestFalse(TEXT("MetaHuman retargeter set"), Data.MetaHumanRetargeter.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelAnimContactSyncTest, "Padel.Animation.ContactSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelAnimContactSyncTest::RunTest(const FString& Parameters)
{
	using namespace Padel::Shots;
	using namespace Padel::Animation;
	const FPadelShotData Shots = FPadelShotData::Load();
	const FPadelAnimData Anim = FPadelAnimData::Load();
	FPlayerRatings Player;
	if (!TestTrue(TEXT("Data"), Shots.bOk && Anim.bOk && FPadelShotData::LoadPlayer(FName(TEXT("practice_default")), Player)))
	{
		return false;
	}
	FPracticeBall Ball(&Shots.PracticeBall);
	FBallMachine Machine(Shots.Feeds, 1.0, 3);
	FPlayerShotController Controller(0, 1.0, &Player, &Shots.Library, &Shots.Movement, FVec3{ 8.5, 0.0, 0.0 }, 3);
	FPlayerAnimModel Model(&Anim.Settings, &Anim.Clips, &Shots.Library);

	// Hold the drive from the feed until the controller fires: the hit tick must show
	// the clip's contact frame, with the placeholder racket on the ball.
	const double Dt = Shots.PracticeBall.StepS;
	bool bHit = false;
	for (int32 Step = 0; Step < 120 * 6 && !bHit; ++Step)
	{
		Machine.Tick(Dt, Ball);
		FPlayerInputFrame Input;
		Input.bDrive = Controller.HasIdealMoment() && Controller.TimeToIdealS() < 0.3;
		const FSwingEvents Events = Controller.Tick(Input, Dt, Ball);
		const FPlayerAnimFrame& Frame = Model.Step(FPlayerAnimInput::FromController(Controller, Events, true), Dt);
		Ball.Advance(Dt);
		if (Events.Outcome == ESwingOutcome::Hit)
		{
			bHit = true;
			TestTrue(TEXT("Contact frame on the hit tick"), Frame.bContactFrame);
			if (TestNotNull(TEXT("Swing clip"), Frame.Clip))
			{
				TestEqual(TEXT("Playhead at the clip's contact time"), Frame.ClipTimeS, Frame.Clip->ContactS, 1e-6);
			}
			TestTrue(TEXT("Placeholder racket on the ball"), Length(Sub(Frame.RacketPosition, Events.Intent.Origin)) < 1e-6);
		}
		else
		{
			TestFalse(TEXT("No contact frame before the hit"), Frame.bContactFrame);
		}
	}
	TestTrue(TEXT("The practice player hits a fed ball"), bHit);
	return true;
}

#endif

// docs/decisions/0006-metahuman-players.md: with the locomotion clips on disk (made by
// Tools/AssetPipeline/unreal/animation_setup.py) the pawns get a skeletal body animated by
// the C++ pose, with no AnimBP. Skipped on a clone without the generated content.
#include "Animation/AnimSequenceBase.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Misc/ScopeExit.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "AnimationRuntime.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelAnimNativePoseTest, "Padel.Animation.NativePose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelAnimNativePoseTest::RunTest(const FString& Parameters)
{
	const FPadelAnimData Anim = FPadelAnimData::Load();
	if (!LoadObject<UAnimSequenceBase>(nullptr, *Anim.IdleAsset, nullptr, LOAD_Quiet | LOAD_NoWarn))
	{
		AddInfo(TEXT("Locomotion clips not generated on this machine; skipped."));
		return true;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	APadelMatchDirector* Director = World->SpawnActor<APadelMatchDirector>();
	Padel::Ui::FMatchSetup Setup;
	Setup.SetsToWin = 1;
	Setup.HumanPlayerId = -1;
	if (!TestNotNull(TEXT("Director"), Director) || !TestTrue(TEXT("Match started"), Director->StartMatch(Setup)))
	{
		return false;
	}

	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		if (!TestNotNull(TEXT("Pawn"), Pawn))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("Pawn %d has a skeletal body"), Id), Pawn->GetAnimation()->HasSkeletalBody());
	}

	// Play a few seconds and check the driver body leaves its reference pose and stays finite.
	APadelPlayerPawn* Pawn = Director->GetPawn(0);
	USkeletalMeshComponent* Body = nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Pawn);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetSkeletalMeshAsset())
		{
			Body = Mesh;
			break;
		}
	}
	if (!TestNotNull(TEXT("Driver mesh"), Body))
	{
		return false;
	}
	const int32 Hand = Body->GetBoneIndex(FName(TEXT("hand_r")));
	const FVector RefHand = FAnimationRuntime::GetComponentSpaceTransformRefPose(Body->GetSkeletalMeshAsset()->GetRefSkeleton(), Hand).GetLocation();
	const int32 Shoulder = Body->GetBoneIndex(FName(TEXT("upperarm_r")));
	const int32 Foot = Body->GetBoneIndex(FName(TEXT("foot_r")));
	double MaxOffset = 0.0;
	double LowestHandBelowShoulder = 0.0;
	double HighestFoot = 0.0;
	for (int32 Frame = 0; Frame < 240; ++Frame)
	{
		Director->StepTicks(2);
		Pawn->PresentAnimation();
		Body->TickAnimation(1.0f / 60.0f, false);
		Body->RefreshBoneTransforms();
		Body->FinalizeBoneTransform();
		const FVector HandNow = Body->GetBoneTransform(Hand, FTransform::Identity).GetLocation();
		if (HandNow.ContainsNaN())
		{
			AddError(TEXT("hand_r went NaN"));
			break;
		}
		MaxOffset = FMath::Max(MaxOffset, FVector::Dist(HandNow, RefHand));
		const double ShoulderZ = Body->GetBoneTransform(Shoulder, FTransform::Identity).GetLocation().Z;
		LowestHandBelowShoulder = FMath::Max(LowestHandBelowShoulder, ShoulderZ - HandNow.Z);
		HighestFoot = FMath::Max(HighestFoot, Body->GetBoneTransform(Foot, FTransform::Identity).GetLocation().Z);
	}
	// A MetaHuman (when one is on disk): every part's bones stay on the body. A part whose
	// LOD needs bones its leader (the body) no longer updates stretches toward the origin.
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		AActor* MetaHuman = Director->GetPawn(Id) ? Director->GetPawn(Id)->GetAnimation()->GetMetaHuman() : nullptr;
		if (!MetaHuman)
		{
			continue;
		}
		TInlineComponentArray<USkeletalMeshComponent*> Parts(MetaHuman);
		for (USkeletalMeshComponent* Part : Parts)
		{
			Part->TickAnimation(1.0f / 60.0f, false);
			Part->RefreshBoneTransforms();
		}
		const FVector Root = MetaHuman->GetActorLocation();
		for (USkeletalMeshComponent* Part : Parts)
		{
			if (!Part->GetSkeletalMeshAsset())
			{
				continue;
			}
			double Worst = 0.0;
			FName WorstBone;
			for (const FName& Bone : Part->GetAllSocketNames())
			{
				const FVector Local = Part->GetSocketLocation(Bone) - Root;
				const double Off = FMath::Max(FMath::Abs(Local.X), FMath::Abs(Local.Y));
				const double Below = -Local.Z;
				const double Bad = FMath::Max(Off - 150.0, FMath::Max(Below - 20.0, Local.Z - 250.0));
				if (Bad > Worst)
				{
					Worst = Bad;
					WorstBone = Bone;
				}
			}
			TestTrue(FString::Printf(TEXT("MetaHuman %s part %s stays on the body (worst %s, %.0f cm out)"), *MetaHuman->GetName(),
				*Part->GetName(), *WorstBone.ToString(), Worst), Worst <= 0.0);
		}
	}

	TestTrue(TEXT("The arm leaves the T-pose (cm)"), MaxOffset > 20.0);
	// A clip retargeted with a wrong pose shows up as arms held out or a body floating.
	TestTrue(FString::Printf(TEXT("Arms come down from the T-pose (hand %.0f cm below the shoulder)"), LowestHandBelowShoulder), LowestHandBelowShoulder > 25.0);
	TestTrue(FString::Printf(TEXT("Feet stay near the ground (highest %.0f cm)"), HighestFoot), HighestFoot < 40.0);
	Director->CloseMatch();
	return true;
}

#endif
