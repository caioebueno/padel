// Movement plan N3 (shots doc 4.2): at contact the racket's face centre meets the ball,
// on the body actually on screen (the placeholder, or a MetaHuman after the retarget).
// The match's own AI places the targets; here they're set directly, across a band of
// heights and both sides, so the check doesn't depend on what the rally happens to do.
#include "Misc/AutomationTest.h"

#include "Animation/Model/PlayerAnimModel.h"
#include "Animation/PadelAnimData.h"
#include "Animation/PadelContactWarp.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Gameplay/PadelShotUnits.h"
#include "Misc/ScopeExit.h"
#include "UI/Model/PadelUiMatchSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelContactWarpTests
{
	constexpr double ToleranceUnits = 5.0; // done-when: within 5 cm of the ball
	constexpr double FaceCentreUnits = 27.0; // Data/Racket.json FaceCentreM

	struct FTarget
	{
		double RightM;
		double ForwardM;
		double UpM;
	};

	UStaticMeshComponent* FindRacket(APadelPlayerPawn* Pawn)
	{
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Pawn);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetFName() == FName(TEXT("Racket")))
			{
				return Mesh;
			}
		}
		return nullptr;
	}

	void Evaluate(USkeletalMeshComponent* Driver, USkeletalMeshComponent* Shown, UStaticMeshComponent* Racket)
	{
		// Twice: the first pass settles the IK rigs and the retarget processor.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Driver->TickAnimation(0.0f, false);
			Driver->RefreshBoneTransforms();
			if (Shown != Driver)
			{
				Shown->TickAnimation(0.0f, false);
				Shown->RefreshBoneTransforms();
			}
		}
		Racket->UpdateComponentToWorld();
	}


	/** Sets the driver's racket target directly (arm only, like a clip with zero contact fields) and evaluates. */
	void PoseAt(APadelPlayerPawn* Pawn, const FVector& Target)
	{
		UStaticMeshComponent* Racket = FindRacket(Pawn);
		USkeletalMeshComponent* Driver = Pawn->GetAnimation()->GetDriverBody();
		USkeletalMeshComponent* Shown = Cast<USkeletalMeshComponent>(Racket->GetAttachParent());
		UPadelPlayerAnimInstance* Anim = Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance());
		Anim->RacketTargetWorld = Target;
		Anim->RawSwingWeight = 1.0f;
		Anim->ContactWarpWeight = 0.0f;
		Anim->SwingSequence = nullptr;
		Evaluate(Driver, Shown, Racket);
	}

	/** Target at (right, forward, up) metres in front of Pawn, sides on the racket hand's side. */
	FVector Around(APadelPlayerPawn* Pawn, double RightM, double ForwardM, double UpM)
	{
		const FTransform Frame = Pawn->GetAnimation()->GetDriverBody()->GetComponentTransform();
		const FVector Forward = Frame.TransformVectorNoScale(FVector(0.0, 1.0, 0.0)).GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward) * (Pawn->GetAnimation()->IsLeftHanded() ? -1.0 : 1.0);
		return Frame.GetLocation() + Right * (RightM * 100.0) + Forward * (ForwardM * 100.0) + FVector(0.0, 0.0, UpM * 100.0);
	}

	FName Side(APadelPlayerPawn* Pawn, const TCHAR* Bone)
	{
		return FName(FString(Bone) + (Pawn->GetAnimation()->IsLeftHanded() ? TEXT("_l") : TEXT("_r")));
	}

	/** The topmost spine bone the shown body has (spine_03 on the placeholder, spine_05 on a MetaHuman). */
	FName TopSpine(const USkeletalMeshComponent* Body)
	{
		for (const TCHAR* Name : { TEXT("spine_05"), TEXT("spine_04"), TEXT("spine_03") })
		{
			if (Body->GetBoneIndex(FName(Name)) != INDEX_NONE)
			{
				return FName(Name);
			}
		}
		return FName(TEXT("spine_02"));
	}

	/**
	 * Puts each target in front of Pawn in turn and returns the worst miss (cm) of the racket's
	 * face centre. bClip: drive it through ApplyFrame with a clip that has contact fields (the
	 * body takes part of the reach); otherwise the arm alone, as for zero-field clips.
	 */
	double WorstMiss(FAutomationTestBase& Test, APadelPlayerPawn* Pawn, const TArray<FTarget>& Targets, bool bClip, FString& Report)
	{
		UStaticMeshComponent* Racket = FindRacket(Pawn);
		USkeletalMeshComponent* Driver = Pawn->GetAnimation()->GetDriverBody();
		USkeletalMeshComponent* Shown = Racket ? Cast<USkeletalMeshComponent>(Racket->GetAttachParent()) : nullptr;
		UPadelPlayerAnimInstance* Anim = Driver ? Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance()) : nullptr;
		if (!Racket || !Shown || !Anim || !Racket->GetStaticMesh() || Racket->GetStaticMesh()->GetName() != TEXT("SM_Racket"))
		{
			return -1.0;
		}
		const FTransform Frame = Driver->GetComponentTransform();
		const FVector Forward = Frame.TransformVectorNoScale(FVector(0.0, 1.0, 0.0)).GetSafeNormal2D();
		// Sides are the racket hand's: a left-hander's forehand is on the left (Players.json Handedness).
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward) * (Pawn->GetAnimation()->IsLeftHanded() ? -1.0 : 1.0);
		const FVector Ground = Frame.GetLocation();

		Padel::Animation::FAnimClip Clip;
		Clip.Name = "contact_warp_test";
		Clip.WindupS = 0.4;
		Clip.ContactS = 0.6;
		Clip.EndS = 1.2;
		// Non-zero contact fields: this clip's own sweet spot at contact, so the body warps.
		Clip.Contact = { 0.6, 0.4, 1.0 };

		double Worst = 0.0;
		for (const FTarget& T : Targets)
		{
			const FVector Target = Ground + Right * (T.RightM * 100.0) + Forward * (T.ForwardM * 100.0) + FVector(0.0, 0.0, T.UpM * 100.0);
			if (bClip)
			{
				Padel::Animation::FPlayerAnimFrame F;
				F.Locomotion = Padel::Animation::ELocomotionState::Idle;
				F.Clip = &Clip;
				F.SwingWeight = 1.0;
				F.SwingProgress = 2.0; // the contact frame
				F.bContactFrame = true;
				F.RacketPosition = Padel::ToMetres(Target);
				Anim->ApplyFrame(F, nullptr, nullptr, nullptr, {});
				Test.TestTrue(TEXT("Clip with contact fields gives the body a share"), Anim->ContactWarpWeight > 0.99f);
			}
			else
			{
				Anim->RacketTargetWorld = Target;
				Anim->RawSwingWeight = 1.0f;
				Anim->ContactWarpWeight = 0.0f;
				Anim->SwingSequence = nullptr;
			}
			Evaluate(Driver, Shown, Racket);
			const FVector Face = Racket->GetComponentTransform().TransformPosition(FVector(0.0, 0.0, FaceCentreUnits));
			const double Miss = FVector::Dist(Face, Target);
			Report += FString::Printf(TEXT(" [%.2f,%.2f,%.2f m: %.1f cm]"), T.RightM, T.ForwardM, T.UpM, Miss);
			Worst = FMath::Max(Worst, Miss);
		}
		return Worst;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelContactWarpTest, "Padel.Animation.ContactWarp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelContactWarpTest::RunTest(const FString& Parameters)
{
	using namespace PadelContactWarpTests;
	TestEqual(TEXT("Body weight 0 in the backswing"), PadelContactWarp::BodyWeight(0.8), 0.0f);
	TestEqual(TEXT("Body weight peaks at contact"), PadelContactWarp::BodyWeight(2.0), 1.0f);
	TestTrue(TEXT("Body weight rises over the forward swing"), PadelContactWarp::BodyWeight(1.5) > 0.4f && PadelContactWarp::BodyWeight(1.5) < 0.6f);
	TestEqual(TEXT("Body weight 0 after the follow-through"), PadelContactWarp::BodyWeight(3.0), 0.0f);

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

	// The arm alone (clips with zero contact fields): where a standing player reaches.
	const TArray<FTarget> ArmBand = {
		{ 0.55, 0.35, 0.9 }, { 0.55, 0.35, 1.2 }, { 0.55, 0.35, 1.5 }, { 0.45, 0.30, 1.8 },
		{ -0.45, 0.35, 1.0 }, { -0.45, 0.35, 1.3 },
	};
	// With the body (clips with contact fields): low balls need the knees and the spine.
	const TArray<FTarget> BodyBand = {
		{ 0.6, 0.4, 0.4 }, { 0.6, 0.4, 0.6 }, { 0.6, 0.4, 0.9 }, { 0.6, 0.4, 1.3 }, { 0.5, 0.3, 1.8 },
		{ 0.85, 0.3, 1.0 }, { -0.5, 0.4, 0.5 }, { -0.5, 0.4, 1.0 },
	};

	int32 Checked = 0;
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		if (!Pawn || !Pawn->GetAnimation()->HasSkeletalBody())
		{
			continue;
		}
		const TCHAR* BodyName = Pawn->GetAnimation()->GetMetaHuman()
			? (Pawn->GetAnimation()->IsLeftHanded() ? TEXT("MetaHuman, left-handed") : TEXT("MetaHuman"))
			: (Pawn->GetAnimation()->IsLeftHanded() ? TEXT("placeholder, left-handed") : TEXT("placeholder"));
		FString Report;
		const double ArmMiss = WorstMiss(*this, Pawn, ArmBand, false, Report);
		if (ArmMiss < 0.0)
		{
			continue;
		}
		AddInfo(FString::Printf(TEXT("Pawn %d (%s), arm only:%s"), Id, BodyName, *Report));
		TestTrue(FString::Printf(TEXT("Pawn %d (%s): arm-only face within %.0f cm (worst %.1f)"), Id, BodyName, ToleranceUnits, ArmMiss), ArmMiss <= ToleranceUnits);
		Report.Reset();
		const double BodyMiss = WorstMiss(*this, Pawn, BodyBand, true, Report);
		AddInfo(FString::Printf(TEXT("Pawn %d (%s), with the body:%s"), Id, BodyName, *Report));
		TestTrue(FString::Printf(TEXT("Pawn %d (%s): warped face within %.0f cm (worst %.1f)"), Id, BodyName, ToleranceUnits, BodyMiss), BodyMiss <= ToleranceUnits);
		++Checked;
	}
	if (Checked == 0)
	{
		AddInfo(TEXT("No skeletal bodies with SM_Racket on this machine; skipped."));
	}
	Director->CloseMatch();
	return true;
}

