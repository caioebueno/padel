// F1 body dynamics (docs/animation/MOVEMENT_FLUENCY.md) on a real skeleton: the pelvis drop
// keeps the feet planted, the split-step hop lifts them, the chest twist turns the shoulders
// and the forward lean carries the head toward the net. The model's numbers are checked by
// the native tests (Anim.Body); this checks the bones do what the numbers say.
#include "Misc/AutomationTest.h"

#include "Animation/PadelBodyDynamics.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Misc/ScopeExit.h"
#include "UI/Model/PadelUiMatchSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelBodyDynamicsTests
{
	struct FSample
	{
		FVector Pelvis;
		FVector Feet[2];
		FVector Head;
		FVector ShoulderLine; // upperarm_l -> upperarm_r, component space
	};

	FVector CS(const USkeletalMeshComponent* Body, const TCHAR* Bone)
	{
		return Body->GetComponentTransform().InverseTransformPosition(Body->GetSocketLocation(FName(Bone)));
	}

	FSample Pose(USkeletalMeshComponent* Body, UPadelPlayerAnimInstance* Anim, const FPadelBodyDynamics& Dyn)
	{
		Anim->Body = Dyn;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Body->TickAnimation(0.0f, false);
			Body->RefreshBoneTransforms();
		}
		FSample S;
		S.Pelvis = CS(Body, TEXT("pelvis"));
		S.Feet[0] = CS(Body, TEXT("foot_l"));
		S.Feet[1] = CS(Body, TEXT("foot_r"));
		S.Head = CS(Body, TEXT("head"));
		S.ShoulderLine = CS(Body, TEXT("upperarm_r")) - CS(Body, TEXT("upperarm_l"));
		return S;
	}

	double YawDeg(const FVector& V)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelBodyDynamicsTest, "Padel.Animation.BodyDynamics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelBodyDynamicsTest::RunTest(const FString& Parameters)
{
	using namespace PadelBodyDynamicsTests;
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
	for (int32 Tick = 0; Tick < 10; ++Tick)
	{
		Director->Tick(1.0f / 60.0f);
	}

	int32 Checked = 0;
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		if (!Pawn || !Pawn->GetAnimation()->HasSkeletalBody())
		{
			continue;
		}
		USkeletalMeshComponent* Driver = Pawn->GetAnimation()->GetDriverBody();
		UPadelPlayerAnimInstance* Anim = Driver ? Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance()) : nullptr;
		if (!Anim || Driver->GetBoneIndex(FName(TEXT("head"))) == INDEX_NONE)
		{
			continue;
		}
		// Hold the arm still so only the body dynamics move the bones.
		Anim->RawSwingWeight = 0.0f;
		Anim->ContactWarpWeight = 0.0f;

		const FSample Base = Pose(Driver, Anim, FPadelBodyDynamics());

		FPadelBodyDynamics Drop;
		Drop.PelvisDropUnits = -5.0f;
		const FSample Dropped = Pose(Driver, Anim, Drop);
		TestEqual(FString::Printf(TEXT("Pawn %d: the pelvis drops 5 cm"), Id), Dropped.Pelvis.Z - Base.Pelvis.Z, -5.0, 0.5);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			TestTrue(FString::Printf(TEXT("Pawn %d: foot %d stays planted on a drop (moved %.2f cm)"), Id, Side, FVector::Dist(Dropped.Feet[Side], Base.Feet[Side])),
				FVector::Dist(Dropped.Feet[Side], Base.Feet[Side]) <= 1.0);
		}

		FPadelBodyDynamics Hop;
		Hop.HopLiftUnits = 6.0f;
		const FSample Hopped = Pose(Driver, Anim, Hop);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			TestEqual(FString::Printf(TEXT("Pawn %d: foot %d leaves the ground on the hop"), Id, Side), Hopped.Feet[Side].Z - Base.Feet[Side].Z, 6.0, 0.5);
		}

		FPadelBodyDynamics Twist;
		Twist.ChestTwistDeg = 20.0f;
		const FSample Twisted = Pose(Driver, Anim, Twist);
		const double Turned = FMath::FindDeltaAngleDegrees(YawDeg(Base.ShoulderLine), YawDeg(Twisted.ShoulderLine));
		TestEqual(FString::Printf(TEXT("Pawn %d: the shoulders turn with the chest twist"), Id), Turned, 20.0, 3.0);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			TestTrue(FString::Printf(TEXT("Pawn %d: foot %d stays put on a twist"), Id, Side), FVector::Dist(Twisted.Feet[Side], Base.Feet[Side]) <= 0.1);
		}

		FPadelBodyDynamics Lean;
		Lean.LeanForwardDeg = 10.0f;
		const FSample Leaned = Pose(Driver, Anim, Lean);
		// Bodies face +Y in component space.
		TestTrue(FString::Printf(TEXT("Pawn %d: a forward lean carries the head toward the net (%.1f cm)"), Id, Leaned.Head.Y - Base.Head.Y),
			Leaned.Head.Y - Base.Head.Y > 5.0);

		Pose(Driver, Anim, FPadelBodyDynamics());
		++Checked;
	}
	if (Checked == 0)
	{
		AddInfo(TEXT("No skeletal bodies on this machine; skipped."));
	}
	Director->CloseMatch();
	return true;
}

#endif