// Movement plan N4: the MetaHuman's arms. A left-hander's racket and reach are on the left
// arm; the forearm twist bones share the hand's roll (no candy-wrap); and the hand still
// meets the ball at the broadcast camera's LOD.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMetaHumanArmsTest, "Padel.Animation.MetaHumanArms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMetaHumanArmsTest::RunTest(const FString& Parameters)
{
	using namespace PadelContactWarpTests;
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

	int32 LeftHanders = 0;
	int32 MetaHumans = 0;
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		if (!Pawn || !Pawn->GetAnimation()->HasSkeletalBody())
		{
			continue;
		}
		UStaticMeshComponent* Racket = FindRacket(Pawn);
		if (!Racket)
		{
			continue;
		}
		const bool bLeft = Pawn->GetAnimation()->IsLeftHanded();
		const FName Socket = Racket->GetAttachSocketName();
		TestEqual(FString::Printf(TEXT("Pawn %d: racket in the %s hand"), Id, bLeft ? TEXT("left") : TEXT("right")),
			Socket, FName(bLeft ? TEXT("hand_l") : TEXT("hand_r")));
		LeftHanders += bLeft ? 1 : 0;

		USkeletalMeshComponent* Shown = Cast<USkeletalMeshComponent>(Racket->GetAttachParent());
		if (!Pawn->GetAnimation()->GetMetaHuman() || !Shown)
		{
			continue;
		}
		++MetaHumans;
		// An overhead on the forehand side rolls the racket hand: the twist bones must follow.
		FString Report;
		const TArray<FTarget> Overhead = { { 0.35, 0.25, 2.3 } };
		WorstMiss(*this, Pawn, Overhead, false, Report);
		const FReferenceSkeleton& Ref = Shown->GetSkeletalMeshAsset()->GetRefSkeleton();
		const TCHAR* Suffix = bLeft ? TEXT("_l") : TEXT("_r");
		const int32 HandIndex = Ref.FindBoneIndex(FName(FString(TEXT("hand")) + Suffix));
		const TArray<FTransform>& Local = Shown->GetBoneSpaceTransforms();
		if (HandIndex != INDEX_NONE && Local.IsValidIndex(HandIndex))
		{
			const FVector Axis = Ref.GetRefBonePose()[HandIndex].GetLocation().GetSafeNormal();
			auto Roll = [&](int32 Bone)
			{
				FQuat Swing, Twist;
				(Local[Bone].GetRotation() * Ref.GetRefBonePose()[Bone].GetRotation().Inverse()).ToSwingTwist(Axis, Swing, Twist);
				return Twist.GetTwistAngle(Axis);
			};
			const double HandRoll = Roll(HandIndex);
			for (const TCHAR* Name : { TEXT("lowerarm_twist_01"), TEXT("lowerarm_twist_02") })
			{
				const int32 Bone = Ref.FindBoneIndex(FName(FString(Name) + Suffix));
				if (Bone == INDEX_NONE || !Local.IsValidIndex(Bone))
				{
					continue;
				}
				const FVector Forearm = Ref.GetRefBonePose()[HandIndex].GetLocation();
				const double Share = FMath::Clamp(FVector::DotProduct(Ref.GetRefBonePose()[Bone].GetLocation(), Forearm) / Forearm.SizeSquared(), 0.0, 1.0);
				const double Got = Roll(Bone);
				AddInfo(FString::Printf(TEXT("Pawn %d %s%s: hand roll %.1f deg, twist %.1f deg (%.0f%% of the way to the wrist)%s"), Id, Name, Suffix,
					FMath::RadiansToDegrees(HandRoll), FMath::RadiansToDegrees(Got), Share * 100.0,
					Shown->GetPostProcessInstance() ? TEXT(", MetaHuman post-process") : TEXT("")));
				// No candy-wrap: each twist bone turns with the hand, the more the nearer the wrist, and
				// never past it. The MetaHuman's post-process correctives may weight them differently
				// from our even spread, so this checks the shape, not exact shares.
				const double Fraction = FMath::Abs(HandRoll) > FMath::DegreesToRadians(10.0) ? Got / HandRoll : Share;
				TestTrue(FString::Printf(TEXT("Pawn %d: %s%s turns with the hand (%.2f of its roll, %.2f of the way along)"), Id, Name, Suffix, Fraction, Share),
					Fraction >= Share * 0.5 && Fraction <= 1.05);
			}
		}

		// The broadcast camera sees the MetaHumans at LOD 1 to 2: the racket still meets the ball.
		AActor* MetaHuman = Pawn->GetAnimation()->GetMetaHuman();
		for (const int32 Lod : { 1, 2 })
		{
			TInlineComponentArray<USkeletalMeshComponent*> Parts(MetaHuman);
			for (USkeletalMeshComponent* Part : Parts)
			{
				Part->SetForcedLOD(Lod + 1);
			}
			Report.Reset();
			const TArray<FTarget> Band = { { 0.55, 0.35, 0.9 }, { 0.55, 0.35, 1.5 }, { -0.45, 0.35, 1.2 } };
			const double Miss = WorstMiss(*this, Pawn, Band, false, Report);
			AddInfo(FString::Printf(TEXT("Pawn %d at LOD %d:%s"), Id, Lod, *Report));
			TestTrue(FString::Printf(TEXT("Pawn %d: face within %.0f cm at LOD %d (worst %.1f)"), Id, ToleranceUnits, Lod, Miss), Miss >= 0.0 && Miss <= ToleranceUnits);
		}
		TInlineComponentArray<USkeletalMeshComponent*> Parts(MetaHuman);
		for (USkeletalMeshComponent* Part : Parts)
		{
			Part->SetForcedLOD(0);
		}
	}
	TestTrue(TEXT("The match has its left-hander (Ruben, Players.json)"), LeftHanders > 0);
	if (MetaHumans == 0)
	{
		AddInfo(TEXT("No MetaHuman assembled on this machine: twist and LOD checks skipped."));
	}
	Director->CloseMatch();
	return true;
}


// N4b: an overhead reaches long, the elbow up and out (a real overhead is about 160 to 175
// degrees at contact), and the face still meets the ball.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelOverheadArmTest, "Padel.Animation.OverheadArm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelOverheadArmTest::RunTest(const FString& Parameters)
{
	using namespace PadelContactWarpTests;
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
	const FTarget Overheads[] = { { 0.25, 0.45, 2.1 }, { 0.25, 0.45, 2.25 }, { 0.15, 0.30, 2.4 }, { 0.10, 0.30, 2.3 }, { 0.40, 0.20, 2.2 } };
	int32 Checked = 0;
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		UStaticMeshComponent* Racket = Pawn && Pawn->GetAnimation()->HasSkeletalBody() ? FindRacket(Pawn) : nullptr;
		USkeletalMeshComponent* Shown = Racket ? Cast<USkeletalMeshComponent>(Racket->GetAttachParent()) : nullptr;
		if (!Shown || !Racket->GetStaticMesh() || Racket->GetStaticMesh()->GetName() != TEXT("SM_Racket"))
		{
			continue;
		}
		++Checked;
		const TCHAR* BodyName = Pawn->GetAnimation()->GetMetaHuman() ? TEXT("MetaHuman") : TEXT("placeholder");
		for (const FTarget& T : Overheads)
		{
			const FVector Target = Around(Pawn, T.RightM, T.ForwardM, T.UpM);
			PoseAt(Pawn, Target);
			const FVector Shoulder = Shown->GetBoneLocation(Side(Pawn, TEXT("upperarm")));
			const FVector Elbow = Shown->GetBoneLocation(Side(Pawn, TEXT("lowerarm")));
			const FVector Wrist = Shown->GetBoneLocation(Side(Pawn, TEXT("hand")));
			const double Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct((Shoulder - Elbow).GetSafeNormal(), (Wrist - Elbow).GetSafeNormal())));
			const FVector Face = Racket->GetComponentTransform().TransformPosition(FVector(0.0, 0.0, FaceCentreUnits));
			const double Miss = FVector::Dist(Face, Target);
			const FString Where = FString::Printf(TEXT("Pawn %d (%s) at %.2f m up"), Id, BodyName, T.UpM);
			AddInfo(FString::Printf(TEXT("%s: elbow %.0f deg, %.1f cm above the shoulder, face %.1f cm off"), *Where, Angle, Elbow.Z - Shoulder.Z, Miss));
			TestTrue(Where + TEXT(": elbow above the shoulder"), Elbow.Z > Shoulder.Z);
			TestTrue(FString::Printf(TEXT("%s: elbow at least 150 deg (%.0f)"), *Where, Angle), Angle >= 150.0);
			TestTrue(FString::Printf(TEXT("%s: face within %.0f cm (%.1f)"), *Where, ToleranceUnits, Miss), Miss <= ToleranceUnits);
		}
	}
	if (Checked == 0)
	{
		AddInfo(TEXT("No skeletal bodies with SM_Racket on this machine; skipped."));
	}
	Director->CloseMatch();
	return true;
}

// N4b guard: through every phase of a forehand, backhand, chiquita and overhead, the racket
// arm's forearm stays outside the torso (a capsule from the pelvis up the spine).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSwingClearanceTest, "Padel.Animation.SwingClearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSwingClearanceTest::RunTest(const FString& Parameters)
{
	using namespace PadelContactWarpTests;
	using namespace Padel::Animation;
	using Padel::EShotType;
	constexpr double TorsoRadiusUnits = 12.0; // half the chest's depth
	const FPadelAnimData Data = FPadelAnimData::Load();
	if (!TestTrue(TEXT("Animation data loads"), Data.bOk))
	{
		return false;
	}
	struct FSwing { EShotType Shot; ESwingSide Side; FTarget Contact; };
	// The contact keys the model uses when the ball is at the shot's ideal point.
	const FSwing Swings[] = {
		{ EShotType::Drive, ESwingSide::Fore, { 0.6, 0.4, 1.0 } },
		{ EShotType::Drive, ESwingSide::Back, { 0.55, 0.4, 1.0 } },
		{ EShotType::Chiquita, ESwingSide::Fore, { 0.55, 0.45, 0.45 } },
		{ EShotType::Chiquita, ESwingSide::Back, { 0.5, 0.45, 0.45 } },
		{ EShotType::Smash, ESwingSide::Fore, { 0.25, 0.45, 2.45 } },
		{ EShotType::Bandeja, ESwingSide::Fore, { 0.3, 0.4, 2.2 } },
	};

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
		UStaticMeshComponent* Racket = Pawn && Pawn->GetAnimation()->HasSkeletalBody() ? FindRacket(Pawn) : nullptr;
		USkeletalMeshComponent* Shown = Racket ? Cast<USkeletalMeshComponent>(Racket->GetAttachParent()) : nullptr;
		if (!Shown)
		{
			continue;
		}
		++Checked;
		const TCHAR* BodyName = Pawn->GetAnimation()->GetMetaHuman() ? TEXT("MetaHuman") : TEXT("placeholder");
		const FName Top = TopSpine(Shown);
		const FPlayerFrameVec& Ready = Data.Settings.ReadyRacket;
		for (const FSwing& Swing : Swings)
		{
			const FAnimClip* Clip = Data.Clips.Find(Swing.Shot, Swing.Side, ESwingVariant::Perfect);
			if (!TestNotNull(TEXT("Clip"), Clip))
			{
				continue;
			}
			// The model's mirroring: the racket hand's side is +, a backhand is on the other side.
			const double RightSign = Swing.Side == ESwingSide::Fore ? 1.0 : -1.0;
			const FPlayerFrameVec Back{ Clip->Backswing.RightM * RightSign, Clip->Backswing.ForwardM, Clip->Backswing.UpM };
			const FPlayerFrameVec Contact{ Swing.Contact.RightM * RightSign, Swing.Contact.ForwardM, Swing.Contact.UpM };
			const FPlayerFrameVec Follow{ Clip->Follow.RightM * RightSign, Clip->Follow.ForwardM, Clip->Follow.UpM };
			auto LerpKey = [](const FPlayerFrameVec& A, const FPlayerFrameVec& B, double T)
			{
				return FPlayerFrameVec{ A.RightM + (B.RightM - A.RightM) * T, A.ForwardM + (B.ForwardM - A.ForwardM) * T, A.UpM + (B.UpM - A.UpM) * T };
			};
			double Closest = TNumericLimits<double>::Max();
			double ClosestP = 0.0;
			for (double P = 0.0; P <= 3.0001; P += 0.1)
			{
				const FPlayerFrameVec Key = P <= 1.0 ? LerpKey(Ready, Back, P) : (P <= 2.0 ? LerpKey(Back, Contact, P - 1.0) : LerpKey(Contact, Follow, P - 2.0));
				PoseAt(Pawn, Around(Pawn, Key.RightM, Key.ForwardM, Key.UpM));
				const FVector Mid = (Shown->GetBoneLocation(Side(Pawn, TEXT("lowerarm"))) + Shown->GetBoneLocation(Side(Pawn, TEXT("hand")))) * 0.5;
				const FVector Bottom = Shown->GetBoneLocation(FName(TEXT("pelvis")));
				const FVector TopAt = Shown->GetBoneLocation(Top);
				const double Distance = FMath::PointDistToSegment(Mid, Bottom, TopAt);
				if (Distance < Closest)
				{
					Closest = Distance;
					ClosestP = P;
				}
			}
			const FString Name = UTF8_TO_TCHAR(Clip->Name.c_str());
			AddInfo(FString::Printf(TEXT("Pawn %d (%s) %s: forearm %.1f cm from the spine at its closest (progress %.1f)"), Id, BodyName, *Name, Closest, ClosestP));
			TestTrue(FString::Printf(TEXT("Pawn %d (%s) %s: forearm clear of the torso (%.1f cm at progress %.1f)"), Id, BodyName, *Name, Closest, ClosestP),
				Closest > TorsoRadiusUnits);
		}
	}
	if (Checked == 0)
	{
		AddInfo(TEXT("No skeletal bodies on this machine; skipped."));
	}
	Director->CloseMatch();
	return true;
}

#endif
